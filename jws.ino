/*
 * ESP32-2432S024 + LVGL 9.2.0 + EEZ Studio - Islamic Prayer Clock
 * ARCHITECTURE: FreeRTOS Multi-Task Design - FULLY AUTOMATED
 * 
 * Features:
 * - Auto NTP Sync (5 server fallback)
 * - Auto Location Detection (IP-based geolocation)
 * - Auto Prayer Times Update
 * - Daily auto-update at midnight
 * - Web configuration interface
 * - AP mode for initial setup
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
bool colonOn = true;
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
const unsigned long SESSION_DURATION = 3600000; // 1 hour

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
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
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
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
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

// Generate random session token
String generateSessionToken() {
    String token = "";
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    for (int i = 0; i < 32; i++) {
        token += chars[random(0, sizeof(chars) - 1)];
    }
    
    return token;
}

// Find or create session for client
String getOrCreateSession(IPAddress clientIP) {
    unsigned long now = millis();
    
    // Check if client already has valid session
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (activeSessions[i].clientIP == clientIP && 
            activeSessions[i].expiry > now) {
            // Refresh session
            activeSessions[i].expiry = now + SESSION_DURATION;
            return activeSessions[i].token;
        }
    }
    
    // Find empty slot or oldest session
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
    
    // Create new session
    if (slot >= 0) {
        activeSessions[slot].token = generateSessionToken();
        activeSessions[slot].expiry = now + SESSION_DURATION;
        activeSessions[slot].clientIP = clientIP;
        
        Serial.printf("New session created for %s\n", clientIP.toString().c_str());
        return activeSessions[slot].token;
    }
    
    return "";
}

// Validate session token
bool validateSession(AsyncWebServerRequest *request) {
    unsigned long now = millis();
    
    // Get session token from cookie or header
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
    
    // Validate token
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (activeSessions[i].token == token && 
            activeSessions[i].expiry > now) {
            // Refresh session
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
            file.flush();
            file.close();
            Serial.println("City selection saved");
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
                prayerConfig.selectedCity.trim();
                prayerConfig.selectedCityName.trim();
                file.close();
                Serial.println("City selection loaded: " + prayerConfig.selectedCityName);
            }
        } else {
            prayerConfig.selectedCity = "";
            prayerConfig.selectedCityName = "";
            Serial.println("No city selection found");
        }
        xSemaphoreGive(settingsMutex);
        
        updateCityDisplay();
    }
}

bool initRTC() {
    Serial.println("\nInitializing DS3231 RTC...");
    
    Wire.begin(RTC_SDA, RTC_SCL);
    
    if (!rtc.begin(&Wire)) {
        Serial.println("DS3231 not found!");
        Serial.println("   Check wiring:");
        Serial.println("   - SDA GPIO21");
        Serial.println("   - SCL GPIO22");
        Serial.println("   - VCC 3.3V");
        Serial.println("   - GND GND");
        return false;
    }
    
    Serial.println("DS3231 detected!");
    
    // Cek apakah RTC kehilangan power
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, needs time sync");
        return true;  // RTC ada tapi perlu sync
    }
    
    // Baca waktu dari RTC
    DateTime now = rtc.now();
    
    // Validasi waktu RTC (harus > 2024)
    if (now.year() < 2024) {
        Serial.println("RTC time invalid, needs sync");
        return true;
    }
    
    // Set system time dari RTC
    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        setTime(now.hour(), now.minute(), now.second(), 
                now.day(), now.month(), now.year());
        timeConfig.currentTime = now.unixtime();
        
        xSemaphoreGive(timeMutex);
        
        Serial.println("Time loaded from RTC:");
        Serial.printf("   %02d:%02d:%02d %02d/%02d/%04d\n",
                     now.hour(), now.minute(), now.second(),
                     now.day(), now.month(), now.year());
        
        // PERBAIKAN: Cek apakah displayQueue sudah dibuat
        if (displayQueue != NULL) {
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
        }
    }
    
    return true;
}

// ================================
// PRAYER TIMES API - FIXED
// ================================
void getPrayerTimesByCity(String cityName) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - keeping existing prayer times");
        return;
    }

    String encodedCity = cityName;
    encodedCity.replace(" ", "+");
    
    String url = "http://api.aladhan.com/v1/timingsByCity?city=" + encodedCity + 
                 "&country=Indonesia&method=20";
    
    Serial.println("\nFetching prayer times...");
    Serial.println("   City: " + cityName);
    Serial.println("   Encoded: " + encodedCity);
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
            Serial.println("   City not found in API");
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
                
                // Update data dulu
                updateCityDisplay();
                
                if (prayerConfig.subuhTime.length() > 0) {
                    updatePrayerDisplay();
                    Serial.println("Initial prayer times displayed");
                }
                
                // **BARU TAMPILKAN UI SETELAH DATA SIAP**
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
                        Serial.println("ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ‚Â¥ WiFi disconnected to prevent overheating");
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case WIFI_CONNECTED:
                static bool autoUpdateDone = false;
                
                if (!autoUpdateDone && wifiConfig.isConnected) {
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    esp_task_wdt_reset();
                    
                    if (prayerConfig.selectedCity.length() > 0) {
                        Serial.println("Auto-updating prayer times for: " + prayerConfig.selectedCityName);
                        getPrayerTimesByCity(prayerConfig.selectedCity);
                    } else {
                        Serial.println("No city selected - please select via web interface");
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
        
        Serial.println("Auto NTP Sync...");

        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            bool syncSuccess = false;
            int serverIndex = 0;
            
            while (!syncSuccess && serverIndex < NTP_SERVER_COUNT) {
                Serial.printf("Trying: %s\n", ntpServers[serverIndex]);
                
                timeClient.setPoolServerName(ntpServers[serverIndex]);
                timeClient.setTimeOffset(25200);
                timeClient.begin();
                
                unsigned long startTime = millis();
                bool updateResult = false;
                
                while (millis() - startTime < 5000) {
                    updateResult = timeClient.forceUpdate();
                    if (updateResult) break;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                
                if (updateResult) {
                    timeConfig.currentTime = timeClient.getEpochTime();
                    setTime(timeConfig.currentTime);
                    timeConfig.ntpSynced = true;
                    syncSuccess = true;
                    timeConfig.ntpServer = String(ntpServers[serverIndex]);
                    
                    // **TAMBAHAN BARU: Save ke RTC setelah NTP sync**
                    if (rtcAvailable) {
                        saveTimeToRTC();
                        Serial.println("NTP time saved to RTC");
                    }
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                    
                    Serial.printf("NTP Success with %s!\n", ntpServers[serverIndex]);
                    break;
                }
                
                serverIndex++;
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            
            if (!syncSuccess) {
                Serial.println("All NTP servers failed!");
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
                                prayerConfig.selectedCity.length() > 0);
            
            xSemaphoreGive(timeMutex);
            
            if (shouldUpdate) {
                Serial.println("Midnight prayer times update for: " + prayerConfig.selectedCityName);
                getPrayerTimesByCity(prayerConfig.selectedCity);
                hasUpdatedToday = true;
            }
        }
    }
}

void rtcSyncTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(60000);  // Setiap 1 menit
    
    while (true) {
        if (rtcAvailable) {
            DateTime rtcTime = rtc.now();
            
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                time_t systemTime = timeConfig.currentTime;
                time_t rtcUnix = rtcTime.unixtime();
                
                // Jika selisih > 2 detik, sync system time ke RTC time
                if (abs(systemTime - rtcUnix) > 2) {
                    timeConfig.currentTime = rtcUnix;
                    setTime(rtcUnix);
                    
                    Serial.println("System time synced from RTC");
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
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
    
    while (true) {
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            timeConfig.currentTime++;
            xSemaphoreGive(timeMutex);
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
        }

        if (wifiConfig.isConnected) {
            autoSyncCounter++;
            if (autoSyncCounter >= 3600) {
                autoSyncCounter = 0;
                if (ntpTaskHandle != NULL) {
                    Serial.println("Auto NTP sync (hourly)");
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
    // Sembunyikan semua elemen UI saat boot
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
    // Tampilkan kembali semua elemen UI
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
        
        if (prayerConfig.selectedCityName.length() > 0) {
            displayText = prayerConfig.selectedCityName;
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
        
        // Format waktu dengan efek colon berkedip
        sprintf(timeStr, "%02d%c%02d", 
                hour(timeConfig.currentTime), 
                colonOn ? ':' : ' ', 
                minute(timeConfig.currentTime));
        
        // Format tanggal
        sprintf(dateStr, "%02d/%02d/%04d", 
                day(timeConfig.currentTime), 
                month(timeConfig.currentTime), 
                year(timeConfig.currentTime));
        
        // Update objek terpisah sesuai screens.h
        if (objects.time_now) {
            lv_label_set_text(objects.time_now, timeStr);
        }
        
        if (objects.date_now) {
            lv_label_set_text(objects.date_now, dateStr);
        }
        
        colonOn = !colonOn;
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
        
        // Set session cookie (HttpOnly for security)
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
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /setcity");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        if (request->hasParam("city", true)) {
            String cityName = request->getParam("city", true)->value();
            
            Serial.println("\nPOST /setcity received: " + cityName);
            
            if (cityName.length() == 0 || cityName.length() > 50) {
                request->send(400, "text/plain", "Invalid city name");
                return;
            }
            
            String displayName = cityName;
            
            if (LittleFS.exists("/cities.json")) {
                fs::File f = LittleFS.open("/cities.json", "r");
                
                if (f) {
                    bool found = false;
                    
                    while (f.available()) {
                        String line = f.readStringUntil('\n');
                        
                        if (line.indexOf("\"api\":\"" + cityName + "\"") > 0) {
                            int displayStart = line.indexOf("\"display\":\"");
                            if (displayStart > 0) {
                                displayStart += 11;
                                int displayEnd = line.indexOf("\"", displayStart);
                                
                                if (displayEnd > displayStart) {
                                    displayName = line.substring(displayStart, displayEnd);
                                    found = true;
                                    Serial.println("Found city: " + displayName);
                                    break;
                                }
                            }
                        }
                    }
                    
                    f.close();
                    
                    if (!found) {
                        Serial.println("City not found in cities.json, using API name");
                    }
                }
            }
            
            prayerConfig.selectedCity = cityName;
            prayerConfig.selectedCityName = displayName;
            saveCitySelection();
            
            Serial.println("City saved: " + displayName + " (" + cityName + ")");
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Fetching prayer times for: " + cityName);
                getPrayerTimesByCity(cityName);
            } else {
                Serial.println("WiFi not connected, prayer times will update when online");
            }
            
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Missing city parameter");
        }
    });

    server.on("/getcityinfo", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!validateSession(request)) {
            Serial.println("Unauthorized access to /getcityinfo");
            request->send(403, "text/plain", "Forbidden");
            return;
        }

        String json = "{";
        
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            bool hasSelection = (prayerConfig.selectedCity.length() > 0 && 
                                prayerConfig.selectedCityName.length() > 0);
            
            json += "\"selectedCity\":\"" + prayerConfig.selectedCityName + "\",";
            json += "\"selectedCityApi\":\"" + prayerConfig.selectedCity + "\",";
            json += "\"hasSelection\":" + String(hasSelection ? "true" : "false");
            
            xSemaphoreGive(settingsMutex);
        } else {
            json += "\"selectedCity\":\"\",";
            json += "\"selectedCityApi\":\"\",";
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
                
                // Save to RTC if available
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
        
        // Time data
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
            
            // Day of week
            const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            int dayOfWeek = weekday(timeConfig.currentTime) - 1;
            strcpy(dayStr, dayNames[dayOfWeek]);
            
            json += "\"time\":\"" + String(timeStr) + "\",";
            json += "\"date\":\"" + String(dateStr) + "\",";
            json += "\"day\":\"" + String(dayStr) + "\",";
            json += "\"timestamp\":" + String(timeConfig.currentTime) + ",";
            
            xSemaphoreGive(timeMutex);
        }
        
        // Prayer times
        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"prayerTimes\":{";
            json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
            json += "\"dzuhur\":\"" + prayerConfig.zuhurTime + "\",";
            json += "\"ashar\":\"" + prayerConfig.asarTime + "\",";
            json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
            json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
            json += "},";
            
            // Location
            json += "\"location\":{";
            json += "\"city\":\"" + prayerConfig.selectedCityName + "\",";
            json += "\"cityId\":\"" + prayerConfig.selectedCity + "\"";
            json += "},";
            
            xSemaphoreGive(settingsMutex);
        }
        
        // Device info
        json += "\"device\":{";
        json += "\"wifiConnected\":" + String((WiFi.status() == WL_CONNECTED && wifiConfig.isConnected) ? "true" : "false") + ",";
        json += "\"wifiSSID\":\"" + String(WiFi.SSID()) + "\",";
        json += "\"ip\":\"" + wifiConfig.localIP.toString() + "\",";
        json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
        json += "\"ntpSynced\":" + String(timeConfig.ntpSynced ? "true" : "false") + ",";
        json += "\"freeHeap\":" + String(ESP.getFreeHeap());
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

        Serial.println("\nFactory reset requested from web interface...");
        
        if (LittleFS.exists("/wifi_creds.txt")) {
            LittleFS.remove("/wifi_creds.txt");
            Serial.println("WiFi creds deleted");
        }
        
        if (LittleFS.exists("/prayer_times.txt")) {
            LittleFS.remove("/prayer_times.txt");
            Serial.println("Prayer times deleted");
        }
        
        if (LittleFS.exists("/ap_creds.txt")) {
            LittleFS.remove("/ap_creds.txt");
            Serial.println("AP creds deleted");
        }
        
        if (LittleFS.exists("/city_selection.txt")) {
            LittleFS.remove("/city_selection.txt");
            Serial.println("City selection deleted");
        }
        
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
            
            strcpy(wifiConfig.apSSID, "JWS ESP32");
            strcpy(wifiConfig.apPassword, "12345678");
            
            xSemaphoreGive(settingsMutex);
        }
        
        updateCityDisplay();
        
        WiFi.disconnect(true);
        
        Serial.println("All settings cleared");
        
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
// SETUP - ESP32 CORE 3.x COMPATIBLE
// ================================
void setup()
{
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
    digitalWrite(TFT_BL, LOW);  // OFF total
    Serial.println("Backlight: OFF");
    
    // ================================
    // INIT TFT
    // ================================
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    
    tft.begin();
    tft.setRotation(1);
    
    // Fill hitam BEBERAPA KALI (force)
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
    lv_timer_handler();  // Render frame pertama
    delay(100);
    lv_timer_handler();  // Render frame kedua
    delay(100);
    
    // Fill hitam sekali lagi sebelum nyalakan backlight
    tft.fillScreen(TFT_BLACK);
    
    Serial.println("UI rendered (black screen confirmed)");
    
    // ================================
    // BARU NYALAKAN BACKLIGHT DENGAN FADE-IN
    // ================================
    Serial.println("Starting backlight...");
    
    ledcAttach(TFT_BL, TFT_BL_FREQ, TFT_BL_RESOLUTION);
    ledcWrite(TFT_BL, TFT_BL_BRIGHTNESS);  // Langsung full brightness
    Serial.printf("Backlight ON: %d/255\n", TFT_BL_BRIGHTNESS);
    
    // ================================
    // WIFI AP+STA MODE - BALANCED MODE
    // ================================
    WiFi.mode(WIFI_AP_STA);
    
    // Balanced mode - prevent overheating
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
    timeConfig.currentTime = 0;
    timeConfig.ntpSynced = false;

    // ================================
    // DISPLAY LOADED CITY & PRAYER TIMES
    // ================================
    if (prayerConfig.selectedCity.length() > 0) {
        Serial.println("\nSelected City: " + prayerConfig.selectedCityName);
        Serial.println("\nðŸ•Œ Loaded Prayer Times:");
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

    // Seed random generator for session tokens
    randomSeed(analogRead(0) + millis());
    
    // Initialize sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        activeSessions[i].token = "";
        activeSessions[i].expiry = 0;
        activeSessions[i].clientIP = IPAddress(0, 0, 0, 0);
    }
    
    Serial.println("\nâœ… Boot complete - Display ready!");
}

// ================================
// LOOP
// ================================
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}