/*
 * ESP32-2432S024 + LVGL 9.2.0 + EEZ Studio - Islamic Prayer Clock
 * ARCHITECTURE: FreeRTOS Multi-Task Design - FULLY AUTOMATED
 */

#include <WiFiClientSecure.h>
#include <Wire.h>
#include <RTClib.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <LittleFS.h>
#include <FS.h>
#include "ArduinoJson.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "TimeLib.h"
#include "NTPClient.h"
#include "WiFiUdp.h"
#include "HTTPClient.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"

// EEZ generated files
#include "src/ui.h"
#include "src/screens.h"
#include "src/images.h"
#include "src/fonts.h"

// ================================
// PIN DEFINITIONS
// ================================
#define TFT_BL      27
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_MOSI  13
#define TOUCH_MISO  12
#define TOUCH_CLK   14
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240

//#define RTC_SDA    21
//#define RTC_SCL    22

// PWM Backlight Configuration
#define TFT_BL_CHANNEL    0
#define TFT_BL_FREQ       5000
#define TFT_BL_RESOLUTION 8
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)

// Touch Calibration
#define TS_MIN_X  370
#define TS_MAX_X  3700
#define TS_MIN_Y  470
#define TS_MAX_Y  3600

// ================================
// RTOS CONFIGURATION
// ================================
#define UI_TASK_STACK_SIZE      16384
#define WIFI_TASK_STACK_SIZE    8192
#define NTP_TASK_STACK_SIZE     8192
#define WEB_TASK_STACK_SIZE     16384
#define PRAYER_TASK_STACK_SIZE  8192
#define WEB_SERVER_MAX_CLIENTS 5
#define WEB_SERVER_STACK_SIZE 8192


#define UI_TASK_PRIORITY        3
#define WIFI_TASK_PRIORITY      2
#define NTP_TASK_PRIORITY       2
#define WEB_TASK_PRIORITY       1
#define PRAYER_TASK_PRIORITY    1

// Task Handles
TaskHandle_t rtcTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t ntpTaskHandle = NULL;
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t prayerTaskHandle = NULL;

// ================================
// SEMAPHORES & MUTEXES
// ================================
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t timeMutex;
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t settingsMutex;
SemaphoreHandle_t spiMutex;

// Queue for display updates
QueueHandle_t displayQueue;

// ================================
// GLOBAL OBJECTS
// ================================
static lv_display_t *display;
static lv_indev_t *indev;
static uint8_t buf[SCREEN_WIDTH * 10];

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

RTC_DS3231 rtc;
bool rtcAvailable = false;

// ================================
// NTP SERVER LIST (FALLBACK)
// ================================
const char* ntpServers[] = {
    "pool.ntp.org",
    "id.pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com",
    "time.windows.com"
};
const int NTP_SERVER_COUNT = 5;

// ================================
// CONFIGURATION STRUCTURES
// ================================
struct WiFiConfig {
    char apSSID[33];
    char apPassword[65];
    String routerSSID;
    String routerPassword;
    bool isConnected;
    IPAddress localIP;
};

struct TimeConfig {
    time_t currentTime;
    bool ntpSynced;
    unsigned long lastNTPUpdate;
    String ntpServer;
};

struct PrayerConfig {
    String subuhTime;
    String zuhurTime;
    String asarTime;
    String maghribTime;
    String isyaTime;
    String selectedCity;
    String selectedCityName;
    String latitude;
    String longitude;
};

// Global configurations
WiFiConfig wifiConfig;
TimeConfig timeConfig;
PrayerConfig prayerConfig;

// ================================
// DISPLAY UPDATE STRUCTURE
// ================================
struct DisplayUpdate {
    enum Type {
        TIME_UPDATE,
        PRAYER_UPDATE,
        STATUS_UPDATEz
    } type;
    String data;
};

// ================================
// NETWORK OBJECTS
// ================================
AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// ================================
// TOUCH VARIABLES
// ================================
bool touchPressed = false;
int16_t lastX = 0;
int16_t lastY = 0;
unsigned long lastTouchTime = 0;

// ================================
// STATE VARIABLES
// ================================
bool displayNeedsUpdate = false;

enum WiFiState {
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
};
volatile WiFiState wifiState = WIFI_IDLE;

// ================================
// AUTO SESSION MANAGEMENT
// ================================
struct Session {
    String token;
    unsigned long expiry;
    IPAddress clientIP;
    unsigned long createdAt;
};

const int MAX_SESSIONS = 5;
Session activeSessions[MAX_SESSIONS];
const unsigned long SESSION_DURATION = 900000;

// Forward Declarations
void updateTimeDisplay();
void updatePrayerDisplay();
void getPrayerTimes(String cityId);
void getPrayerTimesByCoordinates(String lat, String lon);
void saveWiFiCredentials();
void savePrayerTimes();
void setupServerRoutes();
bool getLocationFromIP();
String findClosestCity(String lat, String lon);
bool autoDetectAndUpdatePrayerTimes();

// ================================
// FLUSH CALLBACK
// ================================
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t w = area->x2 - area->x1 + 1;
        uint32_t h = area->y2 - area->y1 + 1;
        uint16_t *color_p = (uint16_t *)px_map;

        tft.startWrite();
        tft.setAddrWindow(area->x1, area->y1, w, h);
        tft.pushColors(color_p, w * h);
        tft.endWrite();

        xSemaphoreGive(spiMutex);
    }
    lv_display_flush_ready(disp);
}

// ================================
// TOUCH CALLBACK
// ================================
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
    static unsigned long lastTouchRead = 0;
    unsigned long now = millis();
    
    if (now - lastTouchRead < 50) {
        data->state = touchPressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        if (touchPressed) {
            data->point.x = lastX;
            data->point.y = lastY;
        }
        return;
    }
    
    lastTouchRead = now;
    
    bool irqActive = (digitalRead(TOUCH_IRQ) == LOW);
    if (irqActive) {
        delayMicroseconds(100);
        if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (touch.touched()) {
                TS_Point p = touch.getPoint();
                if (p.z > 200) {
                    lastX = map(p.x, TS_MIN_X, TS_MAX_X, 0, SCREEN_WIDTH);
                    lastY = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, SCREEN_HEIGHT);
                    lastX = constrain(lastX, 0, SCREEN_WIDTH - 1);
                    lastY = constrain(lastY, 0, SCREEN_HEIGHT - 1);

                    data->state = LV_INDEV_STATE_PR;
                    data->point.x = lastX;
                    data->point.y = lastY;
                    touchPressed = true;
                    xSemaphoreGive(spiMutex);
                    return;
                }
            }
            xSemaphoreGive(spiMutex);
        }
    }
    data->state = LV_INDEV_STATE_REL;
    touchPressed = false;
}

String generateSessionToken() {
    String token = "";
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    for (int i = 0; i < 32; i++) {
        token += chars[random(0, sizeof(chars) - 1)];
    }
    
    return token;
}

String getOrCreateSession(IPAddress clientIP) {
    unsigned long now = millis();
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (activeSessions[i].clientIP == clientIP && 
            activeSessions[i].expiry > now) {
            activeSessions[i].expiry = now + SESSION_DURATION;
            return activeSessions[i].token;
        }
    }
    
    int slot = -1;
    unsigned long oldestExpiry = ULONG_MAX;
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (activeSessions[i].token.length() == 0 || 
            activeSessions[i].expiry < now) {
            slot = i;
            break;
        }
        if (activeSessions[i].expiry < oldestExpiry) {
            oldestExpiry = activeSessions[i].expiry;
            slot = i;
        }
    }
    
    if (slot >= 0) {
        activeSessions[slot].token = generateSessionToken();
        activeSessions[slot].expiry = now + SESSION_DURATION;
        activeSessions[slot].clientIP = clientIP;
        
        Serial.printf("New session created for %s\n", clientIP.toString().c_str());
        return activeSessions[slot].token;
    }
    
    return "";
}

bool validateSession(AsyncWebServerRequest *request) {
    unsigned long now = millis();
    
    String token = "";
    
    if (request->hasHeader("X-Session-Token")) {
        token = request->getHeader("X-Session-Token")->value();
    } else if (request->hasHeader("Cookie")) {
        String cookies = request->getHeader("Cookie")->value();
        int pos = cookies.indexOf("session=");
        if (pos >= 0) {
            int endPos = cookies.indexOf(";", pos);
            if (endPos < 0) endPos = cookies.length();
            token = cookies.substring(pos + 8, endPos);
        }
    }
    
    if (token.length() == 0) {
        return false;
    }
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (activeSessions[i].token == token && 
            activeSessions[i].expiry > now) {
            activeSessions[i].expiry = now + SESSION_DURATION;
            return true;
        }
    }
    
    return false;
}

// ================================
// LITTLEFS FUNCTIONS
// ================================
bool init_littlefs() {
    Serial.println("Initializing LittleFS...");
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed!");
        return false;
    }
    Serial.println("LittleFS Mounted");
    return true;
}

void loadWiFiCredentials() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        if (LittleFS.exists("/wifi_creds.txt")) {
            fs::File file = LittleFS.open("/wifi_creds.txt", "r");
            if (file) {
                wifiConfig.routerSSID = file.readStringUntil('\n');
                wifiConfig.routerPassword = file.readStringUntil('\n');
                wifiConfig.routerSSID.trim();
                wifiConfig.routerPassword.trim();
                file.close();
                Serial.println("WiFi credentials loaded");
            }
        }
        if (LittleFS.exists("/ap_creds.txt")) {
            fs::File file = LittleFS.open("/ap_creds.txt", "r");
            if (file) {
                String ssid = file.readStringUntil('\n');
                String pass = file.readStringUntil('\n');
                ssid.trim();
                pass.trim();
                ssid.toCharArray(wifiConfig.apSSID, 33);
                pass.toCharArray(wifiConfig.apPassword, 65);
                file.close();
            }
        } else {
            strcpy(wifiConfig.apSSID, "JWS ESP32");
            strcpy(wifiConfig.apPassword, "12345678");
        }
        xSemaphoreGive(settingsMutex);
    }
}

void saveWiFiCredentials() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        fs::File file = LittleFS.open("/wifi_creds.txt", "w");
        if (file) {
            file.println(wifiConfig.routerSSID);
            file.println(wifiConfig.routerPassword);
            file.flush();
            file.close();
            Serial.println("WiFi credentials saved");
            
            vTaskDelay(pdMS_TO_TICKS(100));
            if (LittleFS.exists("/wifi_creds.txt")) {
                Serial.println("WiFi file verified");
            }
        } else {
            Serial.println("Failed to open wifi_creds.txt for writing");
        }
        xSemaphoreGive(settingsMutex);
    }
}

void saveAPCredentials() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        fs::File file = LittleFS.open("/ap_creds.txt", "w");
        if (file) {
            file.println(wifiConfig.apSSID);
            file.println(wifiConfig.apPassword);
            file.flush();
            file.close();
            Serial.println("AP credentials saved");
            
            vTaskDelay(pdMS_TO_TICKS(100));
            if (LittleFS.exists("/ap_creds.txt")) {
                Serial.println("AP file verified");
            }
        } else {
            Serial.println("Failed to open ap_creds.txt for writing");
        }
        xSemaphoreGive(settingsMutex);
    }
}

void saveTimeToRTC() {
    if (!rtcAvailable) return;
    
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        DateTime dt(year(timeConfig.currentTime),
                   month(timeConfig.currentTime),
                   day(timeConfig.currentTime),
                   hour(timeConfig.currentTime),
                   minute(timeConfig.currentTime),
                   second(timeConfig.currentTime));
        
        rtc.adjust(dt);
        
        Serial.println("Time saved to RTC:");
        Serial.printf("   %02d:%02d:%02d %02d/%02d/%04d\n",
                     dt.hour(), dt.minute(), dt.second(),
                     dt.day(), dt.month(), dt.year());
        
        xSemaphoreGive(timeMutex);
    }
}

void savePrayerTimes() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        fs::File file = LittleFS.open("/prayer_times.txt", "w");
        if (file) {
            file.println(prayerConfig.subuhTime);
            file.println(prayerConfig.zuhurTime);
            file.println(prayerConfig.asarTime);
            file.println(prayerConfig.maghribTime);
            file.println(prayerConfig.isyaTime);

            file.println(prayerConfig.selectedCity);
            file.println(prayerConfig.selectedCityName);
            file.flush();
            file.close();
            Serial.println("Prayer times saved");
            
            vTaskDelay(pdMS_TO_TICKS(100));
            if (LittleFS.exists("/prayer_times.txt")) {
                Serial.println("Prayer times file verified");
            }
        } else {
            Serial.println("Failed to open prayer_times.txt for writing");
        }
        xSemaphoreGive(settingsMutex);
    }
}

void loadPrayerTimes() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        if (LittleFS.exists("/prayer_times.txt")) {
            fs::File file = LittleFS.open("/prayer_times.txt", "r");
            if (file) {
                prayerConfig.subuhTime = file.readStringUntil('\n'); prayerConfig.subuhTime.trim();
                prayerConfig.zuhurTime = file.readStringUntil('\n'); prayerConfig.zuhurTime.trim();
                prayerConfig.asarTime = file.readStringUntil('\n'); prayerConfig.asarTime.trim();
                prayerConfig.maghribTime = file.readStringUntil('\n'); prayerConfig.maghribTime.trim();
                prayerConfig.isyaTime = file.readStringUntil('\n'); prayerConfig.isyaTime.trim();
                
                if (file.available()) {
                    prayerConfig.selectedCity = file.readStringUntil('\n');
                    prayerConfig.selectedCity.trim();
                }
                if (file.available()) {
                    prayerConfig.selectedCityName = file.readStringUntil('\n');
                    prayerConfig.selectedCityName.trim();
                }
                
                file.close();
                Serial.println("Prayer times loaded");
                Serial.println("  City: " + prayerConfig.selectedCity);
            }
        }
        xSemaphoreGive(settingsMutex);
    }
}

// ================================
// AUTO LOCATION DETECTION
// ================================
void saveCitySelection() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        fs::File file = LittleFS.open("/city_selection.txt", "w");
        if (file) {
            file.println(prayerConfig.selectedCity);
            file.println(prayerConfig.selectedCityName);
            file.println(prayerConfig.latitude);
            file.println(prayerConfig.longitude);
            file.flush();
            file.close();
            Serial.println("City selection saved with coordinates");
            Serial.println("   Lat: " + prayerConfig.latitude + ", Lon: " + prayerConfig.longitude);
        }
        xSemaphoreGive(settingsMutex);
    }
    
    updateCityDisplay();
}

void loadCitySelection() {
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        if (LittleFS.exists("/city_selection.txt")) {
            fs::File file = LittleFS.open("/city_selection.txt", "r");
            if (file) {
                prayerConfig.selectedCity = file.readStringUntil('\n');
                prayerConfig.selectedCityName = file.readStringUntil('\n');
                prayerConfig.latitude = file.readStringUntil('\n');
                prayerConfig.longitude = file.readStringUntil('\n');
                
                prayerConfig.selectedCity.trim();
                prayerConfig.selectedCityName.trim();
                prayerConfig.latitude.trim();
                prayerConfig.longitude.trim();
                
                file.close();
                Serial.println("City selection loaded: " + prayerConfig.selectedCity);
                Serial.println("   Lat: " + prayerConfig.latitude + ", Lon: " + prayerConfig.longitude);
            }
        } else {
            prayerConfig.selectedCity = "";
            prayerConfig.selectedCityName = "";
            prayerConfig.latitude = "";
            prayerConfig.longitude = "";
            Serial.println("No city selection found");
        }
        xSemaphoreGive(settingsMutex);
        
        updateCityDisplay();
    }
}

// ================================
// INIT RTC
// ================================
bool initRTC() {
    Serial.println("\n========================================");
    Serial.println("INITIALIZING DS3231 RTC");
    Serial.println("========================================");
    
    Wire.begin(/*RTC_SDA, RTC_SCL*/);
    
    if (!rtc.begin(/*&Wire*/)) {
        Serial.println("âœ— DS3231 not found!");
        Serial.println("   Check wiring:");
        Serial.println("   - SDA â†’ GPIO21");
        Serial.println("   - SCL â†’ GPIO22");
        Serial.println("   - VCC â†’ 3.3V");
        Serial.println("   - GND â†’ GND");
        Serial.println("   - BATTERY â†’ CR2032 (optional)");
        Serial.println("\nâš  Running without RTC");
        Serial.println("   Time will reset to 00:00:00 01/01/2000 on power loss");
        Serial.println("========================================\n");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            setTime(0, 0, 0, 1, 1, 2000);
            time_t tempTime = now();
            
            if (tempTime < 946684800) {
                timeConfig.currentTime = 946684800;
            } else {
                timeConfig.currentTime = tempTime;
            }
            
            xSemaphoreGive(timeMutex);
        }
        
        return false;
    }
    
    Serial.println("âœ“ DS3231 detected!");
    
    if (rtc.lostPower()) {
        Serial.println("âš  RTC lost power - battery may be dead or missing");
        Serial.println("   Install CR2032 battery for time persistence");
    } else {
        Serial.println("âœ“ RTC has battery backup - time will persist");
    }
    
    DateTime rtcNow = rtc.now();
    
    Serial.printf("\nRTC Current Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                 rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                 rtcNow.day(), rtcNow.month(), rtcNow.year());
    
    // ================================
    // CHECK IF RTC TIME IS VALID
    // ================================
    if (rtcNow.year() >= 2000 && rtcNow.year() <= 2100) {
        Serial.println("\nâœ“ RTC time is valid - using RTC time");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            time_t rtcUnix = rtcNow.unixtime();
            
            setTime(rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                   rtcNow.day(), rtcNow.month(), rtcNow.year());
            
            timeConfig.currentTime = rtcUnix;
            
            Serial.printf("   System time set from RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                         rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                         rtcNow.day(), rtcNow.month(), rtcNow.year());
            Serial.printf("   Timestamp: %ld\n", timeConfig.currentTime);
            
            xSemaphoreGive(timeMutex);
            
            if (displayQueue != NULL) {
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, 0);
            }
        }
    } else {
        Serial.println("\nâš  RTC time invalid (year out of range)");
        Serial.println("   Resetting to default time: 00:00:00 01/01/2000");
        
        DateTime defaultTime(2000, 1, 1, 0, 0, 0);
        rtc.adjust(defaultTime);
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            setTime(0, 0, 0, 1, 1, 2000);
            time_t tempTime = now();
            
            if (tempTime < 946684800) {
                timeConfig.currentTime = 946684800;
            } else {
                timeConfig.currentTime = tempTime;
            }
            
            Serial.println("   System time set to 00:00:00 01/01/2000");
            Serial.printf("   Timestamp: %ld\n", timeConfig.currentTime);
            Serial.println("   RTC will maintain this until NTP sync");
            
            xSemaphoreGive(timeMutex);
            
            if (displayQueue != NULL) {
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, 0);
            }
        }
    }
    
    Serial.println("========================================\n");
    return true;
}

// ================================
// PRAYER TIMES API
// ================================
void getPrayerTimesByCoordinates(String lat, String lon) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - keeping existing prayer times");
        return;
    }

    time_t now_t;
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        now_t = timeConfig.currentTime;
        xSemaphoreGive(timeMutex);
    } else {
        now_t = time(nullptr);
    }
    
    char dateStr[12];
    sprintf(dateStr, "%02d-%02d-%04d", 
            day(now_t), 
            month(now_t), 
            year(now_t));
    
    String url = "http://api.aladhan.com/v1/timings/" + String(dateStr) + 
                 "?latitude=" + lat + 
                 "&longitude=" + lon + 
                 "&method=5";
    
    Serial.println("\nFetching prayer times by coordinates...");
    Serial.println("   Date: " + String(dateStr));
    Serial.println("   Lat: " + lat + ", Lon: " + lon);
    Serial.println("   URL: " + url);
    
    HTTPClient http;
    WiFiClient client;
    
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.begin(client, url);
    http.setTimeout(15000);
    
    int httpResponseCode = http.GET();
    Serial.println("Response code: " + String(httpResponseCode));
    
    if (httpResponseCode == 200) {
        String payload = http.getString();
        
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            JsonObject timings = doc["data"]["timings"];
            
            String tempSubuh   = timings["Fajr"].as<String>().substring(0, 5);
            String tempZuhur   = timings["Dhuhr"].as<String>().substring(0, 5);
            String tempAsar    = timings["Asr"].as<String>().substring(0, 5);
            String tempMaghrib = timings["Maghrib"].as<String>().substring(0, 5);
            String tempIsya    = timings["Isha"].as<String>().substring(0, 5);
            
            bool allValid = true;
            
            if (tempSubuh.length() != 5 || tempSubuh.indexOf(':') != 2) {
                Serial.println("Invalid Subuh time format");
                allValid = false;
            }
            if (tempZuhur.length() != 5 || tempZuhur.indexOf(':') != 2) {
                Serial.println("Invalid Zuhur time format");
                allValid = false;
            }
            if (tempAsar.length() != 5 || tempAsar.indexOf(':') != 2) {
                Serial.println("Invalid Asar time format");
                allValid = false;
            }
            if (tempMaghrib.length() != 5 || tempMaghrib.indexOf(':') != 2) {
                Serial.println("Invalid Maghrib time format");
                allValid = false;
            }
            if (tempIsya.length() != 5 || tempIsya.indexOf(':') != 2) {
                Serial.println("Invalid Isya time format");
                allValid = false;
            }
            
            if (allValid) {
                prayerConfig.subuhTime = tempSubuh;
                prayerConfig.zuhurTime = tempZuhur;
                prayerConfig.asarTime = tempAsar;
                prayerConfig.maghribTime = tempMaghrib;
                prayerConfig.isyaTime = tempIsya;
                
                Serial.println("\nPrayer times updated successfully:");
                Serial.println("   Subuh: " + prayerConfig.subuhTime);
                Serial.println("   Dzuhur: " + prayerConfig.zuhurTime);
                Serial.println("   Ashar: " + prayerConfig.asarTime);
                Serial.println("   Maghrib: " + prayerConfig.maghribTime);
                Serial.println("   Isya: " + prayerConfig.isyaTime);
                
                savePrayerTimes();
                
                DisplayUpdate update;
                update.type = DisplayUpdate::PRAYER_UPDATE;
                xQueueSend(displayQueue, &update, 0);
            } else {
                Serial.println("Invalid prayer times data - keeping existing times");
            }
        } else {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
            Serial.println("Keeping existing prayer times");
        }
    } else {
        Serial.printf("HTTP request failed: %d\n", httpResponseCode);
        Serial.println("Keeping existing prayer times");
        
        if (httpResponseCode < 0) {
            Serial.println("   Network error (timeout/connection failed)");
        } else if (httpResponseCode == 404) {
            Serial.println("   API endpoint not found");
        } else if (httpResponseCode >= 500) {
            Serial.println("   Server error");
        }
    }
    
    http.end();
    client.stop();
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ================================
// TASKS
// ================================
void uiTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    
    static bool initialDisplayDone = false;
    static bool uiShown = false;
    
    while (true) {
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            lv_timer_handler();
            
            if (!initialDisplayDone && objects.subuh_time != NULL) {
                initialDisplayDone = true;
                
                updateCityDisplay();
                
                if (prayerConfig.subuhTime.length() > 0) {
                    updatePrayerDisplay();
                    Serial.println("Initial prayer times displayed");
                }
                
                if (!uiShown) {
                    showAllUIElements();
                    uiShown = true;
                    Serial.println("UI elements shown");
                }
            }
            
            xSemaphoreGive(displayMutex);
        }
        
        DisplayUpdate update;
        if (xQueueReceive(displayQueue, &update, 0) == pdTRUE) {
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                switch (update.type) {
                    case DisplayUpdate::TIME_UPDATE: 
                        updateTimeDisplay(); 
                        break;
                    case DisplayUpdate::PRAYER_UPDATE: 
                        updatePrayerDisplay(); 
                        updateCityDisplay();
                        break;
                    default: 
                        break;
                }
                xSemaphoreGive(displayMutex);
            }
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void wifiTask(void *parameter) {
    int connectAttempt = 0;
    const int MAX_CONNECT_ATTEMPTS = 30;
    
    while (true) {
        esp_task_wdt_reset();
        
        switch (wifiState) {
            case WIFI_IDLE:
                if (wifiConfig.routerSSID.length() > 0 && !wifiConfig.isConnected) {
                    if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
                        Serial.println("Connecting to WiFi: " + wifiConfig.routerSSID);
                        
                        WiFi.setTxPower(WIFI_POWER_19_5dBm);
                        
                        WiFi.begin(wifiConfig.routerSSID.c_str(), wifiConfig.routerPassword.c_str());
                        wifiState = WIFI_CONNECTING;
                        connectAttempt = 0;
                        xSemaphoreGive(wifiMutex);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(5000));
                break;
                
            case WIFI_CONNECTING:
                esp_task_wdt_reset();
                
                if (WiFi.status() == WL_CONNECTED) {
                    if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
                        wifiConfig.isConnected = true;
                        wifiConfig.localIP = WiFi.localIP();
                        wifiState = WIFI_CONNECTED;
                        Serial.println("WiFi Connected!");
                        Serial.print("   IP: ");
                        Serial.println(wifiConfig.localIP);
                        xSemaphoreGive(wifiMutex);
                        
                        if (ntpTaskHandle != NULL) {
                            Serial.println("Auto-triggering NTP sync...");
                            xTaskNotifyGive(ntpTaskHandle);
                        }
                    }
                } else {
                    connectAttempt++;
                    if (connectAttempt >= MAX_CONNECT_ATTEMPTS) {
                        wifiState = WIFI_FAILED;
                        
                        WiFi.disconnect(true);
                        WiFi.mode(WIFI_AP);
                        
                        Serial.println("WiFi connection timeout");
                        Serial.println("WiFi disconnected to prevent overheating");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case WIFI_CONNECTED:
                static bool autoUpdateDone = false;
                
                if (!autoUpdateDone && wifiConfig.isConnected) {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_task_wdt_reset();
                    
                    if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
                        Serial.println("Auto-updating prayer times for: " + prayerConfig.selectedCity);
                        getPrayerTimesByCoordinates(prayerConfig.latitude, prayerConfig.longitude);
                    } else {
                        Serial.println("No city coordinates - please select via web interface");
                    }
                    
                    autoUpdateDone = true;
                }
                
                if (WiFi.status() != WL_CONNECTED) {
                    if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
                        wifiConfig.isConnected = false;
                        wifiState = WIFI_IDLE;
                        autoUpdateDone = false;
                        
                        WiFi.mode(WIFI_AP_STA);
                        
                        Serial.println("WiFi disconnected!");
                        xSemaphoreGive(wifiMutex);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10000));
                break;
                
                case WIFI_FAILED:
                    esp_task_wdt_reset();
                    
                    Serial.println("⚠️ WiFi STA failed - AP TETAP AKTIF");
                    Serial.println("   Retry in 30 seconds...");
                    
                    WiFi.disconnect(false, false);
                    
                    for (int i = 30; i > 0; i--) {
                        if (i % 10 == 0) {
                            Serial.printf("   Retry in %d seconds... (AP: 192.168.4.1)\n", i);
                        }
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_task_wdt_reset();
                    }
                    
                    wifiState = WIFI_IDLE;
                    Serial.println("🔄 Retrying WiFi STA connection...");
                    break;
        }
    }
}

void ntpTask(void *parameter) {
    while (true) {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
        
        if (notifyValue == 0) {
            continue;
        }
        
        Serial.println("\n========================================");
        Serial.println("AUTO NTP SYNC STARTED");
        Serial.println("========================================");

        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            bool syncSuccess = false;
            int serverIndex = 0;
            
            while (!syncSuccess && serverIndex < NTP_SERVER_COUNT) {
                Serial.printf("Trying NTP server: %s\n", ntpServers[serverIndex]);
                
                timeClient.setPoolServerName(ntpServers[serverIndex]);
                timeClient.setTimeOffset(25200); // UTC+7 for Indonesia
                timeClient.begin();
                
                unsigned long startTime = millis();
                bool updateResult = false;
                
                while (millis() - startTime < 5000) {
                    updateResult = timeClient.forceUpdate();
                    if (updateResult) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                
                if (updateResult) {
                    time_t ntpTime = timeClient.getEpochTime();
                    
                    timeConfig.currentTime = ntpTime;
                    setTime(timeConfig.currentTime);
                    timeConfig.ntpSynced = true;
                    syncSuccess = true;
                    timeConfig.ntpServer = String(ntpServers[serverIndex]);
                    
                    Serial.println("âœ“ NTP Sync successful!");
                    Serial.printf("   Server: %s\n", ntpServers[serverIndex]);
                    Serial.printf("   Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 hour(ntpTime), minute(ntpTime), second(ntpTime),
                                 day(ntpTime), month(ntpTime), year(ntpTime));
                    
                    // ================================
                    // SAVE TO RTC (OVERWRITE RESET TIME)
                    // ================================
                    if (rtcAvailable) {
                        DateTime dt(year(ntpTime),
                                   month(ntpTime),
                                   day(ntpTime),
                                   hour(ntpTime),
                                   minute(ntpTime),
                                   second(ntpTime));
                        
                        rtc.adjust(dt);
                        
                        Serial.println("âœ“ NTP time saved to RTC");
                        Serial.println("   (RTC time updated from NTP)");
                    }
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                    
                    Serial.println("========================================\n");
                    break;
                }
                
                serverIndex++;
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            
            if (!syncSuccess) {
                Serial.println("âœ— All NTP servers failed!");
                Serial.println("   Keeping current time");
                Serial.println("========================================\n");
            }
            
            xSemaphoreGive(timeMutex);
        }
    }
}

void webTask(void *parameter) {
    Serial.println("\n========================================");
    Serial.println("WEB TASK STARTING");
    Serial.println("========================================");
    
    setupServerRoutes();
    server.begin();
    
    Serial.println("âœ… Web server started");
    Serial.println("   Port: 80");
    Serial.println("   Max Clients: " + String(MAX_SESSIONS));
    Serial.println("========================================\n");
    
    unsigned long lastReport = 0;
    unsigned long lastAPCheck;
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_task_wdt_reset();
        
        if (millis() - lastAPCheck > 5000) {
            lastAPCheck = millis();
            
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            
            if (mode != WIFI_MODE_APSTA) {
                Serial.println("\n🚨 CRITICAL: WiFi mode changed!");
                Serial.printf("   Current mode: %d (should be %d)\n", mode, WIFI_MODE_APSTA);
                Serial.println("   Forcing back to AP_STA...");
                
                WiFi.mode(WIFI_AP_STA);
                delay(100);
                
                // Re-start AP
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                delay(100);
                
                Serial.println("✅ AP restored: " + String(wifiConfig.apSSID));
            }
        }
        
        // Monitor setiap 30 detik
        if (millis() - lastReport > 30000) {
            Serial.println("\n=== WEB SERVER STATUS ===");
            Serial.printf("Free Heap: %d bytes (%.1f KB)\n", 
                ESP.getFreeHeap(), 
                ESP.getFreeHeap() / 1024.0);
            
            int sessionCount = 0;
            unsigned long now = millis();
            
            Serial.println("Active Sessions:");
            for (int i = 0; i < MAX_SESSIONS; i++) {
                if (::activeSessions[i].token.length() > 0 && 
                    ::activeSessions[i].expiry > now) {
                    sessionCount++;
                    unsigned long remaining = (::activeSessions[i].expiry - now) / 1000;
                    Serial.printf("   [%d] %s (expires in %lu sec)\n", 
                        i, 
                        ::activeSessions[i].clientIP.toString().c_str(),
                        remaining);
                }
            }
            
            if (sessionCount == 0) {
                Serial.println("   No active sessions");
            }
            
            Serial.printf("Total: %d/%d sessions\n", sessionCount, MAX_SESSIONS);
            Serial.println("========================\n");
            
            lastReport = millis();
        }
    }
}

void prayerTask(void *parameter) {
    static bool hasUpdatedToday = false;
    static int lastDay = -1;
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int currentHour = hour(timeConfig.currentTime);
            int currentMinute = minute(timeConfig.currentTime);
            int currentDay = day(timeConfig.currentTime);
            
            if (currentDay != lastDay) {
                hasUpdatedToday = false;
                lastDay = currentDay;
            }
            
            bool shouldUpdate = (currentHour == 0 && currentMinute < 5 &&
                                !hasUpdatedToday &&
                                wifiConfig.isConnected &&
                                prayerConfig.latitude.length() > 0 &&
                                prayerConfig.longitude.length() > 0);
            
            xSemaphoreGive(timeMutex);
            
            if (shouldUpdate) {
                Serial.println("Midnight prayer times update for: " + prayerConfig.selectedCity);
                getPrayerTimesByCoordinates(prayerConfig.latitude, prayerConfig.longitude);
                hasUpdatedToday = true;
            }
        }
    }
}

void rtcSyncTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(60000); // Every 1 minute
    
    while (true) {
        if (rtcAvailable) {
            DateTime rtcTime = rtc.now();
            
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                time_t systemTime = timeConfig.currentTime;
                time_t rtcUnix = rtcTime.unixtime();
                
                if (abs(systemTime - rtcUnix) > 2 && rtcTime.year() >= 2000) {
                    timeConfig.currentTime = rtcUnix;
                    setTime(rtcUnix);
                    
                    Serial.println("System time synced from RTC");
                    Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                 rtcTime.day(), rtcTime.month(), rtcTime.year());
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                } else if (rtcTime.year() < 2000) {
                    Serial.println("RTC time invalid (year < 2000), keeping system time");
                        DateTime resetTime(2000, 1, 1, 0, 0, 0);
                        rtc.adjust(resetTime);
                }
                
                xSemaphoreGive(timeMutex);
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
    
void clockTickTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);
    
    static int autoSyncCounter = 0; 
    static bool firstRun = true;
    
    while (true) {
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // ================================
            // CRITICAL FIX: Prevent 1970 epoch bug
            // ================================
            if (timeConfig.currentTime < 946684800) { 
                
                if (firstRun) {
                    Serial.println("\nâš  CLOCK TASK WARNING:");
                    Serial.printf("  Invalid timestamp detected: %ld\n", timeConfig.currentTime);
                    Serial.println("  This indicates time was not properly initialized");
                    Serial.println("  Forcing reset to: 01/01/2000 00:00:00");
                    firstRun = false;
                }
                
                setTime(0, 0, 0, 1, 1, 2000);
                timeConfig.currentTime = now();
                
                if (timeConfig.currentTime < 946684800) {
                    Serial.println("  TimeLib.h malfunction - using hardcoded timestamp");
                    timeConfig.currentTime = 946684800;
                }
                
                Serial.printf("  âœ“ Time corrected to: %ld\n\n", timeConfig.currentTime);
            } else {
                timeConfig.currentTime++;
            }
            
            xSemaphoreGive(timeMutex);
            
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
        }

        // ================================
        // AUTO NTP SYNC EVERY HOUR
        // ================================
        if (wifiConfig.isConnected) {
            autoSyncCounter++;
            if (autoSyncCounter >= 3600) {  // 3600 seconds = 1 hour
                autoSyncCounter = 0;
                if (ntpTaskHandle != NULL) {
                    Serial.println("\nâ° Auto NTP sync (hourly)");
                    xTaskNotifyGive(ntpTaskHandle);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ================================
// HELPER FUNCTIONS
// ================================
void hideAllUIElements() {
    if (objects.time_now) lv_obj_add_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
    if (objects.date_now) lv_obj_add_flag(objects.date_now, LV_OBJ_FLAG_HIDDEN);
    if (objects.city_time) lv_obj_add_flag(objects.city_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.subuh_time) lv_obj_add_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.zuhur_time) lv_obj_add_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.ashar_time) lv_obj_add_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.maghrib_time) lv_obj_add_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.isya_time) lv_obj_add_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
}

void showAllUIElements() {
    if (objects.time_now) lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
    if (objects.date_now) lv_obj_clear_flag(objects.date_now, LV_OBJ_FLAG_HIDDEN);
    if (objects.city_time) lv_obj_clear_flag(objects.city_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.subuh_time) lv_obj_clear_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.zuhur_time) lv_obj_clear_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.ashar_time) lv_obj_clear_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.maghrib_time) lv_obj_clear_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
    if (objects.isya_time) lv_obj_clear_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
}

void updateCityDisplay() {
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        String displayText = "--";
        
        if (prayerConfig.selectedCity.length() > 0) {
            displayText = prayerConfig.selectedCity; 
        }
        
        if (objects.city_time) {
            lv_label_set_text(objects.city_time, displayText.c_str());
        }
        
        xSemaphoreGive(settingsMutex);
    }
}

void updateTimeDisplay() {
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        char timeStr[10];
        char dateStr[15];
        
        sprintf(timeStr, "%02d:%02d",
                hour(timeConfig.currentTime), 
                minute(timeConfig.currentTime));
        
        sprintf(dateStr, "%02d/%02d/%04d", 
                day(timeConfig.currentTime), 
                month(timeConfig.currentTime), 
                year(timeConfig.currentTime));
        
        if (objects.time_now) {
            lv_label_set_text(objects.time_now, timeStr);
        }
        
        if (objects.date_now) {
            lv_label_set_text(objects.date_now, dateStr);
        }
        
        xSemaphoreGive(timeMutex);
    }
}

void updatePrayerDisplay() {
    if(objects.subuh_time) lv_label_set_text(objects.subuh_time, prayerConfig.subuhTime.c_str());
    if(objects.zuhur_time) lv_label_set_text(objects.zuhur_time, prayerConfig.zuhurTime.c_str());
    if(objects.ashar_time) lv_label_set_text(objects.ashar_time, prayerConfig.asarTime.c_str());
    if(objects.maghrib_time) lv_label_set_text(objects.maghrib_time, prayerConfig.maghribTime.c_str());
    if(objects.isya_time) lv_label_set_text(objects.isya_time, prayerConfig.isyaTime.c_str());
}

// ================================
// DELAYED RESTART HELPER
// ================================
void delayedRestart(void *parameter) {
    int delaySeconds = *((int*)parameter);
    
    Serial.printf("Restarting in %d seconds...\n", delaySeconds);
    
    for (int i = delaySeconds; i > 0; i--) {
        Serial.printf("   Restart in %d...\n", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    Serial.println("Restarting NOW!");
    ESP.restart();
    
    vTaskDelete(NULL);
}

void scheduleRestart(int delaySeconds) {
    static int delay = delaySeconds;
    xTaskCreate(
        delayedRestart,
        "RestartTask",
        2048,
        &delay,
        1,
        NULL
    );
}

// ================================
// WEB SERVER ROUTES
// ================================
void setupServerRoutes() {
    
    // ================================
    // 1. SERVE HTML - ROOT PAGE
    // ================================
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        IPAddress clientIP = request->client()->remoteIP();
        Serial.printf("[ROOT] Request from: %s\n", clientIP.toString().c_str());
        
        String sessionToken = getOrCreateSession(clientIP);
        
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, 
            "/index.html", 
            "text/html"
        );
        
        response->addHeader("Set-Cookie", 
            "session=" + sessionToken + "; Max-Age=3600; Path=/; SameSite=Strict");
        response->addHeader("Connection", "keep-alive");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Access-Control-Allow-Origin", "*");
        
        request->send(response);
        Serial.printf("[ROOT] Sent to: %s\n", clientIP.toString().c_str());
    });
    
    // ================================
    // 2. SERVE CSS
    // ================================
    server.on("/assets/css/foundation.css", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.printf("[CSS] Request from: %s\n", 
            request->client()->remoteIP().toString().c_str());
        
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, 
            "/assets/css/foundation.css", 
            "text/css"
        );
        
        response->addHeader("Connection", "keep-alive");
        response->addHeader("Cache-Control", "public, max-age=86400");
        response->addHeader("Access-Control-Allow-Origin", "*");
        
        request->send(response);
    });

    // ================================
    // 3. DEVICE STATUS
    // ================================
    server.on("/devicestatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.printf("[STATUS] From: %s\n", 
            request->client()->remoteIP().toString().c_str());
        
        if (!validateSession(request)) {
            Serial.println("[STATUS] Invalid session");
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        char timeStr[20], dateStr[20];
        sprintf(timeStr, "%02d:%02d:%02d",
                hour(timeConfig.currentTime),
                minute(timeConfig.currentTime),
                second(timeConfig.currentTime));
        
        sprintf(dateStr, "%02d/%02d/%04d",
                day(timeConfig.currentTime),
                month(timeConfig.currentTime),
                year(timeConfig.currentTime));
        
        bool isWiFiConnected = (WiFi.status() == WL_CONNECTED && 
                                wifiConfig.isConnected && 
                                wifiConfig.localIP.toString() != "0.0.0.0");
        
        String ssid = isWiFiConnected ? WiFi.SSID() : "";
        String ip = isWiFiConnected ? wifiConfig.localIP.toString() : "-";
        
        String response;
        response.reserve(256);
        response = "{";
        response += "\"connected\":" + String(isWiFiConnected ? "true" : "false") + ",";
        response += "\"ssid\":\"" + ssid + "\",";
        response += "\"ip\":\"" + ip + "\",";
        response += "\"ntpSynced\":" + String(timeConfig.ntpSynced ? "true" : "false") + ",";
        response += "\"ntpServer\":\"" + timeConfig.ntpServer + "\",";
        response += "\"currentTime\":\"" + String(timeStr) + "\",";
        response += "\"currentDate\":\"" + String(dateStr) + "\",";
        response += "\"uptime\":" + String(millis() / 1000) + ",";
        response += "\"freeHeap\":\"" + String(ESP.getFreeHeap()) + "\"";
        response += "}";
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        resp->addHeader("Connection", "keep-alive");
        resp->addHeader("Cache-Control", "no-cache");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    });

    // ================================
    // 4. GET PRAYER TIMES
    // ================================
    server.on("/getprayertimes", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            Serial.println("[PRAYER] Invalid session");
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        String json;
        json.reserve(256);
        json = "{";
        json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
        json += "\"dzuhur\":\"" + prayerConfig.zuhurTime + "\",";
        json += "\"ashar\":\"" + prayerConfig.asarTime + "\",";
        json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
        json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
        json += "}";
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Connection", "keep-alive");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    });

    // ================================
    // 5. GET CITIES LIST
    // ================================
    server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.printf("[CITIES] Request from: %s\n", 
            request->client()->remoteIP().toString().c_str());
        
        if (!validateSession(request)) {
            Serial.println("[CITIES] Invalid session");
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        if (!LittleFS.exists("/cities.json")) {
            Serial.println("[CITIES] File not found");
            request->send(404, "application/json", "[]");
            return;
        }
        
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, 
            "/cities.json", 
            "application/json",
            false
        );
        
        response->addHeader("Connection", "keep-alive");
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "public, max-age=3600");
        
        request->send(response);
        Serial.println("[CITIES] Stream started");
    });

    // ================================
    // 6. SET CITY
    // ================================
    server.on("/setcity", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.printf("\n[SETCITY] Request from: %s\n", 
            request->client()->remoteIP().toString().c_str());
        
        if (!validateSession(request)) {
            Serial.println("[SETCITY] Invalid session");
            request->send(403, "application/json", 
                "{\"error\":\"Session expired. Please refresh page.\"}");
            return;
        }

        if (!request->hasParam("city", true)) {
            Serial.println("[SETCITY] Missing city parameter");
            request->send(400, "application/json", 
                "{\"error\":\"Missing city parameter\"}");
            return;
        }

        String cityApi = request->getParam("city", true)->value();
        cityApi.trim();
        
        String cityName = request->hasParam("cityName", true) ? 
            request->getParam("cityName", true)->value() : cityApi;
        cityName.trim();
        
        String lat = request->hasParam("lat", true) ? 
            request->getParam("lat", true)->value() : "";
        lat.trim();
        
        String lon = request->hasParam("lon", true) ? 
            request->getParam("lon", true)->value() : "";
        lon.trim();
        
        Serial.printf("[SETCITY] City: %s, Lat: %s, Lon: %s\n", 
            cityApi.c_str(), lat.c_str(), lon.c_str());

        if (cityApi.length() == 0 || cityApi.length() > 100) {
            request->send(400, "application/json", "{\"error\":\"Invalid city name\"}");
            return;
        }

        if (lat.length() > 0 && lon.length() > 0) {
            float latVal = lat.toFloat();
            float lonVal = lon.toFloat();
            
            if (latVal < -90.0 || latVal > 90.0 || lonVal < -180.0 || lonVal > 180.0) {
                request->send(400, "application/json", "{\"error\":\"Invalid coordinates\"}");
                return;
            }
        }

        String response;
        response.reserve(256);
        response = "{\"success\":true,\"city\":\"" + cityName + 
                   "\",\"cityApi\":\"" + cityApi + "\",\"message\":\"Saving...\"}";
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        resp->addHeader("Connection", "close");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
        
        Serial.println("[SETCITY] Response sent, processing...");
        
        static String s_cityApi, s_cityName, s_lat, s_lon;
        s_cityApi = cityApi;
        s_cityName = cityName;
        s_lat = lat;
        s_lon = lon;
        
        xTaskCreate([](void* param) {
            vTaskDelay(pdMS_TO_TICKS(100));
            
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                prayerConfig.selectedCity = s_cityApi;
                prayerConfig.selectedCityName = s_cityName;
                prayerConfig.latitude = s_lat;
                prayerConfig.longitude = s_lon;
                xSemaphoreGive(settingsMutex);
            }
            
            saveCitySelection();
            updateCityDisplay();
            
            if (WiFi.status() == WL_CONNECTED && s_lat.length() > 0 && s_lon.length() > 0) {
                getPrayerTimesByCoordinates(s_lat, s_lon);
            }
            
            Serial.println("[SETCITY-BG] Complete\n");
            vTaskDelete(NULL);
        }, "SaveCity", 4096, NULL, 1, NULL);
    });

    // ================================
    // 7. GET CITY INFO
    // ================================
    server.on("/getcityinfo", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        String json;
        json.reserve(256);
        json = "{";
        
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"selectedCity\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"selectedCityApi\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"latitude\":\"" + prayerConfig.latitude + "\",";
            json += "\"longitude\":\"" + prayerConfig.longitude + "\",";
            json += "\"hasSelection\":" + String((prayerConfig.selectedCity.length() > 0) ? "true" : "false");
            xSemaphoreGive(settingsMutex);
        } else {
            json += "\"selectedCity\":\"\",\"selectedCityApi\":\"\",\"latitude\":\"\",\"longitude\":\"\",\"hasSelection\":false";
        }
        
        json += "}";
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Connection", "keep-alive");
        request->send(resp);
    });

    // ================================
    // 8-11. WiFi, AP, Time Sync
    // ================================
    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            wifiConfig.routerSSID = request->getParam("ssid", true)->value();
            wifiConfig.routerPassword = request->getParam("password", true)->value();
            saveWiFiCredentials();
            
            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
            resp->addHeader("Connection", "close");
            request->send(resp);
            scheduleRestart(5);
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    server.on("/setap", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String pass = request->getParam("password", true)->value();
            
            if (pass.length() > 0 && pass.length() < 8) {
                request->send(400, "text/plain", "Password minimal 8 karakter");
                return;
            }
            
            ssid.toCharArray(wifiConfig.apSSID, 33);
            pass.toCharArray(wifiConfig.apPassword, 65);
            saveAPCredentials();
            
            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
            resp->addHeader("Connection", "close");
            request->send(resp);
            scheduleRestart(5);
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    server.on("/synctime", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        if (request->hasParam("y", true) && request->hasParam("m", true) &&
            request->hasParam("d", true) && request->hasParam("h", true) &&
            request->hasParam("i", true) && request->hasParam("s", true)) {
            
            int y = request->getParam("y", true)->value().toInt();
            int m = request->getParam("m", true)->value().toInt();
            int d = request->getParam("d", true)->value().toInt();
            int h = request->getParam("h", true)->value().toInt();
            int i = request->getParam("i", true)->value().toInt();
            int s = request->getParam("s", true)->value().toInt();
            
            if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
                setTime(h, i, s, d, m, y);
                timeConfig.currentTime = now(); 
                timeConfig.ntpSynced = true;
                
                if (rtcAvailable) {
                    saveTimeToRTC();
                }
                
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, 0);
                
                xSemaphoreGive(timeMutex);
            }

            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
            resp->addHeader("Connection", "close");
            request->send(resp);
        } else {
            request->send(400, "text/plain", "Incomplete data");
        }
    });

    // ================================
    // 12. API DATA
    // ================================
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json;
        json.reserve(512);
        json = "{\"time\":\"00:00:00\",\"date\":\"01/01/2000\"}";
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Connection", "keep-alive");
        request->send(resp);
    });

    // ================================
    // 13. NOT FOUND PAGE
    // ================================
    server.on("/notfound", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html><html><body><h1>404</h1><a href='/'>Back</a></body></html>";
        request->send(404, "text/html", html);
    });

    // ================================
    // 14. FACTORY RESET
    // ================================
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        LittleFS.remove("/wifi_creds.txt");
        LittleFS.remove("/prayer_times.txt");
        LittleFS.remove("/ap_creds.txt");
        LittleFS.remove("/city_selection.txt");
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
        resp->addHeader("Connection", "close");
        request->send(resp);
        scheduleRestart(5);
    });

    // ================================
    // 15. UPLOAD CITIES.JSON - LENGKAP TANPA ERROR
    // ================================
    server.on("/uploadcities", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            if (!validateSession(request)) {
                request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
                return;
            }
            
            AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", "{\"success\":true}");
            resp->addHeader("Connection", "close");
            request->send(resp);
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static fs::File uploadFile;
            static size_t totalSize = 0;
            
            if (index == 0) {
                if (!validateSession(request)) {
                    return;
                }
                
                Serial.println("\n[UPLOAD] Starting: " + filename);
                
                if (filename != "cities.json") {
                    Serial.println("[UPLOAD] Invalid filename");
                    return;
                }
                
                if (LittleFS.exists("/cities.json")) {
                    LittleFS.remove("/cities.json");
                }
                
                uploadFile = LittleFS.open("/cities.json", "w");
                if (!uploadFile) {
                    Serial.println("[UPLOAD] Failed to open file");
                    return;
                }
                
                totalSize = 0;
                Serial.println("[UPLOAD] Writing...");
            }
            
            if (uploadFile) {
                uploadFile.write(data, len);
                totalSize += len;
            }
            
            if (final && uploadFile) {
                uploadFile.close();
                Serial.printf("[UPLOAD] Complete: %d bytes\n", totalSize);
            }
        }
    );

    // ================================
    // 16. 404 NOT FOUND HANDLER - SMART REDIRECT (ORIGINAL)
    // ================================
    server.onNotFound([](AsyncWebServerRequest *request){
        String url = request->url();
        IPAddress clientIP = request->client()->remoteIP();
        
        Serial.printf("[404] %s from %s\n", url.c_str(), clientIP.toString().c_str());
        
        // RULE 1: Static Assets - Return 404 Plain Text
        if (url.startsWith("/assets/") || 
            url.endsWith(".css") || 
            url.endsWith(".js") || 
            url.endsWith(".png") || 
            url.endsWith(".jpg") || 
            url.endsWith(".jpeg") ||
            url.endsWith(".gif") ||
            url.endsWith(".ico") ||
            url.endsWith(".svg") ||
            url.endsWith(".woff") ||
            url.endsWith(".woff2") ||
            url.endsWith(".ttf")) {
            
            Serial.println("   â†’ Static asset not found (returning 404)");
            request->send(404, "text/plain", "File not found");
            return;
        }
        
        // RULE 2: Protected API Endpoints - Check Session
        bool isProtectedEndpoint = (
            url.startsWith("/api/") ||
            url == "/devicestatus" ||
            url == "/getprayertimes" ||
            url == "/getcities" ||
            url == "/setcity" ||
            url == "/setwifi" ||
            url == "/setap" ||
            url == "/synctime" ||
            url == "/reset" ||
            url == "/getcityinfo"
        );
        
        if (isProtectedEndpoint) {
            if (!validateSession(request)) {
                Serial.println("   â†’ Protected endpoint, invalid session");
                Serial.println("   â†’ Redirecting to /notfound");
                request->redirect("/notfound");
                return;
            }
            
            Serial.println("   â†’ Protected endpoint not found (session valid)");
            request->redirect("/notfound");
            return;
        }
        
        // RULE 3: Random URLs - Direct Redirect
        Serial.println("   â†’ Invalid URL, redirecting to /notfound");
        request->redirect("/notfound");
    });
}

// ================================
// SETUP - ESP32 CORE 3.x
// ================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   ESP32 Islamic Prayer Clock");
    Serial.println("   LVGL 9.2.0 + FreeRTOS");
    Serial.println("   CONCURRENT ACCESS OPTIMIZED");
    Serial.println("   VERSION 2.1 - MULTI-CLIENT");
    Serial.println("========================================\n");

    // ================================
    // MATIKAN BACKLIGHT TOTAL DULU
    // ================================
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    Serial.println("Backlight: OFF");
    
    // ================================
    // INIT TFT
    // ================================
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    Serial.println("TFT initialized");
    
    // ================================
    // CREATE SEMAPHORES & QUEUE
    // ================================
    displayMutex = xSemaphoreCreateMutex();
    timeMutex = xSemaphoreCreateMutex();
    wifiMutex = xSemaphoreCreateMutex();
    settingsMutex = xSemaphoreCreateMutex();
    spiMutex = xSemaphoreCreateMutex();
    
    displayQueue = xQueueCreate(10, sizeof(DisplayUpdate));
    
    Serial.println("Semaphores & Queue created");
    
    // ================================
    // LITTLEFS & LOAD SETTINGS
    // ================================
    init_littlefs();
    loadWiFiCredentials();
    loadPrayerTimes();
    loadCitySelection();
    
    // ================================
    // RTC DS3231 INIT
    // ================================
    rtcAvailable = initRTC();
    if (rtcAvailable) {
        Serial.println("RTC DS3231 module ready");
    } else {
        Serial.println("Running without RTC (time will reset on power loss)");
    }
    
    // ================================
    // TOUCH INIT
    // ================================
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin(touchSPI);
    touch.setRotation(1);
    Serial.println("Touch initialized");
    
    // ================================
    // LVGL INIT
    // ================================
    lv_init();
    lv_tick_set_cb([]() { return (uint32_t)millis(); });
    
    display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, my_disp_flush);
    
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    Serial.println("LVGL initialized");
    
    // ================================
    // EEZ UI INIT
    // ================================
    ui_init();
    Serial.println("EEZ UI initialized");

    // ================================
    // SEMBUNYIKAN UI ELEMENTS DULU
    // ================================
    hideAllUIElements();
    Serial.println("UI elements hidden");
    
    // ================================
    // FORCE RENDER BLACK SCREEN
    // ================================
    delay(100);
    lv_timer_handler();
    delay(100);
    lv_timer_handler();
    delay(100);
    
    tft.fillScreen(TFT_BLACK);
    
    Serial.println("UI rendered (black screen)");
    
    // ================================
    // NYALAKAN BACKLIGHT DENGAN PWM
    // ================================
    Serial.println("Starting backlight...");
    
    ledcAttach(TFT_BL, TFT_BL_FREQ, TFT_BL_RESOLUTION);
    ledcWrite(TFT_BL, TFT_BL_BRIGHTNESS);
    Serial.printf("Backlight ON: %d/255\n", TFT_BL_BRIGHTNESS);
    
    // ================================
    // WIFI CONFIGURATION
    // ================================
    Serial.println("\n========================================");
    Serial.println("WIFI CONFIGURATION");
    Serial.println("========================================");

    WiFi.mode(WIFI_AP_STA);
    delay(100);

    WiFi.setSleep(WIFI_PS_NONE);

    esp_wifi_set_ps(WIFI_PS_NONE);

    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_max_tx_power(78);

    WiFi.setAutoReconnect(true);

    WiFi.persistent(false);

    Serial.println("✅ WiFi Mode: AP + STA");
    Serial.println("✅ WiFi Sleep: DOUBLE DISABLED");
    Serial.println("   - Arduino: WIFI_PS_NONE");
    Serial.println("   - ESP-IDF: WIFI_PS_NONE");
    Serial.println("✅ WiFi Power: Maximum (19.5dBm)");
    Serial.println("✅ Auto Reconnect: Enabled");
    Serial.println("✅ Persistent: Disabled");
    Serial.println("========================================\n");

    WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
    delay(100);

    Serial.printf("✅ AP Started: %s\n", wifiConfig.apSSID);
    Serial.printf("   Password: %s\n", wifiConfig.apPassword);
    Serial.print("   AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.printf("   AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
    
    WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
    Serial.printf("AP Started: %s\n", wifiConfig.apSSID);
    Serial.printf("   Password: %s\n", wifiConfig.apPassword);
    Serial.print("   AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // ================================
    // TIME CONFIG INIT
    // ================================
    timeConfig.ntpServer = "pool.ntp.org";
    timeConfig.ntpSynced = false;

    // ================================
    // DISPLAY LOADED CITY & PRAYER TIMES
    // ================================
    if (prayerConfig.selectedCity.length() > 0) {
        Serial.println("\nSelected City: " + prayerConfig.selectedCity);
        Serial.println("\nLoaded Prayer Times:");
        Serial.println("   City: " + prayerConfig.selectedCity);
        Serial.println("   Subuh: " + prayerConfig.subuhTime);
        Serial.println("   Dzuhur: " + prayerConfig.zuhurTime);
        Serial.println("   Ashar: " + prayerConfig.asarTime);
        Serial.println("   Maghrib: " + prayerConfig.maghribTime);
        Serial.println("   Isya: " + prayerConfig.isyaTime);

        DisplayUpdate update;
        update.type = DisplayUpdate::PRAYER_UPDATE;
        xQueueSend(displayQueue, &update, 0);
    } else {
        Serial.println("\nNo city selected");
        Serial.println("   Please select city via web interface");
    }

    // ================================
    // WATCHDOG CONFIGURATION
    // ================================
    Serial.println("\nConfiguring Watchdog...");
    
    esp_task_wdt_deinit();
    
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    
    esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
    if (wdt_err == ESP_OK) {
        Serial.println("Watchdog configured (60s timeout)");
    } else {
        Serial.printf("Watchdog init error: %s\n", esp_err_to_name(wdt_err));
    }
    
    // ================================
    // SESSION SYSTEM INIT
    // ================================
    randomSeed(analogRead(0) + millis());
    
    Serial.println("\n========================================");
    Serial.println("SESSION SYSTEM INITIALIZATION");
    Serial.println("========================================");
    Serial.println("ðŸ—‘ï¸  Clearing all sessions...");
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        activeSessions[i].token = "";
        activeSessions[i].expiry = 0;
        activeSessions[i].clientIP = IPAddress(0, 0, 0, 0);
    }
    
    Serial.println("âœ… All sessions cleared");
    Serial.printf("   Max sessions: %d\n", MAX_SESSIONS);
    Serial.printf("   Session duration: %lu minutes\n", SESSION_DURATION / 60000);
    Serial.println("========================================\n");
    
    // ================================
    // CREATE FREERTOS TASKS
    // ================================
    Serial.println("Starting FreeRTOS Tasks...");
    
    xTaskCreatePinnedToCore(
        uiTask,
        "UI",
        UI_TASK_STACK_SIZE,
        NULL,
        UI_TASK_PRIORITY,
        &uiTaskHandle,
        1  // Core 1
    );
    Serial.println("   âœ“ UI Task (Core 1)");
    
    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFi",
        WIFI_TASK_STACK_SIZE,
        NULL,
        WIFI_TASK_PRIORITY,
        &wifiTaskHandle,
        0  // Core 0
    );
    Serial.println("   âœ“ WiFi Task (Core 0)");
    
    xTaskCreatePinnedToCore(
        ntpTask,
        "NTP",
        NTP_TASK_STACK_SIZE,
        NULL,
        NTP_TASK_PRIORITY,
        &ntpTaskHandle,
        0  // Core 0
    );
    Serial.println("   âœ“ NTP Task (Core 0)");
    
    xTaskCreatePinnedToCore(
        webTask,
        "Web",
        WEB_TASK_STACK_SIZE,
        NULL,
        WEB_TASK_PRIORITY,
        &webTaskHandle,
        0  // Core 0
    );
    Serial.println("   âœ“ Web Task (Core 0)");
    
    xTaskCreatePinnedToCore(
        prayerTask,
        "Prayer",
        PRAYER_TASK_STACK_SIZE,
        NULL,
        PRAYER_TASK_PRIORITY,
        &prayerTaskHandle,
        0  // Core 0
    );
    Serial.println("   âœ“ Prayer Task (Core 0)");
    
    xTaskCreatePinnedToCore(
        clockTickTask,
        "Clock",
        4096,
        NULL,
        2,
        NULL,
        0  // Core 0
    );
    Serial.println("   âœ“ Clock Task (Core 0)");
    
    // ================================
    // RTC SYNC TASK
    // ================================
    if (rtcAvailable) {
        xTaskCreatePinnedToCore(
            rtcSyncTask,
            "RTC Sync",
            4096,
            NULL,
            1,
            &rtcTaskHandle,
            0  // Core 0
        );
        Serial.println("   âœ“ RTC Sync Task (Core 0)");
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ================================
    // REGISTER TASKS TO WATCHDOG
    // ================================
    if (wifiTaskHandle) {
        esp_task_wdt_add(wifiTaskHandle);
        Serial.println("   âœ“ WiFi Task â†’ WDT");
    }
    if (webTaskHandle) {
        esp_task_wdt_add(webTaskHandle);
        Serial.println("   âœ“ Web Task â†’ WDT");
    }
    
    Serial.println("All tasks started\n");
    
    // ================================
    // STARTUP COMPLETE
    // ================================
    Serial.println("========================================");
    Serial.println("SYSTEM READY!");
    Serial.println("========================================");
    Serial.println("âœ… Multi-client concurrent access enabled");
    Serial.println("âœ… WiFi sleep disabled for better response");
    Serial.println("âœ… Max " + String(MAX_SESSIONS) + " simultaneous connections");
    Serial.println("========================================\n");
    
    if (wifiConfig.routerSSID.length() > 0) {
        Serial.println("ðŸ“¡ WiFi configured, will auto-connect...");
        Serial.println("   SSID: " + wifiConfig.routerSSID);
    } else {
        Serial.println("ðŸ“± Connect to AP to configure:");
        Serial.println("   1. WiFi: " + String(wifiConfig.apSSID));
        Serial.println("   2. Password: " + String(wifiConfig.apPassword));
        Serial.println("   3. Browser: http://192.168.4.1");
        Serial.println("   4. Set WiFi & select city");
    }
    
    if (prayerConfig.selectedCity.length() == 0) {
        Serial.println("\nâš ï¸  REMINDER: Select city via web interface");
    }
    
    Serial.println("\nðŸŽ‰ Boot complete - Ready for connections!");
    Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}