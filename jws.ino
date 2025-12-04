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

#define RTC_SDA    21
#define RTC_SCL    22

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
                Serial.println("  City: " + prayerConfig.selectedCityName);
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
                Serial.println("City selection loaded: " + prayerConfig.selectedCityName);
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
    
    Wire.begin(RTC_SDA, RTC_SCL);
    
    if (!rtc.begin(&Wire)) {
        Serial.println("✗ DS3231 not found!");
        Serial.println("   Check wiring:");
        Serial.println("   - SDA → GPIO21");
        Serial.println("   - SCL → GPIO22");
        Serial.println("   - VCC → 3.3V");
        Serial.println("   - GND → GND");
        Serial.println("   - BATTERY → CR2032 (optional)");
        Serial.println("\n⚠ Running without RTC");
        Serial.println("   Time will reset to 00:00:00 01/01/2000 on power loss");
        Serial.println("========================================\n");
        
        // SET DEFAULT TIME 01/01/2000 00:00:00
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            // Set system time menggunakan TimeLib
            setTime(0, 0, 0, 1, 1, 2000);
            
            // Get timestamp dari TimeLib
            time_t tempTime = now();
            
            // ================================
            // CRITICAL FIX: Verify timestamp validity
            // ================================
            if (tempTime < 946684800) {
                // If TimeLib returns invalid time (before 2000)
                Serial.println("⚠ TimeLib returned invalid time");
                Serial.printf("  Got timestamp: %ld (should be >= 946684800)\n", tempTime);
                Serial.println("  Forcing timestamp to 946684800 (01/01/2000)");
                timeConfig.currentTime = 946684800;
            } else {
                timeConfig.currentTime = tempTime;
                Serial.println("✓ TimeLib set successfully");
                Serial.printf("  Timestamp: %ld\n", timeConfig.currentTime);
            }
            
            xSemaphoreGive(timeMutex);
        }
        
        return false;
    }
    
    Serial.println("✓ DS3231 detected!");
    
    if (rtc.lostPower()) {
        Serial.println("⚠ RTC lost power - battery may be dead or missing");
        Serial.println("   Install CR2032 battery for time persistence");
    } else {
        Serial.println("✓ RTC has battery backup - time will persist");
    }
    
    DateTime rtcNow = rtc.now();
    
    Serial.printf("\nRTC Current Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                 rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                 rtcNow.day(), rtcNow.month(), rtcNow.year());
    
    // ALWAYS RESET TO 01/01/2000 00:00:00 ON BOOT
    Serial.println("\n⚠ Resetting to default time: 00:00:00 01/01/2000");
    Serial.println("   (Time will be updated by NTP when WiFi connects)");
    
    DateTime defaultTime(2000, 1, 1, 0, 0, 0);
    rtc.adjust(defaultTime);
    
    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        // Set system time menggunakan TimeLib
        setTime(0, 0, 0, 1, 1, 2000);
        
        // Get timestamp dari TimeLib
        time_t tempTime = now();
        
        // ================================
        // CRITICAL FIX: Verify timestamp validity
        // ================================
        if (tempTime < 946684800) {
            Serial.println("⚠ TimeLib returned invalid time");
            Serial.printf("  Got timestamp: %ld (should be >= 946684800)\n", tempTime);
            Serial.println("  Forcing timestamp to 946684800 (01/01/2000)");
            timeConfig.currentTime = 946684800;
        } else {
            timeConfig.currentTime = tempTime;
            Serial.println("✓ System time set to 00:00:00 01/01/2000");
            Serial.printf("  Timestamp: %ld (correct!)\n", timeConfig.currentTime);
        }
        
        Serial.println("   RTC will maintain this until NTP sync");
        
        xSemaphoreGive(timeMutex);
        
        if (displayQueue != NULL) {
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
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
                 "&method=20";
    
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
                        Serial.println("🔥 WiFi disconnected to prevent overheating");
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
                        Serial.println("Auto-updating prayer times for: " + prayerConfig.selectedCityName);
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
                Serial.println("Waiting 60s before retry...");
                
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                
                vTaskDelay(pdMS_TO_TICKS(60000));
                
                WiFi.mode(WIFI_AP_STA);
                wifiState = WIFI_IDLE;
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
                    
                    Serial.println("✓ NTP Sync successful!");
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
                        
                        Serial.println("✓ NTP time saved to RTC");
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
                Serial.println("✗ All NTP servers failed!");
                Serial.println("   Keeping current time");
                Serial.println("========================================\n");
            }
            
            xSemaphoreGive(timeMutex);
        }
    }
}

void webTask(void *parameter) {
    setupServerRoutes();
    server.begin();
    Serial.println("Web server started");
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_task_wdt_reset();
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
                Serial.println("Midnight prayer times update for: " + prayerConfig.selectedCityName);
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
                // 946684800 = Unix timestamp for 01/01/2000 00:00:00 UTC
                // Any time before year 2000 is invalid
                
                if (firstRun) {
                    Serial.println("\n⚠ CLOCK TASK WARNING:");
                    Serial.printf("  Invalid timestamp detected: %ld\n", timeConfig.currentTime);
                    Serial.println("  This indicates time was not properly initialized");
                    Serial.println("  Forcing reset to: 01/01/2000 00:00:00");
                    firstRun = false;
                }
                
                // Force reset to 2000
                setTime(0, 0, 0, 1, 1, 2000);
                timeConfig.currentTime = now();
                
                // Double-check: if TimeLib still returns invalid time
                if (timeConfig.currentTime < 946684800) {
                    Serial.println("  TimeLib.h malfunction - using hardcoded timestamp");
                    timeConfig.currentTime = 946684800;
                }
                
                Serial.printf("  ✓ Time corrected to: %ld\n\n", timeConfig.currentTime);
            } else {
                // Normal operation: increment by 1 second
                timeConfig.currentTime++;
            }
            
            xSemaphoreGive(timeMutex);
            
            // Send update to display
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
                    Serial.println("\n⏰ Auto NTP sync (hourly)");
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
// WEB SERVER ROUTES - COMPLETE
// ================================
void setupServerRoutes() {
    // ================================
    // 1. SERVE HTML & CSS FILES
    // ================================
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        IPAddress clientIP = request->client()->remoteIP();
        String sessionToken = getOrCreateSession(clientIP);
        
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, "/index.html", "text/html"
        );
        
        response->addHeader("Set-Cookie", 
            "session=" + sessionToken + "; Max-Age=3600; Path=/; SameSite=Strict");
        
        request->send(response);
    });
    
    server.on("/assets/css/foundation.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/css/foundation.css", "text/css");
    });

    // ================================
    // 2. DEVICE STATUS
    // ================================
    server.on("/devicestatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /devicestatus");
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        char timeStr[20];
        sprintf(timeStr, "%02d:%02d:%02d",
                hour(timeConfig.currentTime),
                minute(timeConfig.currentTime),
                second(timeConfig.currentTime));
        
        bool isWiFiConnected = (WiFi.status() == WL_CONNECTED && 
                                wifiConfig.isConnected && 
                                wifiConfig.localIP.toString() != "0.0.0.0");
        
        String ssid = "";
        String ip = "-";
        
        if (isWiFiConnected) {
            ssid = WiFi.SSID();
            ip = wifiConfig.localIP.toString();
        }
        
        String response = "{";
        response += "\"connected\":" + String(isWiFiConnected ? "true" : "false") + ",";
        response += "\"ssid\":\"" + ssid + "\",";
        response += "\"ip\":\"" + ip + "\",";
        response += "\"ntpSynced\":" + String(timeConfig.ntpSynced ? "true" : "false") + ",";
        response += "\"ntpServer\":\"" + timeConfig.ntpServer + "\",";
        response += "\"currentTime\":\"" + String(timeStr) + "\",";
        response += "\"freeHeap\":\"" + String(ESP.getFreeHeap()) + "\"";
        response += "}";
        
        request->send(200, "application/json", response);
    });

    // ================================
    // 3. PRAYER TIMES
    // ================================
    server.on("/getprayertimes", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /getprayertimes");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        String json = "{";
        json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
        json += "\"dzuhur\":\"" + prayerConfig.zuhurTime + "\",";
        json += "\"ashar\":\"" + prayerConfig.asarTime + "\",";
        json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
        json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // ================================
    // 4. CITY SELECTION ENDPOINTS
    // ================================
    server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /getcities");
            request->send(403, "application/json", "{\"error\":\"Invalid session\"}");
            return;
        }

        Serial.println("GET /getcities");
        
        if (!LittleFS.exists("/cities.json")) {
            Serial.println("cities.json not found");
            request->send(404, "application/json", "[]");
            return;
        }
        
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, 
            "/cities.json", 
            "application/json"
        );
        
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "public, max-age=3600");
        
        response->setContentLength(LittleFS.open("/cities.json", "r").size());
        
        request->send(response);
        
        Serial.println("cities.json sent");
    });

    server.on("/setcity", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("\n========================================");
        Serial.println("POST /setcity received");
        Serial.print("Client IP: ");
        Serial.println(request->client()->remoteIP().toString());
        Serial.println("========================================");
        
        if (!validateSession(request)) {
            Serial.println("ERROR: Session validation failed");
            
            int headers = request->headers();
            Serial.printf("Request headers (%d):\n", headers);
            for(int i = 0; i < headers; i++) {
                const AsyncWebHeader* h = request->getHeader(i);
                Serial.printf("  %s: %s\n", h->name().c_str(), h->value().c_str());
            }
            
            request->send(403, "application/json", 
                "{\"error\":\"Session expired or invalid. Please refresh page.\"}");
            return;
        }
        Serial.println("✓ Session validated");

        if (!request->hasParam("city", true)) {
            Serial.println("ERROR: Missing 'city' parameter");
            
            int params = request->params();
            Serial.printf("Received parameters (%d):\n", params);
            for(int i = 0; i < params; i++) {
                const AsyncWebParameter* p = request->getParam(i);
                Serial.printf("  %s = %s\n", p->name().c_str(), p->value().c_str());
            }
            
            request->send(400, "application/json", 
                "{\"error\":\"Missing city parameter\"}");
            return;
        }

        String cityApi = request->getParam("city", true)->value();
        cityApi.trim();
        
        String cityName = "";
        if (request->hasParam("cityName", true)) {
            cityName = request->getParam("cityName", true)->value();
            cityName.trim();
        }
        
        String lat = "";
        if (request->hasParam("lat", true)) {
            lat = request->getParam("lat", true)->value();
            lat.trim();
        }
        
        String lon = "";
        if (request->hasParam("lon", true)) {
            lon = request->getParam("lon", true)->value();
            lon.trim();
        }
        
        Serial.println("Received data:");
        Serial.println("  City API: " + cityApi);
        Serial.println("  City Name: " + cityName);
        Serial.println("  Latitude: " + lat);
        Serial.println("  Longitude: " + lon);
        
        if (cityApi.length() == 0) {
            Serial.println("ERROR: Empty city API name");
            request->send(400, "application/json", 
                "{\"error\":\"City name cannot be empty\"}");
            return;
        }
        
        if (cityApi.length() > 100) {
            Serial.println("ERROR: City API name too long");
            request->send(400, "application/json", 
                "{\"error\":\"City name too long (max 100 chars)\"}");
            return;
        }

        if (lat.length() > 0 && lon.length() > 0) {
            float latVal = lat.toFloat();
            float lonVal = lon.toFloat();
            
            if (latVal < -90.0 || latVal > 90.0) {
                Serial.println("ERROR: Invalid latitude range");
                request->send(400, "application/json", 
                    "{\"error\":\"Invalid latitude value\"}");
                return;
            }
            
            if (lonVal < -180.0 || lonVal > 180.0) {
                Serial.println("ERROR: Invalid longitude range");
                request->send(400, "application/json", 
                    "{\"error\":\"Invalid longitude value\"}");
                return;
            }
        } else {
            Serial.println("WARNING: Coordinates not provided");
        }

        Serial.println("Saving to memory...");
        
        bool memorySuccess = false;
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            prayerConfig.selectedCity = cityApi;
            prayerConfig.selectedCityName = (cityName.length() > 0) ? cityName : cityApi; 
            prayerConfig.latitude = lat;
            prayerConfig.longitude = lon;
            
            xSemaphoreGive(settingsMutex);
            Serial.println("✓ Memory updated");
            Serial.println("  selectedCity (API): " + prayerConfig.selectedCity);
            Serial.println("  selectedCityName (Display): " + prayerConfig.selectedCityName);
            memorySuccess = true;
        } else {
            Serial.println("ERROR: Cannot acquire settings mutex (timeout)");
            request->send(500, "application/json", 
                "{\"error\":\"System busy, please retry in a moment\"}");
            return;
        }
        
        if (!memorySuccess) {
            Serial.println("ERROR: Memory update failed");
            request->send(500, "application/json", 
                "{\"error\":\"Failed to update memory\"}");
            return;
        }
        
        Serial.println("Writing to LittleFS...");
        
        bool fileSuccess = false;
        int retryCount = 0;
        const int maxRetries = 3;
        
        while (!fileSuccess && retryCount < maxRetries) {
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                fs::File file = LittleFS.open("/city_selection.txt", "w");
                if (file) {
                    file.println(prayerConfig.selectedCity);
                    file.println(prayerConfig.selectedCityName);
                    file.println(prayerConfig.latitude);
                    file.println(prayerConfig.longitude);
                    file.flush();
                    
                    size_t bytesWritten = file.size();
                    file.close();
                    
                    if (bytesWritten > 0) {
                        fileSuccess = true;
                        Serial.printf("✓ File saved (%d bytes)\n", bytesWritten);
                        Serial.println("  Line 1 (API): " + prayerConfig.selectedCity);
                        Serial.println("  Line 2 (Display): " + prayerConfig.selectedCityName);
                        Serial.println("  Line 3 (Lat): " + prayerConfig.latitude);
                        Serial.println("  Line 4 (Lon): " + prayerConfig.longitude);
                    } else {
                        Serial.println("WARNING: File is empty after write");
                    }
                } else {
                    Serial.printf("ERROR: Cannot open file (attempt %d/%d)\n", 
                        retryCount + 1, maxRetries);
                }
                xSemaphoreGive(settingsMutex);
            } else {
                Serial.println("ERROR: Cannot acquire mutex for file write");
            }
            
            if (!fileSuccess) {
                retryCount++;
                if (retryCount < maxRetries) {
                    Serial.printf("Retrying file write (%d/%d)...\n", retryCount + 1, maxRetries);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
        }
        
        if (!fileSuccess) {
            Serial.println("ERROR: Failed to save to file after retries");
            request->send(500, "application/json", 
                "{\"error\":\"Failed to save city selection to storage\"}");
            return;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        if (LittleFS.exists("/city_selection.txt")) {
            fs::File verifyFile = LittleFS.open("/city_selection.txt", "r");
            if (verifyFile) {
                size_t fileSize = verifyFile.size();
                
                Serial.println("File content verification:");
                String line1 = verifyFile.readStringUntil('\n'); line1.trim();
                String line2 = verifyFile.readStringUntil('\n'); line2.trim();
                String line3 = verifyFile.readStringUntil('\n'); line3.trim();
                String line4 = verifyFile.readStringUntil('\n'); line4.trim();
                
                Serial.println("  Line 1: " + line1);
                Serial.println("  Line 2: " + line2);
                Serial.println("  Line 3: " + line3);
                Serial.println("  Line 4: " + line4);
                
                verifyFile.close();
                Serial.printf("✓ File verified (size: %d bytes)\n", fileSize);
            }
        } else {
            Serial.println("WARNING: File verification failed - file not found");
        }
        
        Serial.println("Updating display...");
        updateCityDisplay();
        Serial.println("✓ Display updated");
        
        bool willFetchPrayerTimes = false;
        
        if (WiFi.status() == WL_CONNECTED) {
            if (lat.length() > 0 && lon.length() > 0) {
                Serial.println("Fetching prayer times with coordinates...");
                Serial.println("  City: " + prayerConfig.selectedCityName);
                Serial.println("  Lat: " + lat);
                Serial.println("  Lon: " + lon);
                
                getPrayerTimesByCoordinates(lat, lon);
                
                Serial.println("✓ Prayer times update initiated");
                willFetchPrayerTimes = true;
            } else {
                Serial.println("No coordinates provided - cannot fetch prayer times");
            }
        } else {
            Serial.println("WiFi not connected - prayer times will update when online");
        }
        
        Serial.println("========================================");
        Serial.println("SUCCESS: City saved successfully");
        Serial.println("  API Name: " + prayerConfig.selectedCity);
        Serial.println("  Display Name: " + prayerConfig.selectedCityName);
        if (willFetchPrayerTimes) {
            Serial.println("Prayer times will update shortly...");
        }
        Serial.println("========================================\n");
        
        String response = "{";
        response += "\"success\":true,";
        response += "\"city\":\"" + prayerConfig.selectedCityName + "\",";
        response += "\"cityApi\":\"" + prayerConfig.selectedCity + "\",";
        
        if (lat.length() > 0) {
            response += "\"lat\":\"" + lat + "\",";
        }
        
        if (lon.length() > 0) {
            response += "\"lon\":\"" + lon + "\",";
        }
        
        response += "\"prayerTimesUpdating\":" + String(willFetchPrayerTimes ? "true" : "false");
        response += "}";
        
        request->send(200, "application/json", response);
    });

    server.on("/getcityinfo", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /getcityinfo");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        String json = "{";
        
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            bool hasSelection = (prayerConfig.selectedCity.length() > 0);
            
            json += "\"selectedCity\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"selectedCityApi\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"latitude\":\"" + prayerConfig.latitude + "\",";
            json += "\"longitude\":\"" + prayerConfig.longitude + "\",";
            json += "\"hasSelection\":" + String(hasSelection ? "true" : "false");
            
            xSemaphoreGive(settingsMutex);
        } else {
            json += "\"selectedCity\":\"\",";
            json += "\"selectedCityApi\":\"\",";
            json += "\"latitude\":\"\",";
            json += "\"longitude\":\"\",";
            json += "\"hasSelection\":false";
        }
        
        json += "}";
        
        Serial.println("GET /getcityinfo: " + json);
        request->send(200, "application/json", json);
    });

    // ================================
    // 5. WIFI SETTINGS
    // ================================
    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /setwifi");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            wifiConfig.routerSSID = request->getParam("ssid", true)->value();
            wifiConfig.routerPassword = request->getParam("password", true)->value();
            
            saveWiFiCredentials();
            
            Serial.println("WiFi credentials saved successfully");
            
            request->send(200, "text/plain", "OK");
            
            scheduleRestart(5);
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    // ================================
    // 6. ACCESS POINT SETTINGS
    // ================================
    server.on("/setap", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /setap");
            request->send(403, "text/plain", "Forbidden");
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
            
            Serial.println("AP Settings berhasil disimpan");
            
            request->send(200, "text/plain", "OK");
            
            scheduleRestart(5);
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    // ================================
    // 7. TIME SYNCHRONIZATION
    // ================================
    server.on("/synctime", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /synctime");
            request->send(403, "text/plain", "Forbidden");
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
                    Serial.println("Browser time saved to RTC");
                }
                
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, 0);
                
                xSemaphoreGive(timeMutex);
                
                Serial.printf("Time synced from browser: %02d:%02d:%02d %02d/%02d/%04d\n", h, i, s, d, m, y);
            }

            request->send(200, "text/plain", "Waktu berhasil di-sync!");
        } else {
            request->send(400, "text/plain", "Data waktu tidak lengkap");
        }
    });

    // ================================
    // 8. API ENDPOINT - FULL DATA JSON
    // ================================
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        
        // ================================
        // 1. TIME & DATE
        // ================================
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            char timeStr[10], dateStr[12], dayStr[15];
            
            sprintf(timeStr, "%02d:%02d:%02d",
                    hour(timeConfig.currentTime),
                    minute(timeConfig.currentTime),
                    second(timeConfig.currentTime));
            
            sprintf(dateStr, "%02d/%02d/%04d",
                    day(timeConfig.currentTime),
                    month(timeConfig.currentTime),
                    year(timeConfig.currentTime));
            
            const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            int dayOfWeek = weekday(timeConfig.currentTime) - 1;
            strcpy(dayStr, dayNames[dayOfWeek]);
            
            json += "\"time\":\"" + String(timeStr) + "\",";
            json += "\"date\":\"" + String(dateStr) + "\",";
            json += "\"day\":\"" + String(dayStr) + "\",";
            json += "\"timestamp\":" + String(timeConfig.currentTime) + ",";
            
            xSemaphoreGive(timeMutex);
        }
        
        // ================================
        // 2. PRAYER TIMES & LOCATION
        // ================================
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"prayerTimes\":{";
            json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
            json += "\"dzuhur\":\"" + prayerConfig.zuhurTime + "\",";
            json += "\"ashar\":\"" + prayerConfig.asarTime + "\",";
            json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
            json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
            json += "},";
            
            json += "\"location\":{";
            json += "\"city\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"cityId\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"displayName\":\"" + prayerConfig.selectedCityName + "\",";
            json += "\"latitude\":\"" + prayerConfig.latitude + "\",";
            json += "\"longitude\":\"" + prayerConfig.longitude + "\"";
            json += "},";
            
            xSemaphoreGive(settingsMutex);
        }
        
        // ================================
        // 3. DEVICE STATUS
        // ================================
        json += "\"device\":{";
        json += "\"wifiConnected\":" + String((WiFi.status() == WL_CONNECTED && wifiConfig.isConnected) ? "true" : "false") + ",";
        json += "\"wifiSSID\":\"" + String(WiFi.SSID()) + "\",";
        json += "\"ip\":\"" + wifiConfig.localIP.toString() + "\",";
        json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
        json += "\"ntpSynced\":" + String(timeConfig.ntpSynced ? "true" : "false") + ",";
        json += "\"ntpServer\":\"" + timeConfig.ntpServer + "\",";
        json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "}";
        
        json += "}";
        
        Serial.println("API /api/data requested");
        
        request->send(200, "application/json", json);
    });

    // ================================
    // 9. FACTORY RESET
    // ================================
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /reset");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        Serial.println("\n========================================");
        Serial.println("FACTORY RESET STARTED");
        Serial.println("========================================");
        
        // ================================
        // 1. DELETE ALL FILES
        // ================================
        if (LittleFS.exists("/wifi_creds.txt")) {
            LittleFS.remove("/wifi_creds.txt");
            Serial.println("✓ WiFi creds deleted");
        }
        
        if (LittleFS.exists("/prayer_times.txt")) {
            LittleFS.remove("/prayer_times.txt");
            Serial.println("✓ Prayer times deleted");
        }
        
        if (LittleFS.exists("/ap_creds.txt")) {
            LittleFS.remove("/ap_creds.txt");
            Serial.println("✓ AP creds deleted");
        }
        
        if (LittleFS.exists("/city_selection.txt")) {
            LittleFS.remove("/city_selection.txt");
            Serial.println("✓ City selection deleted");
        }
        
        // ================================
        // 2. RESET TIME TO 00:00:00 01/01/2000
        // ================================
        Serial.println("\nResetting time to default...");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = now();
            timeConfig.ntpSynced = false;
            timeConfig.ntpServer = "";
            
            Serial.println("✓ System time reset to: 00:00:00 01/01/2000");
            
            // ================================
            // 3. SAVE TO RTC IF AVAILABLE
            // ================================
            if (rtcAvailable) {
                DateTime resetTime(2000, 1, 1, 0, 0, 0);
                rtc.adjust(resetTime);
                
                Serial.println("✓ RTC time reset to: 00:00:00 01/01/2000");
                Serial.println("  (RTC will keep this time until NTP sync)");
            } else {
                Serial.println("✓ System time reset (no RTC detected)");
                Serial.println("  (Time will be lost on power cycle)");
            }
            
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
            
            xSemaphoreGive(timeMutex);
        }
        
        // ================================
        // 4. CLEAR MEMORY SETTINGS
        // ================================
        if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
            wifiConfig.routerSSID = "";
            wifiConfig.routerPassword = "";
            wifiConfig.isConnected = false;
            
            prayerConfig.subuhTime = "";
            prayerConfig.zuhurTime = "";
            prayerConfig.asarTime = "";
            prayerConfig.maghribTime = "";
            prayerConfig.isyaTime = "";
            prayerConfig.selectedCity = "";
            prayerConfig.selectedCityName = "";
            prayerConfig.latitude = "";
            prayerConfig.longitude = "";
            
            strcpy(wifiConfig.apSSID, "JWS ESP32");
            strcpy(wifiConfig.apPassword, "12345678");
            
            Serial.println("✓ Memory settings cleared");
            
            xSemaphoreGive(settingsMutex);
        }
        
        // ================================
        // 5. UPDATE DISPLAY
        // ================================
        updateCityDisplay();
        
        // ================================
        // 6. DISCONNECT WIFI
        // ================================
        WiFi.disconnect(true);
        Serial.println("✓ WiFi disconnected");
        
        Serial.println("\n========================================");
        Serial.println("FACTORY RESET COMPLETE");
        Serial.println("✓ System time reset to: 00:00:00 01/01/2000");
        if (rtcAvailable) {
            Serial.println("RTC will maintain this time until NTP sync");
        }
        Serial.println("Device will restart in 5 seconds...");
        Serial.println("========================================\n");
        
        request->send(200, "text/plain", "OK");
        
        scheduleRestart(5);
    });

    // ================================
    // 10. 404 NOT FOUND HANDLER
    // ================================
    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.printf("404: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
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
    Serial.println("   MANUAL CITY SELECTION");
    Serial.println("   VERSION 2.0 - CITY SELECTOR");
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
    
    Serial.println("TFT initialized (triple black fill)");
    
    // ================================
    // CREATE SEMAPHORES & QUEUE
    // ================================
    displayMutex = xSemaphoreCreateMutex();
    timeMutex = xSemaphoreCreateMutex();
    wifiMutex = xSemaphoreCreateMutex();
    settingsMutex = xSemaphoreCreateMutex();
    spiMutex = xSemaphoreCreateMutex();
    
    displayQueue = xQueueCreate(10, sizeof(DisplayUpdate));
    
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
        Serial.println("Running without RTC (will lose time on power off)");
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
    
    Serial.println("UI rendered (black screen confirmed)");
    
    // ================================
    // BARU NYALAKAN BACKLIGHT DENGAN FADE-IN
    // ================================
    Serial.println("Starting backlight...");
    
    ledcAttach(TFT_BL, TFT_BL_FREQ, TFT_BL_RESOLUTION);
    ledcWrite(TFT_BL, TFT_BL_BRIGHTNESS);
    Serial.printf("Backlight ON: %d/255\n", TFT_BL_BRIGHTNESS);
    
    // ================================
    // WIFI AP+STA MODE - BALANCED MODE
    // ================================
    WiFi.mode(WIFI_AP_STA);
    
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    Serial.println("WiFi Balanced Mode:");
    Serial.println("   - Sleep: Enabled (Anti-Overheat)");
    Serial.println("   - Latency: ~3ms (Tidak terasa)");
    Serial.println("   - Temperature: -10Â°C cooler");
    
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
        Serial.println("\nSelected City: " + prayerConfig.selectedCityName);
        Serial.println("\nLoaded Prayer Times:");
        Serial.println("   City: " + prayerConfig.selectedCityName);
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
    Serial.println("\nStarting FreeRTOS Tasks...");
    
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
    // CREATE FREERTOS TASKS
    // ================================
    xTaskCreatePinnedToCore(
        uiTask,
        "UI",
        UI_TASK_STACK_SIZE,
        NULL,
        UI_TASK_PRIORITY,
        &uiTaskHandle,
        1  // Core 1
    );
    
    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFi",
        WIFI_TASK_STACK_SIZE,
        NULL,
        WIFI_TASK_PRIORITY,
        &wifiTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        ntpTask,
        "NTP",
        NTP_TASK_STACK_SIZE,
        NULL,
        NTP_TASK_PRIORITY,
        &ntpTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        webTask,
        "Web",
        WEB_TASK_STACK_SIZE,
        NULL,
        WEB_TASK_PRIORITY,
        &webTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        prayerTask,
        "Prayer",
        PRAYER_TASK_STACK_SIZE,
        NULL,
        PRAYER_TASK_PRIORITY,
        &prayerTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        clockTickTask,
        "Clock",
        4096,
        NULL,
        2,
        NULL,
        0  // Core 0
    );
    
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
        Serial.println("RTC Sync task started");
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // ================================
    // REGISTER TASKS TO WATCHDOG
    // ================================
    if (wifiTaskHandle) {
        esp_task_wdt_add(wifiTaskHandle);
        Serial.println("   WiFi task registered to WDT");
    }
    if (webTaskHandle) {
        esp_task_wdt_add(webTaskHandle);
        Serial.println("   Web task registered to WDT");
    }
    
    Serial.println("All tasks started");
    
    // ================================
    // STARTUP COMPLETE
    // ================================
    Serial.println("\n========================================");
    Serial.println("System Ready!");
    Serial.println("MANUAL CITY SELECTION MODE");
    Serial.println("========================================\n");
    
    if (wifiConfig.routerSSID.length() > 0) {
        Serial.println("WiFi configured, will auto-connect...");
    } else {
        Serial.println("Connect to AP and configure WiFi");
        Serial.println("   1. Connect to: " + String(wifiConfig.apSSID));
        Serial.println("   2. Open browser: http://192.168.4.1");
        Serial.println("   3. Set WiFi credentials");
        Serial.println("   4. Select your city");
        Serial.println("   5. Prayer times will auto-update!");
    }
    
    if (prayerConfig.selectedCity.length() == 0) {
        Serial.println("\nREMINDER: Please select a city via web interface");
    }

    randomSeed(analogRead(0) + millis());
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        activeSessions[i].token = "";
        activeSessions[i].expiry = 0;
        activeSessions[i].clientIP = IPAddress(0, 0, 0, 0);
    }
    
    Serial.println("\nBoot complete - Display ready!");
}

// ================================
// LOOP
// ================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}