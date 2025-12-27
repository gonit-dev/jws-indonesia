/*
 * ESP32-2432S024 + LVGL 9.2.0 + EEZ Studio - Islamic Prayer Clock
 * ARCHITECTURE: FreeRTOS Multi-Task Design - FULLY AUTOMATED
 * OPTIMIZED VERSION - Event-Driven + Built-in NTP
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
#include <time.h>
#include <sys/time.h>
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
#define TFT_BL 27
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 13
#define TOUCH_MISO 12
#define TOUCH_CLK 14
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

//#define RTC_SDA    21
//#define RTC_SCL    22

// Pin & PWM config
#define BUZZER_PIN 26
#define BUZZER_CHANNEL 1
#define BUZZER_FREQ 2000
#define BUZZER_RESOLUTION 8

// PWM Backlight Configuration
#define TFT_BL_CHANNEL 0
#define TFT_BL_FREQ 5000
#define TFT_BL_RESOLUTION 8
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)

// Touch Calibration
#define TS_MIN_X 370
#define TS_MAX_X 3700
#define TS_MIN_Y 470
#define TS_MAX_Y 3600

// ================================
// RTOS CONFIGURATION
// ================================
// Task Stack Sizes (in bytes)
#define UI_TASK_STACK_SIZE 10240       // LVGL + EEZ rendering
#define WIFI_TASK_STACK_SIZE 4608      // Event-driven
#define NTP_TASK_STACK_SIZE 5120       // Built-in NTP
#define WEB_TASK_STACK_SIZE 4096       // AsyncWebServer + file handling
#define PRAYER_TASK_STACK_SIZE 2560    // HTTP + JSON
#define RTC_TASK_STACK_SIZE 2048       // Simple I2C
#define CLOCK_TASK_STACK_SIZE 2048     // Simple time increment

// Task Priorities (0 = lowest, higher number = higher priority)
#define UI_TASK_PRIORITY 3             // Highest (display responsiveness)
#define WIFI_TASK_PRIORITY 2           // High (network stability)
#define NTP_TASK_PRIORITY 2            // High (time sync)
#define WEB_TASK_PRIORITY 1            // Low (background web server)
#define PRAYER_TASK_PRIORITY 1         // Low (daily update)
#define RTC_TASK_PRIORITY 1            // Low (backup sync)
#define CLOCK_TASK_PRIORITY 2          // High (time accuracy)

// Task Handles
TaskHandle_t rtcTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t ntpTaskHandle = NULL;
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t prayerTaskHandle = NULL;
TaskHandle_t clockTaskHandle = NULL;

// ================================
// SEMAPHORES & MUTEXES
// ================================
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t timeMutex;
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t settingsMutex;
SemaphoreHandle_t spiMutex;
SemaphoreHandle_t i2cMutex;

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
// DEFAULT AP CONFIGURATION
// ================================
#define DEFAULT_AP_SSID "JWS Indonesia"
#define DEFAULT_AP_PASSWORD "12345678"

// ================================
// WIFI EVENT GROUP - Event-Driven
// ================================
EventGroupHandle_t wifiEventGroup;

// Event bits untuk WiFi status
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1
#define WIFI_GOT_IP_BIT       BIT2

// ================================
// NTP SERVER LIST (FALLBACK)
// ================================
const char *ntpServers[] = {
  "pool.ntp.org",
  "time.google.com",
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
  
  // NEW: AP Advanced Settings
  IPAddress apIP;
  IPAddress apGateway;
  IPAddress apSubnet;
};

struct TimeConfig {
  time_t currentTime;
  bool ntpSynced;
  unsigned long lastNTPUpdate;
  String ntpServer;
};

struct PrayerConfig {
  String subuhTime;
  String terbitTime;
  String zuhurTime;
  String asharTime;
  String maghribTime;
  String isyaTime;
  String imsakTime;
  String selectedCity;
  String selectedCityName;
  String latitude;
  String longitude;
};

struct MethodConfig {
  int methodId;
  String methodName;
};

struct BuzzerConfig {
  bool imsakEnabled;
  bool subuhEnabled;
  bool terbitEnabled;
  bool zuhurEnabled;
  bool asharEnabled;
  bool maghribEnabled;
  bool isyaEnabled;
  int volume;
};


struct CountdownState {
  bool isActive;
  unsigned long startTime;
  int totalSeconds;
  String message;
  String reason; // "ap_restart", "device_restart", "factory_reset"
};

CountdownState countdownState = {
  .isActive = false,
  .startTime = 0,
  .totalSeconds = 0,
  .message = "",
  .reason = ""
};

SemaphoreHandle_t countdownMutex = NULL;

WiFiConfig wifiConfig;
TimeConfig timeConfig;
PrayerConfig prayerConfig;
MethodConfig methodConfig = { 5, "Egyptian General Authority of Survey" };
int timezoneOffset = 7;

BuzzerConfig buzzerConfig = {
  false, false, false, false, false, false, false,
  50
};

volatile bool needPrayerUpdate = false;
String pendingPrayerLat = "";
String pendingPrayerLon = "";

unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;  // Check every 5 seconds
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
unsigned long wifiDisconnectedTime = 0;  // Track when disconnect happened

bool fastReconnectMode = false;
unsigned long lastFastScan = 0;
const unsigned long FAST_SCAN_INTERVAL = 3000;  // Scan every 3 seconds

// ================================
// DISPLAY UPDATE STRUCTURE
// ================================
struct DisplayUpdate {
  enum Type {
    TIME_UPDATE,
    PRAYER_UPDATE,
    STATUS_UPDATE
  } type;
  String data;
};

// ================================
// NETWORK OBJECTS
// ================================
AsyncWebServer server(80);

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
volatile bool ntpSyncInProgress = false;
volatile bool ntpSyncCompleted = false;

// ================================
// PRAYER TIME BLINK STATE
// ================================
struct BlinkState {
  bool isBlinking;
  unsigned long blinkStartTime;
  unsigned long lastBlinkToggle;
  bool currentVisible;
  String activePrayer;
};

BlinkState blinkState = {
  .isBlinking = false,
  .blinkStartTime = 0,
  .lastBlinkToggle = 0,
  .currentVisible = true,
  .activePrayer = ""
};

const unsigned long BLINK_DURATION = 60000; // 1 menit
const unsigned long BLINK_INTERVAL = 500;   // Kedip setiap 500ms

// ================================
// WIFI RESTART PROTECTION - TAMBAHAN BARU
// ================================
static SemaphoreHandle_t wifiRestartMutex = NULL;
static bool wifiRestartInProgress = false;
static bool apRestartInProgress = false;

// Debouncing timestamps
static unsigned long lastWiFiRestartRequest = 0;
static unsigned long lastAPRestartRequest = 0;
const unsigned long RESTART_DEBOUNCE_MS = 3000; // 3 detik minimum antar restart

// ================================
// FORWARD DECLARATIONS
// ================================

// ============================================
// Display & UI Functions
// ============================================
void updateCityDisplay();
void updateTimeDisplay();
void updatePrayerDisplay();
void hideAllUIElements();
void showAllUIElements();

// ============================================
// Prayer Time Blink Functions
// ============================================
void checkPrayerTime();
void startBlinking(String prayerName);
void stopBlinking();
void handleBlinking();

// ============================================
// Prayer Times API Functions
// ============================================
void getPrayerTimesByCoordinates(String lat, String lon);
void savePrayerTimes();
void loadPrayerTimes();

// ============================================
// WiFi Functions
// ============================================
void saveWiFiCredentials();
void loadWiFiCredentials();
void saveAPCredentials();
void setupWiFiEvents();

// ============================================
// Settings & Configuration Functions
// ============================================
void saveTimezoneConfig();
void loadTimezoneConfig();
void saveBuzzerConfig();
void loadBuzzerConfig();
void saveCitySelection();
void loadCitySelection();
void saveMethodSelection();
void loadMethodSelection();

// ============================================
// RTC Functions
// ============================================
bool initRTC();
bool isRTCValid();
bool isRTCTimeValid(DateTime dt);
void saveTimeToRTC();

// ============================================
// Web Server Functions
// ============================================
void setupServerRoutes();

// ============================================
// Utility Functions
// ============================================
bool init_littlefs();
void printStackReport();

// ============================================
// LVGL Callback Functions
// ============================================
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data);

// ============================================
// FreeRTOS Task Functions
// ============================================
void uiTask(void *parameter);
void wifiTask(void *parameter);
void ntpTask(void *parameter);
void webTask(void *parameter);
void prayerTask(void *parameter);
void rtcSyncTask(void *parameter);
void clockTickTask(void *parameter);

// ============================================
// WiFi & AP Restart Tasks
// ============================================
void restartWiFiTask(void *parameter);
void restartAPTask(void *parameter);

// ============================================
// Display & UI Functions
// ============================================
void updateCityDisplay() {
  if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    String displayText = "--";

    if (prayerConfig.selectedCity.length() > 0) {
      displayText = prayerConfig.selectedCity;

      displayText.replace("Kabupaten ", "Kab ");
      displayText.replace("Kabupaten", "Kab");
      displayText.replace("District ", "Dist ");
      displayText.replace("District", "Dist");
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
    
    time_t now_t = timeConfig.currentTime;
    struct tm timeinfo;
    localtime_r(&now_t, &timeinfo);

    sprintf(timeStr, "%02d:%02d",
            timeinfo.tm_hour,
            timeinfo.tm_min);

    sprintf(dateStr, "%02d/%02d/%04d",
            timeinfo.tm_mday,
            timeinfo.tm_mon + 1,
            timeinfo.tm_year + 1900);

    if (objects.time_now) {
      lv_label_set_text(objects.time_now, timeStr);
    }

    if (objects.date_now) {
      lv_label_set_text(objects.date_now, dateStr);
    }

    xSemaphoreGive(timeMutex);
  }
}

// ============================================
// Countdown Helper Functions
// ============================================
void startCountdown(String reason, String message, int seconds) {
  if (countdownMutex == NULL) {
    countdownMutex = xSemaphoreCreateMutex();
  }
  
  if (xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    countdownState.isActive = true;
    countdownState.startTime = millis();
    countdownState.totalSeconds = seconds;
    countdownState.message = message;
    countdownState.reason = reason;
    xSemaphoreGive(countdownMutex);
    
    Serial.println("\n========================================");
    Serial.println("COUNTDOWN STARTED");
    Serial.println("========================================");
    Serial.println("Reason: " + reason);
    Serial.println("Message: " + message);
    Serial.println("Duration: " + String(seconds) + " seconds");
    Serial.println("========================================\n");
  }
}

void stopCountdown() {
  if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    countdownState.isActive = false;
    countdownState.reason = "";
    xSemaphoreGive(countdownMutex);
    
    Serial.println("Countdown stopped");
  }
}

int getRemainingSeconds() {
  if (!countdownState.isActive) return 0;
  
  unsigned long elapsed = (millis() - countdownState.startTime) / 1000;
  int remaining = countdownState.totalSeconds - elapsed;
  
  if (remaining <= 0) {
    stopCountdown();
    return 0;
  }
  
  return remaining;
}

void updatePrayerDisplay() {
  if (objects.imsak_time) lv_label_set_text(objects.imsak_time, prayerConfig.imsakTime.c_str());
  if (objects.subuh_time) lv_label_set_text(objects.subuh_time, prayerConfig.subuhTime.c_str());
  if (objects.terbit_time) lv_label_set_text(objects.terbit_time, prayerConfig.terbitTime.c_str());
  if (objects.zuhur_time) lv_label_set_text(objects.zuhur_time, prayerConfig.zuhurTime.c_str());
  if (objects.ashar_time) lv_label_set_text(objects.ashar_time, prayerConfig.asharTime.c_str());
  if (objects.maghrib_time) lv_label_set_text(objects.maghrib_time, prayerConfig.maghribTime.c_str());
  if (objects.isya_time) lv_label_set_text(objects.isya_time, prayerConfig.isyaTime.c_str());
}

void hideAllUIElements() {
  if (objects.time_now) lv_obj_add_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
  if (objects.date_now) lv_obj_add_flag(objects.date_now, LV_OBJ_FLAG_HIDDEN);
  if (objects.city_time) lv_obj_add_flag(objects.city_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.imsak_time) lv_obj_add_flag(objects.imsak_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.subuh_time) lv_obj_add_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.terbit_time) lv_obj_add_flag(objects.terbit_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.zuhur_time) lv_obj_add_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.ashar_time) lv_obj_add_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.maghrib_time) lv_obj_add_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.isya_time) lv_obj_add_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
}

void showAllUIElements() {
  if (objects.time_now) lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
  if (objects.date_now) lv_obj_clear_flag(objects.date_now, LV_OBJ_FLAG_HIDDEN);
  if (objects.city_time) lv_obj_clear_flag(objects.city_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.imsak_time) lv_obj_clear_flag(objects.imsak_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.subuh_time) lv_obj_clear_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.terbit_time) lv_obj_clear_flag(objects.terbit_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.zuhur_time) lv_obj_clear_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.ashar_time) lv_obj_clear_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.maghrib_time) lv_obj_clear_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
  if (objects.isya_time) lv_obj_clear_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
}

// ============================================
// Prayer Time Blink Functions
// ============================================
void checkPrayerTime() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    time_t now_t = timeConfig.currentTime;
    struct tm timeinfo;
    localtime_r(&now_t, &timeinfo);
    
    char currentTime[6];
    sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    
    xSemaphoreGive(timeMutex);
    
    if (timeinfo.tm_sec == 0) {
      String current = String(currentTime);
      
      if (!blinkState.isBlinking) {
        if (current == prayerConfig.imsakTime && buzzerConfig.imsakEnabled) {
          startBlinking("imsak");
        } else if (current == prayerConfig.subuhTime && buzzerConfig.subuhEnabled) {
          startBlinking("subuh");
        } else if (current == prayerConfig.terbitTime && buzzerConfig.terbitEnabled) {
          startBlinking("terbit");
        } else if (current == prayerConfig.zuhurTime && buzzerConfig.zuhurEnabled) {
          startBlinking("zuhur");
        } else if (current == prayerConfig.asharTime && buzzerConfig.asharEnabled) {
          startBlinking("ashar");
        } else if (current == prayerConfig.maghribTime && buzzerConfig.maghribEnabled) {
          startBlinking("maghrib");
        } else if (current == prayerConfig.isyaTime && buzzerConfig.isyaEnabled) {
          startBlinking("isya");
        }
      }
    }
  }
}

void startBlinking(String prayerName) {
  blinkState.isBlinking = true;
  blinkState.blinkStartTime = millis();
  blinkState.lastBlinkToggle = millis();
  blinkState.currentVisible = true;
  blinkState.activePrayer = prayerName;
  
  String upperName = prayerName;
  upperName.toUpperCase();
  
  Serial.println("\n========================================");
  Serial.print("WAKTU SHALAT MASUK: ");
  Serial.println(upperName);
  Serial.println("========================================");
  Serial.println("Memulai kedip selama 1 menit...");
  Serial.println("========================================\n");
}

void stopBlinking() {
  if (blinkState.isBlinking) {
    blinkState.isBlinking = false;
    blinkState.activePrayer = "";
    
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (objects.imsak_time) lv_obj_clear_flag(objects.imsak_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.subuh_time) lv_obj_clear_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.terbit_time) lv_obj_clear_flag(objects.terbit_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.zuhur_time) lv_obj_clear_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.ashar_time) lv_obj_clear_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.maghrib_time) lv_obj_clear_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.isya_time) lv_obj_clear_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
      xSemaphoreGive(displayMutex);
    }
    
    Serial.println("Kedip selesai - semua waktu shalat terlihat normal");
  }
}

void handleBlinking() {
  if (!blinkState.isBlinking) return;
  
  unsigned long currentMillis = millis();
  
  if (currentMillis - blinkState.blinkStartTime >= BLINK_DURATION) {
    stopBlinking();
    // Matikan buzzer
    ledcWrite(BUZZER_CHANNEL, 0);
    return;
  }
  
  if (currentMillis - blinkState.lastBlinkToggle >= BLINK_INTERVAL) {
    blinkState.lastBlinkToggle = currentMillis;
    blinkState.currentVisible = !blinkState.currentVisible;
    
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      lv_obj_t *targetLabel = NULL;
      
      if (blinkState.activePrayer == "imsak") targetLabel = objects.imsak_time;
      else if (blinkState.activePrayer == "subuh") targetLabel = objects.subuh_time;
      else if (blinkState.activePrayer == "terbit") targetLabel = objects.terbit_time;
      else if (blinkState.activePrayer == "zuhur") targetLabel = objects.zuhur_time;
      else if (blinkState.activePrayer == "ashar") targetLabel = objects.ashar_time;
      else if (blinkState.activePrayer == "maghrib") targetLabel = objects.maghrib_time;
      else if (blinkState.activePrayer == "isya") targetLabel = objects.isya_time;
      
      if (targetLabel) {
        if (blinkState.currentVisible) {
          lv_obj_clear_flag(targetLabel, LV_OBJ_FLAG_HIDDEN);
          // Buzzer ON
          int pwmValue = map(buzzerConfig.volume, 0, 100, 0, 255);
          ledcWrite(BUZZER_CHANNEL, pwmValue);
        } else {
          lv_obj_add_flag(targetLabel, LV_OBJ_FLAG_HIDDEN);
          // Buzzer OFF
          ledcWrite(BUZZER_CHANNEL, 0);
        }
      }
      
      xSemaphoreGive(displayMutex);
    }
  }
}

// ============================================
// Prayer Times API Functions
// ============================================
void getPrayerTimesByCoordinates(String lat, String lon) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - keeping existing prayer times");
    return;
  }

  // ============================================
  // VALIDASI WAKTU SEBELUM REQUEST API
  // ============================================
  time_t now_t;
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    now_t = timeConfig.currentTime;
    xSemaphoreGive(timeMutex);
  } else {
    now_t = time(nullptr);
  }

  if (now_t < 946684800) {
    Serial.println("\nPRAYER TIMES UPDATE BLOCKED");
    Serial.println("========================================");
    Serial.println("Reason: Invalid system time detected");
    Serial.printf("Current timestamp: %ld\n", now_t);
    Serial.println("This would send wrong date to API");
    Serial.println("Waiting for NTP sync to fix time...");
    Serial.println("========================================\n");
    return;
  }

  char dateStr[12];
  sprintf(dateStr, "%02d-%02d-%04d",
          day(now_t),
          month(now_t),
          year(now_t));

  int currentMethod = methodConfig.methodId;

  String url = "http://api.aladhan.com/v1/timings/" + String(dateStr) + "?latitude=" + lat + "&longitude=" + lon + "&method=" + String(currentMethod);

  Serial.println("\nFetching prayer times by coordinates...");
  Serial.println("Date: " + String(dateStr));
  Serial.println("Lat: " + lat + ", Lon: " + lon);
  Serial.println("Method: " + String(currentMethod) + " (" + methodConfig.methodName + ")");
  Serial.println("URL: " + url);

  HTTPClient http;
  WiFiClient client;

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.begin(client, url);
  http.setTimeout(15000);

  int httpResponseCode = http.GET();
  Serial.println("Response code: " + String(httpResponseCode));

  if (httpResponseCode == 200) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject timings = doc["data"]["timings"];

      String tempSubuh = timings["Fajr"].as<String>().substring(0, 5);
      String tempTerbit = timings["Sunrise"].as<String>().substring(0, 5);
      String tempZuhur = timings["Dhuhr"].as<String>().substring(0, 5);
      String tempAshar = timings["Asr"].as<String>().substring(0, 5);
      String tempMaghrib = timings["Maghrib"].as<String>().substring(0, 5);
      String tempIsya = timings["Isha"].as<String>().substring(0, 5);
      String tempImsak = timings["Imsak"].as<String>().substring(0, 5);

      bool allValid = true;

      if (tempSubuh.length() != 5 || tempSubuh.indexOf(':') != 2) {
        Serial.println("Invalid Subuh time format");
        allValid = false;
      }
      if (tempTerbit.length() != 5 || tempTerbit.indexOf(':') != 2) {
        Serial.println("Invalid Terbit time format");
        allValid = false;
      }
      if (tempZuhur.length() != 5 || tempZuhur.indexOf(':') != 2) {
        Serial.println("Invalid Zuhur time format");
        allValid = false;
      }
      if (tempAshar.length() != 5 || tempAshar.indexOf(':') != 2) {
        Serial.println("Invalid Ashar time format");
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
      if (tempImsak.length() != 5 || tempImsak.indexOf(':') != 2) {
        Serial.println("Invalid Imsak time format");
        allValid = false;
      }

      if (allValid) {
        prayerConfig.subuhTime = tempSubuh;
        prayerConfig.terbitTime = tempTerbit;
        prayerConfig.zuhurTime = tempZuhur;
        prayerConfig.asharTime = tempAshar;
        prayerConfig.maghribTime = tempMaghrib;
        prayerConfig.isyaTime = tempIsya;
        prayerConfig.imsakTime = tempImsak;

        Serial.println("\nPrayer times updated successfully:");
        Serial.println("Method: " + methodConfig.methodName);
        Serial.println("Imsak: " + prayerConfig.imsakTime);
        Serial.println("Subuh: " + prayerConfig.subuhTime);
        Serial.println("Terbit: " + prayerConfig.terbitTime);
        Serial.println("Zuhur: " + prayerConfig.zuhurTime);
        Serial.println("Ashar: " + prayerConfig.asharTime);
        Serial.println("Maghrib: " + prayerConfig.maghribTime);
        Serial.println("Isya: " + prayerConfig.isyaTime);

        savePrayerTimes();

        DisplayUpdate update;
        update.type = DisplayUpdate::PRAYER_UPDATE;
        xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
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
      Serial.println("Network error (timeout/connection failed)");
    } else if (httpResponseCode == 404) {
      Serial.println("API endpoint not found");
    } else if (httpResponseCode >= 500) {
      Serial.println("Server error");
    }
  }

  http.end();
  client.stop();
  vTaskDelay(pdMS_TO_TICKS(100));
}

void savePrayerTimes() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/prayer_times.txt", "w");
    if (file) {
      file.println(prayerConfig.subuhTime);
      file.println(prayerConfig.terbitTime);
      file.println(prayerConfig.zuhurTime);
      file.println(prayerConfig.asharTime);
      file.println(prayerConfig.maghribTime);
      file.println(prayerConfig.isyaTime);
      file.println(prayerConfig.imsakTime);

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
        prayerConfig.subuhTime = file.readStringUntil('\n');
        prayerConfig.subuhTime.trim();

        prayerConfig.terbitTime = file.readStringUntil('\n');
        prayerConfig.terbitTime.trim();

        prayerConfig.zuhurTime = file.readStringUntil('\n');
        prayerConfig.zuhurTime.trim();

        prayerConfig.asharTime = file.readStringUntil('\n');
        prayerConfig.asharTime.trim();

        prayerConfig.maghribTime = file.readStringUntil('\n');
        prayerConfig.maghribTime.trim();

        prayerConfig.isyaTime = file.readStringUntil('\n');
        prayerConfig.isyaTime.trim();

        prayerConfig.imsakTime = file.readStringUntil('\n');
        prayerConfig.imsakTime.trim();

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
        Serial.println("City: " + prayerConfig.selectedCity);
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// WiFi Functions
// ============================================
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
        
        if (file.available()) {
          String ipStr = file.readStringUntil('\n');
          ipStr.trim();
          wifiConfig.apIP.fromString(ipStr);
        } else {
          wifiConfig.apIP = IPAddress(192, 168, 4, 1);
        }
        
        if (file.available()) {
          String gwStr = file.readStringUntil('\n');
          gwStr.trim();
          wifiConfig.apGateway.fromString(gwStr);
        } else {
          wifiConfig.apGateway = IPAddress(192, 168, 4, 1);
        }
        
        if (file.available()) {
          String snStr = file.readStringUntil('\n');
          snStr.trim();
          wifiConfig.apSubnet.fromString(snStr);
        } else {
          wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);
        }
        
        file.close();
        
        Serial.println("AP config loaded:");
        Serial.println("  SSID: " + String(wifiConfig.apSSID));
        Serial.println("  IP: " + wifiConfig.apIP.toString());
      }
    } else {
      strcpy(wifiConfig.apSSID, DEFAULT_AP_SSID);
      strcpy(wifiConfig.apPassword, DEFAULT_AP_PASSWORD);
      wifiConfig.apIP = IPAddress(192, 168, 4, 1);
      wifiConfig.apGateway = IPAddress(192, 168, 4, 1);
      wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);
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
      
      file.println(wifiConfig.apIP.toString());
      file.println(wifiConfig.apGateway.toString());
      file.println(wifiConfig.apSubnet.toString());
      
      file.flush();
      file.close();
      Serial.println("AP credentials saved");
      Serial.println("  SSID: " + String(wifiConfig.apSSID));
      Serial.println("  IP: " + wifiConfig.apIP.toString());
      Serial.println("  Gateway: " + wifiConfig.apGateway.toString());
      Serial.println("  Subnet: " + wifiConfig.apSubnet.toString());

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

void setupWiFiEvents() {
    wifiEventGroup = xEventGroupCreate();
    
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        // ============================================
        // SAFETY: SKIP EVENTS DURING RESTART
        // ============================================
        if (wifiRestartInProgress || apRestartInProgress) {

            return;
        }
        
        Serial.print(String("[WiFi-Event] ") + String(event));

        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.print("STA Connected to AP");
                xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
                break;

            case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
                Serial.println("\n========================================");
                Serial.println("WiFi Connected Successfully");
                Serial.println("========================================");
                Serial.println("IP: " + WiFi.localIP().toString());
                Serial.println("Gateway: " + WiFi.gatewayIP().toString());
                Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
                
                int rssi = WiFi.RSSI();
                String quality = rssi >= -50 ? "Excellent" : 
                                rssi >= -60 ? "Good" : 
                                rssi >= -70 ? "Fair" : "Weak";
                Serial.println("Signal: " + quality);
                Serial.println("========================================\n");
                
                xEventGroupSetBits(wifiEventGroup, WIFI_GOT_IP_BIT);
                
                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    wifiConfig.isConnected = true;
                    wifiConfig.localIP = WiFi.localIP();
                    wifiState = WIFI_CONNECTED;
                    reconnectAttempts = 0;
                    xSemaphoreGive(wifiMutex);
                }
                
                if (ntpTaskHandle != NULL) {
                    Serial.print("Triggering NTP sync...");
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;
                    xTaskNotifyGive(ntpTaskHandle);
                }
                break;
            }

            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
                Serial.println("\n========================================");
                Serial.println("WiFi Disconnected");
                Serial.println("========================================");
                Serial.printf("Reason Code: %d\n", info.wifi_sta_disconnected.reason);

                xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
                xEventGroupSetBits(wifiEventGroup, WIFI_DISCONNECTED_BIT);

                switch (info.wifi_sta_disconnected.reason) {
                    case WIFI_REASON_AUTH_EXPIRE:
                        Serial.println("Detail: Authentication expired");
                        break;
                    case WIFI_REASON_AUTH_LEAVE:
                        Serial.println("Detail: Deauthenticated");
                        break;
                    case WIFI_REASON_ASSOC_LEAVE:
                        Serial.println("Detail: Disassociated");
                        break;
                    case WIFI_REASON_BEACON_TIMEOUT:
                        Serial.println("Detail: Beacon timeout (router unreachable)");
                        break;
                    case WIFI_REASON_NO_AP_FOUND:
                        Serial.println("Detail: AP not found");
                        break;
                    case WIFI_REASON_HANDSHAKE_TIMEOUT:
                        Serial.println("Detail: Handshake timeout");
                        break;
                    default:
                        Serial.printf("Detail: Unknown reason (%d)\n", 
                                     info.wifi_sta_disconnected.reason);
                }

                wifiDisconnectedTime = millis();

                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    wifiConfig.isConnected = false;
                    wifiState = WIFI_IDLE;
                    xSemaphoreGive(wifiMutex);
                }
                
                Serial.println("Will attempt reconnect...");
                Serial.println("========================================\n");
                break;
            }

            case ARDUINO_EVENT_WIFI_AP_START:
                Serial.print("AP Started: " + String(WiFi.softAPSSID()));
                Serial.print("   AP IP: " + WiFi.softAPIP().toString());
                break;

            case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                Serial.print("Client connected to AP");
                Serial.print(String("   Total stations: ") + String(WiFi.softAPgetStationNum()));
                break;

            case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                Serial.print("Client disconnected from AP");
                Serial.print(String("   Total stations: ") + String(WiFi.softAPgetStationNum()));
                break;
        }
    });

    Serial.print("WiFi Event Handler registered (with restart protection)");
}

// ============================================
// Settings & Configuration Functions
// ============================================
void saveTimezoneConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/timezone.txt", "w");
    if (file) {
      file.println(timezoneOffset);
      file.flush();
      file.close();
      Serial.println("Timezone saved: UTC" + String(timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset));
    } else {
      Serial.println("Failed to save timezone");
    }
    xSemaphoreGive(settingsMutex);
  }
}

void loadTimezoneConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    if (LittleFS.exists("/timezone.txt")) {
      fs::File file = LittleFS.open("/timezone.txt", "r");
      if (file) {
        String offsetStr = file.readStringUntil('\n');
        offsetStr.trim();
        timezoneOffset = offsetStr.toInt();

        if (timezoneOffset < -12 || timezoneOffset > 14) {
          Serial.println("Invalid timezone offset in file, using default +7");
          timezoneOffset = 7;
        }

        file.close();
        Serial.println("Timezone loaded: UTC" + String(timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset));
      }
    } else {
      timezoneOffset = 7;
      Serial.println("No timezone config found - using default (UTC+7)");
    }
    xSemaphoreGive(settingsMutex);
  }
}

void loadBuzzerConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    if (LittleFS.exists("/buzzer_config.txt")) {
      fs::File file = LittleFS.open("/buzzer_config.txt", "r");
      if (file) {
        buzzerConfig.imsakEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.subuhEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.terbitEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.zuhurEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.asharEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.maghribEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.isyaEnabled = file.readStringUntil('\n').toInt() == 1;
        buzzerConfig.volume = file.readStringUntil('\n').toInt();
        file.close();
        Serial.println("Buzzer config loaded");
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

void saveBuzzerConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/buzzer_config.txt", "w");
    if (file) {
      file.println(buzzerConfig.imsakEnabled ? "1" : "0");
      file.println(buzzerConfig.subuhEnabled ? "1" : "0");
      file.println(buzzerConfig.terbitEnabled ? "1" : "0");
      file.println(buzzerConfig.zuhurEnabled ? "1" : "0");
      file.println(buzzerConfig.asharEnabled ? "1" : "0");
      file.println(buzzerConfig.maghribEnabled ? "1" : "0");
      file.println(buzzerConfig.isyaEnabled ? "1" : "0");
      file.println(buzzerConfig.volume);
      file.flush();
      file.close();
      Serial.println("Buzzer config saved");
    }
    xSemaphoreGive(settingsMutex);
  }
}

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
      Serial.println("Lat: " + prayerConfig.latitude + ", Lon: " + prayerConfig.longitude);
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
        Serial.println("Lat: " + prayerConfig.latitude + ", Lon: " + prayerConfig.longitude);
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

void saveMethodSelection() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/method_selection.txt", "w");
    if (file) {
      file.println(methodConfig.methodId);
      file.println(methodConfig.methodName);
      file.flush();
      file.close();
      Serial.println("Method selection saved:");
      Serial.println("ID: " + String(methodConfig.methodId));
      Serial.println("Name: " + methodConfig.methodName);
    } else {
      Serial.println("Failed to save method selection");
    }
    xSemaphoreGive(settingsMutex);
  }
}

void loadMethodSelection() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    if (LittleFS.exists("/method_selection.txt")) {
      fs::File file = LittleFS.open("/method_selection.txt", "r");
      if (file) {
        String idStr = file.readStringUntil('\n');
        idStr.trim();
        methodConfig.methodId = idStr.toInt();

        if (file.available()) {
          methodConfig.methodName = file.readStringUntil('\n');
          methodConfig.methodName.trim();
        }

        file.close();
        Serial.println("Method selection loaded:");
        Serial.println("ID: " + String(methodConfig.methodId));
        Serial.println("Name: " + methodConfig.methodName);
      }
    } else {
      methodConfig.methodId = 5;
      methodConfig.methodName = "Egyptian General Authority of Survey";
      Serial.println("No method selection found - using default (Egyptian)");
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// RTC Functions
// ============================================
bool initRTC() {
    Serial.println("\n========================================");
    Serial.println("INITIALIZING DS3231 RTC");
    Serial.println("========================================");
    
    if (!rtc.begin()) {
        Serial.println("DS3231 not found!");
        Serial.println("Wiring:");
        Serial.println("  SDA -> GPIO21");
        Serial.println("  SCL -> GPIO22");
        Serial.println("  VCC -> 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("\nRunning without RTC");
        Serial.println("========================================\n");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            const time_t EPOCH_2000 = 946684800;
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = EPOCH_2000;  // Force hardcoded UTC
            
            // Safety check
            if (timeConfig.currentTime < EPOCH_2000) {
                timeConfig.currentTime = EPOCH_2000;
            }
            
            xSemaphoreGive(timeMutex);
        }
        return false;
    }
    
    Serial.println("DS3231 detected on I2C");
    
    // Test apakah RTC bisa menyimpan waktu
    Serial.println("Testing RTC functionality...");
    rtc.adjust(DateTime(2024, 12, 16, 10, 30, 0));
    delay(2000); // Tunggu 2 detik
    
    DateTime test = rtc.now();
    Serial.printf("Test result: %02d:%02d:%02d %02d/%02d/%04d\n",
                 test.hour(), test.minute(), test.second(),
                 test.day(), test.month(), test.year());
    
    // Validasi hasil
    bool isValid = (
        test.year() >= 2024 && test.year() <= 2025 &&
        test.month() >= 1 && test.month() <= 12 &&
        test.day() >= 1 && test.day() <= 31 &&
        test.hour() >= 0 && test.hour() <= 23 &&
        test.minute() >= 0 && test.minute() <= 59 &&
        test.second() >= 0 && test.second() <= 59
    );
    
    if (!isValid) {
        Serial.println("\n*** RTC HARDWARE FAILURE ***");
        Serial.println("DS3231 chip is defective!");
        Serial.println("Time registers return garbage data");
        Serial.println("Temperature sensor works: " + String(rtc.getTemperature()) + "Ã‚Â°C");
        Serial.println("\nPossible causes:");
        Serial.println("  1. Counterfeit/clone DS3231 chip");
        Serial.println("  2. Crystal oscillator failure");
        Serial.println("  3. Internal SRAM corruption");
        Serial.println("\n>>> SOLUTION: BUY NEW DS3231 MODULE <<<");
        Serial.println("\nSystem will run without RTC");
        Serial.println("Time will reset to 01/01/2000 on every restart");
        Serial.println("NTP sync will fix time when WiFi connects");
        Serial.println("========================================\n");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            const time_t EPOCH_2000 = 946684800;
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = EPOCH_2000;  // Force hardcoded UTC
            
            // Safety check
            if (timeConfig.currentTime < EPOCH_2000) {
                timeConfig.currentTime = EPOCH_2000;
            }
            
            xSemaphoreGive(timeMutex);
        }
        
        return false;
    }
    
    Serial.println("RTC hardware test PASSED");
    Serial.println("RTC is working correctly");
    
    // ========================================
    // CHECK BATTERY STATUS
    // ========================================
    if (rtc.lostPower()) {
        Serial.println("\nWARNING: RTC lost power!");
        Serial.println("Battery may be dead or disconnected");
        Serial.println("Time will be set from NTP sync");
        
        rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
    } else {
        Serial.println("\nRTC battery backup is good");
    }
    
    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        setTime(test.hour(), test.minute(), test.second(),
               test.day(), test.month(), test.year());
        timeConfig.currentTime = now();
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

bool isRTCTimeValid(DateTime dt) {
    return (
        dt.year() >= 2000 && dt.year() <= 2100 &&
        dt.month() >= 1 && dt.month() <= 12 &&
        dt.day() >= 1 && dt.day() <= 31 &&
        dt.hour() >= 0 && dt.hour() <= 23 &&
        dt.minute() >= 0 && dt.minute() <= 59 &&
        dt.second() >= 0 && dt.second() <= 59
    );
}

bool isRTCValid() {
    if (!rtcAvailable) {
        return false;
    }
    
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (rtc.lostPower()) {
            xSemaphoreGive(i2cMutex);
            Serial.println("RTC Check: Battery dead/lost power");
            return false;
        }
        
        DateTime rtcNow = rtc.now();
        xSemaphoreGive(i2cMutex);
        
        if (!isRTCTimeValid(rtcNow)) {
            Serial.println("RTC Check: Time invalid");
            Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                         rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                         rtcNow.day(), rtcNow.month(), rtcNow.year());
            return false;
        }
        
        return true;
    }
    
    Serial.println("RTC Check: Cannot acquire I2C mutex");
    return false;
}

void saveTimeToRTC() {
    if (!rtcAvailable) {
        return;
    }
    
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        DateTime dt(year(timeConfig.currentTime),
                   month(timeConfig.currentTime),
                   day(timeConfig.currentTime),
                   hour(timeConfig.currentTime),
                   minute(timeConfig.currentTime),
                   second(timeConfig.currentTime));
        
        rtc.adjust(dt);
        
        xSemaphoreGive(timeMutex);
        
        delay(500);
        DateTime verify = rtc.now();
        
        bool saved = (
            verify.year() >= 2000 && verify.year() <= 2100 &&
            verify.month() >= 1 && verify.month() <= 12 &&
            verify.day() >= 1 && verify.day() <= 31 &&
            verify.hour() >= 0 && verify.hour() <= 23
        );
        
        if (saved) {
            Serial.println("Time saved to RTC successfully");
        } else {
            Serial.println("WARNING: RTC save failed (hardware issue)");
            Serial.println("RTC returned: " + String(verify.hour()) + ":" + 
                          String(verify.minute()) + " " + 
                          String(verify.day()) + "/" + String(verify.month()));
            
            rtcAvailable = false;
            Serial.println("RTC disabled due to hardware failure");
        }
    }
}


// ============================================
// Web Server Functions
// ============================================
void setupServerRoutes() {
  // ========================================
  // HALAMAN UTAMA & ASSETS
  // ========================================
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/index.html")) {
      request -> send(404, "text/plain", "index.html not found");
      return;
    }

    AsyncWebServerResponse * response = request -> beginResponse(
      LittleFS, "/index.html", "text/html");

    response -> addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response -> addHeader("Pragma", "no-cache");
    response -> addHeader("Expires", "0");
    response -> addHeader("Content-Type", "text/html; charset=utf-8");

    request -> send(response);
  });

  server.on("/assets/css/foundation.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/assets/css/foundation.min.css")) {
      request -> send(404, "text/plain", "CSS not found");
      return;
    }

    AsyncWebServerResponse * response = request -> beginResponse(
      LittleFS, "/assets/css/foundation.min.css", "text/css");

    response -> addHeader("Cache-Control", "public, max-age=3600");
    response -> addHeader("Content-Type", "text/css; charset=utf-8");

    request -> send(response);
  });

  // ========================================
  // TAB BERANDA - DEVICE STATUS
  // ========================================
  server.on("/devicestatus", HTTP_GET, [](AsyncWebServerRequest * request) {
    char timeStr[20];
    char dateStr[20];

    time_t now_t;
    struct tm timeinfo;

    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      now_t = timeConfig.currentTime;
      xSemaphoreGive(timeMutex);
    } else {
      time( & now_t);
    }

    localtime_r( & now_t, & timeinfo);

    sprintf(timeStr, "%02d:%02d:%02d",
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);

    sprintf(dateStr, "%02d/%02d/%04d",
      timeinfo.tm_mday,
      timeinfo.tm_mon + 1,
      timeinfo.tm_year + 1900);

    bool isWiFiConnected = (WiFi.status() == WL_CONNECTED && wifiConfig.isConnected && wifiConfig.localIP.toString() != "0.0.0.0");

    String ssid = isWiFiConnected ? WiFi.SSID() : "";
    String ip = isWiFiConnected ? wifiConfig.localIP.toString() : "-";

    // Pre-allocate buffer
    char jsonBuffer[512];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{"
      "\"connected\":%s,"
      "\"ssid\":\"%s\","
      "\"ip\":\"%s\","
      "\"ntpSynced\":%s,"
      "\"ntpServer\":\"%s\","
      "\"currentTime\":\"%s\","
      "\"currentDate\":\"%s\","
      "\"uptime\":%lu,"
      "\"freeHeap\":\"%d\""
      "}",
      isWiFiConnected ? "true" : "false",
      ssid.c_str(),
      ip.c_str(),
      timeConfig.ntpSynced ? "true" : "false",
      timeConfig.ntpServer.c_str(),
      timeStr,
      dateStr,
      millis() / 1000,
      ESP.getFreeHeap()
    );

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", jsonBuffer);
    resp -> addHeader("Cache-Control", "no-cache");
    resp -> addHeader("Content-Length", String(strlen(jsonBuffer)));
    request -> send(resp);
  });

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    IPAddress clientIP = request->client()->remoteIP();
    IPAddress apIP = WiFi.softAPIP();
    IPAddress apSubnet = WiFi.softAPSubnetMask();
    
    IPAddress apNetwork(
        apIP[0] & apSubnet[0],
        apIP[1] & apSubnet[1],
        apIP[2] & apSubnet[2],
        apIP[3] & apSubnet[3]
    );
    
    IPAddress clientNetwork(
        clientIP[0] & apSubnet[0],
        clientIP[1] & apSubnet[1],
        clientIP[2] & apSubnet[2],
        clientIP[3] & apSubnet[3]
    );
    
    bool isLocalAP = (apNetwork == clientNetwork);
    
    Serial.println("\n========================================");
    Serial.println("RESTART REQUEST RECEIVED");
    Serial.println("========================================");
    Serial.println("Client IP: " + clientIP.toString());
    Serial.println("Access: " + String(isLocalAP ? "Local AP" : "Remote WiFi"));
    
    startCountdown("device_restart", "Memulai ulang perangkat", 60);
    
    Serial.println("Countdown started");
    Serial.println("========================================\n");
    
    request->send(200, "text/plain", "OK");
    
    xTaskCreate(
      [](void* param) {
          for (int i = 60; i > 0; i--) {
              if (i == 60 || i == 20 || i == 10 || i <= 5) {
                  Serial.printf("Restarting in %d seconds...\n", i);
              }
              vTaskDelay(pdMS_TO_TICKS(1000));
          }
          
          Serial.println("Restarting NOW...");
          Serial.flush();
          delay(1000);
          
          ESP.restart();
          vTaskDelete(NULL);
      },
      "DeviceRestartTask",
      2048,
      NULL,
      1,
      NULL
    );
  });

  // ========================================
  // TAB WIFI - ROUTER & AP CONFIG
  // ========================================
  server.on("/getwificonfig", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";

    if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      json += "\"routerSSID\":\"" + wifiConfig.routerSSID + "\",";
      json += "\"routerPassword\":\"" + wifiConfig.routerPassword + "\",";
      xSemaphoreGive(wifiMutex);
    } else {
      json += "\"routerSSID\":\"\",";
      json += "\"routerPassword\":\"\",";
    }

    String currentAPSSID = WiFi.softAPSSID();
    if (currentAPSSID.length() == 0 || currentAPSSID == "null") {
      currentAPSSID = String(wifiConfig.apSSID);
    }
    if (currentAPSSID.length() == 0) {
      currentAPSSID = DEFAULT_AP_SSID;
    }

    String apPassword = String(wifiConfig.apPassword);
    if (apPassword.length() == 0) {
      apPassword = DEFAULT_AP_PASSWORD;
    }

    json += "\"apSSID\":\"" + currentAPSSID + "\",";
    json += "\"apPassword\":\"" + apPassword + "\",";

    json += "\"apIP\":\"" + wifiConfig.apIP.toString() + "\",";
    json += "\"apGateway\":\"" + wifiConfig.apGateway.toString() + "\",";
    json += "\"apSubnet\":\"" + wifiConfig.apSubnet.toString() + "\"";

    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest * request) {
    // ============================================
    // DEBOUNCING CHECK
    // ============================================
    unsigned long now = millis();
    if (now - lastWiFiRestartRequest < RESTART_DEBOUNCE_MS) {
      unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastWiFiRestartRequest);

      String msg = "Please wait " +
        String(waitTime / 1000) +
        " seconds before next WiFi restart";

      Serial.println("\n========================================");
      Serial.println("WiFi RESTART REQUEST REJECTED");
      Serial.println("========================================");
      Serial.println("Reason: Too fast (debouncing active)");
      Serial.printf("Wait time: %lu ms\n", waitTime);
      Serial.println("========================================\n");

      request -> send(429, "text/plain", msg); // 429 = Too Many Requests
      return;
    }

    if (request -> hasParam("ssid", true) && request -> hasParam("password", true)) {
      String newSSID = request -> getParam("ssid", true) -> value();
      String newPassword = request -> getParam("password", true) -> value();

      Serial.println("\n========================================");
      Serial.println("Simpan WiFi Credentials");
      Serial.println("========================================");
      Serial.println("New SSID: " + newSSID);

      // Simpan ke memory dan file
      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        wifiConfig.routerSSID = newSSID;
        wifiConfig.routerPassword = newPassword;
        xSemaphoreGive(wifiMutex);
      }

      saveWiFiCredentials();

      Serial.println("Credentials saved to LittleFS");
      Serial.println("WiFi will reconnect in 3 seconds...");
      Serial.println("========================================\n");

      request -> send(200, "text/plain", "OK");

      xTaskCreate(
        restartWiFiTask, // Function
        "WiFiRestart", // Name
        4096, // Stack size
        NULL, // Parameter
        1, // Priority (low)
        NULL // Handle
      );

    } else {
      request -> send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/setap", HTTP_POST, [](AsyncWebServerRequest * request) {
      // ============================================
      // DEBOUNCING CHECK
      // ============================================
      unsigned long now = millis();
      if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
          unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);
          request -> send(429, "text/plain", "Please wait " + String(waitTime / 1000) + " seconds");
          return;
      }

      // ============================================
      // VALIDATE PARAMETERS
      // ============================================
      if (!request -> hasParam("ssid", true) || !request -> hasParam("password", true)) {
          request -> send(400, "text/plain", "Missing ssid or password");
          return;
      }

      String ssid = request -> getParam("ssid", true) -> value();
      String pass = request -> getParam("password", true) -> value();

      if (pass.length() > 0 && pass.length() < 8) {
          request -> send(400, "text/plain", "Password minimal 8 karakter");
          return;
      }

      Serial.println("\n========================================");
      Serial.println("SIMPAN AP CONFIGURATION");
      Serial.println("========================================");

      // ============================================
      // LOAD IP SETTINGS (ALWAYS)
      // ============================================
      IPAddress newAPIP = wifiConfig.apIP;
      IPAddress newGateway = wifiConfig.apGateway;
      IPAddress newSubnet = wifiConfig.apSubnet;

      if (request -> hasParam("apIP", true)) {
          String ipStr = request -> getParam("apIP", true) -> value();
          ipStr.trim();

          IPAddress tempIP;
          if (tempIP.fromString(ipStr)) {
              newAPIP = tempIP;
              Serial.println("New IP: " + newAPIP.toString());
          } else {
              Serial.println("Invalid IP format, keeping: " + newAPIP.toString());
          }
      } else {
          Serial.println("No IP param, keeping: " + newAPIP.toString());
      }

      if (request -> hasParam("gateway", true)) {
          String gwStr = request -> getParam("gateway", true) -> value();
          gwStr.trim();

          IPAddress tempGW;
          if (tempGW.fromString(gwStr)) {
              newGateway = tempGW;
              Serial.println("New Gateway: " + newGateway.toString());
          } else {
              Serial.println("Invalid Gateway, keeping: " + newGateway.toString());
          }
      } else {
          Serial.println("No Gateway param, keeping: " + newGateway.toString());
      }

      if (request -> hasParam("subnet", true)) {
          String snStr = request -> getParam("subnet", true) -> value();
          snStr.trim();

          IPAddress tempSN;
          if (tempSN.fromString(snStr)) {
              newSubnet = tempSN;
              Serial.println("New Subnet: " + newSubnet.toString());
          } else {
              Serial.println("Invalid Subnet, keeping: " + newSubnet.toString());
          }
      } else {
          Serial.println("No Subnet param, keeping: " + newSubnet.toString());
      }

      Serial.println("========================================");

      // ============================================
      // SAVE TO CONFIG (ALWAYS)
      // ============================================
      ssid.toCharArray(wifiConfig.apSSID, 33);
      pass.toCharArray(wifiConfig.apPassword, 65);
      wifiConfig.apIP = newAPIP;
      wifiConfig.apGateway = newGateway;
      wifiConfig.apSubnet = newSubnet;

      saveAPCredentials();

      Serial.println("SAVED CONFIG:");
      Serial.println("  SSID: " + String(wifiConfig.apSSID));
      Serial.println("  IP: " + newAPIP.toString());
      Serial.println("========================================\n");

      IPAddress clientIP = request->client()->remoteIP();
      IPAddress apIP = WiFi.softAPIP();
      IPAddress apSubnet = WiFi.softAPSubnetMask();
      
      IPAddress apNetwork(
          apIP[0] & apSubnet[0],
          apIP[1] & apSubnet[1],
          apIP[2] & apSubnet[2],
          apIP[3] & apSubnet[3]
      );
      
      IPAddress clientNetwork(
          clientIP[0] & apSubnet[0],
          clientIP[1] & apSubnet[1],
          clientIP[2] & apSubnet[2],
          clientIP[3] & apSubnet[3]
      );
      
      bool isLocalAP = (apNetwork == clientNetwork);
      
      if (isLocalAP) {
          startCountdown("ap_restart", "Memulai ulang Access Point", 60);
          Serial.println("Countdown started (client is on local AP)");
      } else {
          Serial.println("Client is on router network - no countdown needed");
      }

      request->send(200, "text/plain", "OK");

      xTaskCreate(restartAPTask, "APRestart", 4096, NULL, 1, NULL);
  });

  server.on("/api/connection-type", HTTP_GET, [](AsyncWebServerRequest * request) {
    IPAddress clientIP = request -> client() -> remoteIP();
    IPAddress apIP = WiFi.softAPIP();
    IPAddress apSubnet = WiFi.softAPSubnetMask();

    IPAddress apNetwork(
      apIP[0] & apSubnet[0],
      apIP[1] & apSubnet[1],
      apIP[2] & apSubnet[2],
      apIP[3] & apSubnet[3]
    );

    IPAddress clientNetwork(
      clientIP[0] & apSubnet[0],
      clientIP[1] & apSubnet[1],
      clientIP[2] & apSubnet[2],
      clientIP[3] & apSubnet[3]
    );

    bool isLocalAP = (apNetwork == clientNetwork);

    String json = "{";
    json += "\"isLocalAP\":" + String(isLocalAP ? "true" : "false") + ",";
    json += "\"clientIP\":\"" + clientIP.toString() + "\",";
    json += "\"apIP\":\"" + apIP.toString() + "\",";
    json += "\"apSubnet\":\"" + apSubnet.toString() + "\"";
    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  // ========================================
  // TAB WAKTU - TIME SYNC & TIMEZONE
  // ========================================
  server.on("/synctime", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request -> hasParam("y", true) &&
      request -> hasParam("m", true) &&
      request -> hasParam("d", true) &&
      request -> hasParam("h", true) &&
      request -> hasParam("i", true) &&
      request -> hasParam("s", true)) {

      int y = request -> getParam("y", true) -> value().toInt();
      int m = request -> getParam("m", true) -> value().toInt();
      int d = request -> getParam("d", true) -> value().toInt();
      int h = request -> getParam("h", true) -> value().toInt();
      int i = request -> getParam("i", true) -> value().toInt();
      int s = request -> getParam("s", true) -> value().toInt();

      Serial.println("\n========================================");
      Serial.println("BROWSER TIME SYNC");
      Serial.println("========================================");
      Serial.printf("Received: %02d:%02d:%02d %02d/%02d/%04d\n", h, i, s, d, m, y);

      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        setTime(h, i, s, d, m, y);
        timeConfig.currentTime = now();
        timeConfig.ntpSynced = true;

        DisplayUpdate update;
        update.type = DisplayUpdate::TIME_UPDATE;
        xQueueSend(displayQueue, & update, 0);

        xSemaphoreGive(timeMutex);
      }

      if (rtcAvailable) {
        Serial.println("\nSaving time to RTC hardware...");

        saveTimeToRTC();

        delay(500);

        DateTime rtcNow = rtc.now();
        Serial.println("RTC Verification:");
        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
          rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
          rtcNow.day(), rtcNow.month(), rtcNow.year());

        bool rtcValid = (
          rtcNow.year() >= 2000 && rtcNow.year() <= 2100 &&
          rtcNow.month() >= 1 && rtcNow.month() <= 12 &&
          rtcNow.day() >= 1 && rtcNow.day() <= 31
        );

        if (rtcValid) {
          Serial.println("RTC saved successfully");
          Serial.println("Time will persist across restarts");
        } else {
          Serial.println("RTC save FAILED - time is invalid");
          Serial.println("Check RTC battery or I2C connection");
        }
      } else {
        Serial.println("\nRTC not available - time will reset on restart");
      }

      Serial.println("========================================\n");

      AsyncWebServerResponse * resp = request -> beginResponse(200, "text/plain", "Waktu berhasil di-sync!");
      request -> send(resp);

    } else {
      request -> send(400, "text/plain", "Data waktu tidak lengkap");
    }
  });

  server.on("/gettimezone", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      json += "\"offset\":" + String(timezoneOffset);
      xSemaphoreGive(settingsMutex);
    } else {
      json += "\"offset\":7";
    }

    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/settimezone", HTTP_POST, [](AsyncWebServerRequest * request) {
    Serial.println("\n========================================");
    Serial.println("Simpan Timezone");
    Serial.println("========================================");

    if (!request -> hasParam("offset", true)) {
      Serial.println("ERROR: Missing offset parameter");
      request -> send(400, "application/json",
        "{\"error\":\"Missing offset parameter\"}");
      return;
    }

    String offsetStr = request -> getParam("offset", true) -> value();
    offsetStr.trim();

    int offset = offsetStr.toInt();

    Serial.println("Received offset: " + String(offset));

    if (offset < -12 || offset > 14) {
      Serial.println("ERROR: Invalid timezone offset");
      request -> send(400, "application/json",
        "{\"error\":\"Invalid timezone offset (must be -12 to +14)\"}");
      return;
    }

    Serial.println("Saving to memory and file...");

    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
      timezoneOffset = offset;
      xSemaphoreGive(settingsMutex);
      Serial.println("Memory updated");
    }

    saveTimezoneConfig();

    Serial.println("========================================");
    Serial.println("SUCCESS: Timezone saved");
    Serial.println("Offset: UTC" + String(offset >= 0 ? "+" : "") + String(offset));
    Serial.println("========================================\n");

    // ========================================
    // TRIGGER NTP SYNC
    // ========================================
    bool ntpTriggered = false;
    bool prayerWillUpdate = false;

    if (wifiConfig.isConnected && ntpTaskHandle != NULL) {
      Serial.println("\n========================================");
      Serial.println("AUTO-TRIGGERING NTP RE-SYNC");
      Serial.println("========================================");
      Serial.println("Reason: Timezone changed to UTC" + String(offset >= 0 ? "+" : "") + String(offset));

      // Check if prayer times will be updated
      if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
        Serial.println("City: " + prayerConfig.selectedCity);
        Serial.println("Coordinates: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
        Serial.println("");
        Serial.println("NTP Task will automatically:");
        Serial.println("  1. Sync time with new timezone");
        Serial.println("  2. Update prayer times with correct date");
        prayerWillUpdate = true;
      } else {
        Serial.println("Note: No city coordinates available");
        Serial.println("Only time will be synced (no prayer times update)");
      }

      // Reset NTP status
      if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        timeConfig.ntpSynced = false;
        xSemaphoreGive(timeMutex);
      }

      // Trigger NTP sync
      xTaskNotifyGive(ntpTaskHandle);
      ntpTriggered = true;

      Serial.println("NTP re-sync triggered successfully");
      Serial.println("========================================\n");

    } else {
      Serial.println("\nCannot trigger NTP sync:");
      if (!wifiConfig.isConnected) {
        Serial.println("Reason: WiFi not connected");
      }
      if (ntpTaskHandle == NULL) {
        Serial.println("Reason: NTP task not running");
      }
      Serial.println("Timezone will apply on next connection\n");
    }

    // ========================================
    // SEND RESPONSE
    // ========================================
    String response = "{";
    response += "\"success\":true,";
    response += "\"offset\":" + String(offset) + ",";
    response += "\"ntpTriggered\":" + String(ntpTriggered ? "true" : "false") + ",";
    response += "\"prayerTimesWillUpdate\":" + String(prayerWillUpdate ? "true" : "false");
    response += "}";

    request -> send(200, "application/json", response);
  });

  // ========================================
  // TAB LOKASI - CITY & METHOD
  // ========================================
  server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/cities.json")) {
      Serial.println("cities.json not found");
      request -> send(404, "application/json", "[]");
      return;
    }

    AsyncWebServerResponse * response = request -> beginResponse(
      LittleFS,
      "/cities.json",
      "application/json");

    response -> addHeader("Access-Control-Allow-Origin", "*");
    response -> addHeader("Cache-Control", "public, max-age=3600");
    response -> setContentLength(LittleFS.open("/cities.json", "r").size());
    request -> send(response);
  });

  server.on("/getcityinfo", HTTP_GET, [](AsyncWebServerRequest * request) {
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

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/setcity", HTTP_POST, [](AsyncWebServerRequest * request) {
    Serial.println("\n========================================");
    Serial.println("Simpan City/Distric");
    Serial.println("========================================");

    if (!request -> hasParam("city", true)) {
      request -> send(400, "application/json", "{\"error\":\"Missing city parameter\"}");
      return;
    }

    String cityApi = request -> getParam("city", true) -> value();
    String cityName = request -> hasParam("cityName", true) ? request -> getParam("cityName", true) -> value() : cityApi;
    String lat = request -> hasParam("lat", true) ? request -> getParam("lat", true) -> value() : "";
    String lon = request -> hasParam("lon", true) ? request -> getParam("lon", true) -> value() : "";

    cityApi.trim();
    cityName.trim();
    lat.trim();
    lon.trim();

    Serial.println("City: " + cityApi);
    Serial.println("Lat: " + lat + ", Lon: " + lon);

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      prayerConfig.selectedCity = cityApi;
      prayerConfig.selectedCityName = cityName;
      prayerConfig.latitude = lat;
      prayerConfig.longitude = lon;
      xSemaphoreGive(settingsMutex);
    }

    saveCitySelection();
    updateCityDisplay();

    bool willUpdate = (WiFi.status() == WL_CONNECTED && lat.length() > 0 && lon.length() > 0);

    char responseBuffer[256];
    snprintf(responseBuffer, sizeof(responseBuffer),
      "{\"success\":true,\"city\":\"%s\",\"updating\":%s}",
      cityName.c_str(),
      willUpdate ? "true" : "false"
    );

    request -> send(200, "application/json", responseBuffer);

    if (willUpdate) {
      Serial.println("Triggering background prayer times update...");
      pendingPrayerLat = lat;
      pendingPrayerLon = lon;
      needPrayerUpdate = true;

      if (prayerTaskHandle != NULL) {
        xTaskNotifyGive(prayerTaskHandle);
      }
    }

    Serial.println("========================================\n");
  });

  server.on(
    "/uploadcities", HTTP_POST,
    [](AsyncWebServerRequest * request) {
      String jsonSizeStr = "";
      String citiesCountStr = "";

      if (request -> hasParam("jsonSize", true)) {
        jsonSizeStr = request -> getParam("jsonSize", true) -> value();
      }

      if (request -> hasParam("citiesCount", true)) {
        citiesCountStr = request -> getParam("citiesCount", true) -> value();
      }

      if (jsonSizeStr.length() > 0 && citiesCountStr.length() > 0) {
        fs::File metaFile = LittleFS.open("/cities_meta.txt", "w");
        if (metaFile) {
          metaFile.println(jsonSizeStr);
          metaFile.println(citiesCountStr);
          metaFile.close();

          Serial.println("Cities metadata saved:");
          Serial.println("JSON Size: " + jsonSizeStr + " bytes");
          Serial.println("Cities Count: " + citiesCountStr);
        }
      }

      request -> send(200, "application/json", "{\"success\":true}");
    },
    [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t * data, size_t len, bool final) {
      static fs::File uploadFile;
      static size_t totalSize = 0;
      static unsigned long uploadStartTime = 0;

      if (index == 0) {
        Serial.println("\n========================================");
        Serial.println("CITIES.JSON UPLOAD STARTED");
        Serial.println("========================================");
        Serial.printf("Filename: %s\n", filename.c_str());

        if (filename != "cities.json") {
          Serial.printf("Invalid filename: %s (must be cities.json)\n", filename.c_str());
          return;
        }

        if (LittleFS.exists("/cities.json")) {
          LittleFS.remove("/cities.json");
          Serial.println("Old cities.json deleted");
        }

        uploadFile = LittleFS.open("/cities.json", "w");
        if (!uploadFile) {
          Serial.println("Failed to open file for writing");
          return;
        }

        totalSize = 0;
        uploadStartTime = millis();
        Serial.println("Writing to LittleFS...");
      }

      if (uploadFile) {
        size_t written = uploadFile.write(data, len);
        if (written != len) {
          Serial.printf("Write mismatch: %d/%d bytes\n", written, len);
        }
        totalSize += written;

        if (totalSize % 5120 == 0 || final) {
          Serial.printf("Progress: %d bytes (%.1f KB)\n",
            totalSize, totalSize / 1024.0);
        }
      }

      if (final) {
        if (uploadFile) {
          uploadFile.flush();
          uploadFile.close();

          unsigned long uploadDuration = millis() - uploadStartTime;

          Serial.println("\nUpload complete");
          Serial.printf("Total size: %d bytes (%.2f KB)\n",
            totalSize, totalSize / 1024.0);
          Serial.printf("Duration: %lu ms\n", uploadDuration);

          vTaskDelay(pdMS_TO_TICKS(100));

          if (LittleFS.exists("/cities.json")) {
            fs::File verifyFile = LittleFS.open("/cities.json", "r");
            if (verifyFile) {
              size_t fileSize = verifyFile.size();

              char buffer[101];
              size_t bytesRead = verifyFile.readBytes(buffer, 100);
              buffer[bytesRead] = '\0';

              verifyFile.close();

              Serial.printf("File verified: %d bytes\n", fileSize);
              Serial.println("First 100 chars:");
              Serial.println(buffer);

              String preview(buffer);
              if (preview.indexOf('[') >= 0 && preview.indexOf('{') >= 0) {
                Serial.println("JSON format looks valid");
              } else {
                Serial.println("Warning: File may not be valid JSON");
              }

              Serial.println("========================================\n");
            }
          } else {
            Serial.println("File verification failed - file not found");
            Serial.println("========================================\n");
          }
        }
      }
    }
  );

  server.on("/getmethod", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";

    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      json += "\"methodId\":" + String(methodConfig.methodId) + ",";
      json += "\"methodName\":\"" + methodConfig.methodName + "\"";
      xSemaphoreGive(settingsMutex);
    } else {
      json += "\"methodId\":5,";
      json += "\"methodName\":\"Egyptian General Authority of Survey\"";
    }

    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/setmethod", HTTP_POST, [](AsyncWebServerRequest * request) {
    Serial.println("\n========================================");
    Serial.println("Simpan Metode");
    Serial.print("Client IP: ");
    Serial.println(request -> client() -> remoteIP().toString());
    Serial.println("========================================");

    if (!request -> hasParam("methodId", true) || !request -> hasParam("methodName", true)) {
      Serial.println("ERROR: Missing parameters");

      int params = request -> params();
      Serial.printf("Received parameters (%d):\n", params);
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter * p = request -> getParam(i);
        Serial.printf("  %s = %s\n", p -> name().c_str(), p -> value().c_str());
      }

      request -> send(400, "application/json",
        "{\"error\":\"Missing methodId or methodName parameter\"}");
      return;
    }

    String methodIdStr = request -> getParam("methodId", true) -> value();
    String methodName = request -> getParam("methodName", true) -> value();

    methodIdStr.trim();
    methodName.trim();

    int methodId = methodIdStr.toInt();

    Serial.println("Received data:");
    Serial.println("Method ID: " + String(methodId));
    Serial.println("Method Name: " + methodName);

    if (methodId < 0 || methodId > 20) {
      Serial.println("ERROR: Invalid method ID");
      request -> send(400, "application/json",
        "{\"error\":\"Invalid method ID\"}");
      return;
    }

    if (methodName.length() == 0) {
      Serial.println("ERROR: Empty method name");
      request -> send(400, "application/json",
        "{\"error\":\"Method name cannot be empty\"}");
      return;
    }

    if (methodName.length() > 100) {
      Serial.println("ERROR: Method name too long");
      request -> send(400, "application/json",
        "{\"error\":\"Method name too long (max 100 chars)\"}");
      return;
    }

    Serial.println("Saving to memory...");

    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
      methodConfig.methodId = methodId;
      methodConfig.methodName = methodName;

      xSemaphoreGive(settingsMutex);
      Serial.println("Memory updated");
      Serial.println("Method ID: " + String(methodConfig.methodId));
      Serial.println("Method Name: " + methodConfig.methodName);
    }

    Serial.println("Writing to LittleFS...");
    saveMethodSelection();

    bool willFetchPrayerTimes = false;

    if (WiFi.status() == WL_CONNECTED) {
      if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
        Serial.println("Fetching prayer times with new method...");
        Serial.println("City: " + prayerConfig.selectedCity);
        Serial.println("Method: " + methodName);
        getPrayerTimesByCoordinates(prayerConfig.latitude, prayerConfig.longitude);

        Serial.println("Prayer times update initiated");
        willFetchPrayerTimes = true;
      } else {
        Serial.println("No coordinates available");
      }
    } else {
      Serial.println("WiFi not connected");
    }

    Serial.println("========================================");
    Serial.println("SUCCESS: Method saved successfully");
    Serial.println("Method: " + methodName);
    if (willFetchPrayerTimes) {
      Serial.println("Prayer times will update shortly...");
    }
    Serial.println("========================================\n");

    String response = "{";
    response += "\"success\":true,";
    response += "\"methodId\":" + String(methodId) + ",";
    response += "\"methodName\":\"" + methodName + "\",";
    response += "\"prayerTimesUpdating\":" + String(willFetchPrayerTimes ? "true" : "false");
    response += "}";

    request -> send(200, "application/json", response);
  });

  // ========================================
  // TAB JADWAL - PRAYER TIMES & BUZZER
  // ========================================
  server.on("/getprayertimes", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";
    json += "\"imsak\":\"" + prayerConfig.imsakTime + "\",";
    json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
    json += "\"terbit\":\"" + prayerConfig.terbitTime + "\",";
    json += "\"zuhur\":\"" + prayerConfig.zuhurTime + "\",";
    json += "\"ashar\":\"" + prayerConfig.asharTime + "\",";
    json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
    json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/getbuzzerconfig", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";
    json += "\"imsak\":" + String(buzzerConfig.imsakEnabled ? "true" : "false") + ",";
    json += "\"subuh\":" + String(buzzerConfig.subuhEnabled ? "true" : "false") + ",";
    json += "\"terbit\":" + String(buzzerConfig.terbitEnabled ? "true" : "false") + ",";
    json += "\"zuhur\":" + String(buzzerConfig.zuhurEnabled ? "true" : "false") + ",";
    json += "\"ashar\":" + String(buzzerConfig.asharEnabled ? "true" : "false") + ",";
    json += "\"maghrib\":" + String(buzzerConfig.maghribEnabled ? "true" : "false") + ",";
    json += "\"isya\":" + String(buzzerConfig.isyaEnabled ? "true" : "false") + ",";
    json += "\"volume\":" + String(buzzerConfig.volume);
    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  server.on("/setbuzzertoggle", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (!request -> hasParam("prayer", true) || !request -> hasParam("enabled", true)) {
      request -> send(400, "text/plain", "Missing parameters");
      return;
    }

    String prayer = request -> getParam("prayer", true) -> value();
    bool enabled = request -> getParam("enabled", true) -> value() == "true";

    if (prayer == "imsak") buzzerConfig.imsakEnabled = enabled;
    else if (prayer == "subuh") buzzerConfig.subuhEnabled = enabled;
    else if (prayer == "terbit") buzzerConfig.terbitEnabled = enabled;
    else if (prayer == "zuhur") buzzerConfig.zuhurEnabled = enabled;
    else if (prayer == "ashar") buzzerConfig.asharEnabled = enabled;
    else if (prayer == "maghrib") buzzerConfig.maghribEnabled = enabled;
    else if (prayer == "isya") buzzerConfig.isyaEnabled = enabled;

    saveBuzzerConfig();
    request -> send(200, "text/plain", "OK");
  });

  server.on("/setbuzzervolume", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (!request -> hasParam("volume", true)) {
      request -> send(400, "text/plain", "Missing volume");
      return;
    }

    int volume = request -> getParam("volume", true) -> value().toInt();
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    buzzerConfig.volume = volume;
    saveBuzzerConfig();

    request -> send(200, "text/plain", "OK");
  });

  // ========================================
  // TAB RESET - FACTORY RESET
  // ========================================
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest * request) {
    Serial.println("\n========================================");
    Serial.println("FACTORY RESET STARTED");
    Serial.println("========================================");

    // ============================================
    // HAPUS SEMUA FILE KONFIGURASI
    // ============================================
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

    if (LittleFS.exists("/method_selection.txt")) {
        LittleFS.remove("/method_selection.txt");
        Serial.println("Method selection deleted");
    }

    if (LittleFS.exists("/timezone.txt")) {
        LittleFS.remove("/timezone.txt");
        Serial.println("Timezone config deleted");
    }

    if (LittleFS.exists("/buzzer_config.txt")) {
        LittleFS.remove("/buzzer_config.txt");
        Serial.println("Buzzer config deleted");
    }

    // ============================================
    // RESET MEMORY SETTINGS
    // ============================================
    if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        methodConfig.methodId = 5;
        methodConfig.methodName = "Egyptian General Authority of Survey";

        wifiConfig.routerSSID = "";
        wifiConfig.routerPassword = "";
        wifiConfig.isConnected = false;

        prayerConfig.imsakTime = "";
        prayerConfig.subuhTime = "";
        prayerConfig.terbitTime = "";
        prayerConfig.zuhurTime = "";
        prayerConfig.asharTime = "";
        prayerConfig.maghribTime = "";
        prayerConfig.isyaTime = "";
        prayerConfig.selectedCity = "";
        prayerConfig.selectedCityName = "";
        prayerConfig.latitude = "";
        prayerConfig.longitude = "";

        strcpy(wifiConfig.apSSID, DEFAULT_AP_SSID);
        strcpy(wifiConfig.apPassword, DEFAULT_AP_PASSWORD);
        wifiConfig.apIP = IPAddress(192, 168, 4, 1);
        wifiConfig.apGateway = IPAddress(192, 168, 4, 1);
        wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);

        xSemaphoreGive(settingsMutex);
    }

    timezoneOffset = 7;
    Serial.println("Settings reset to default");

    // ============================================
    // RESET TIME TO 01/01/2000
    // ============================================
    Serial.println("\nResetting time to 00:00:00 01/01/2000...");

    if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
        const time_t EPOCH_2000 = 946684800;
        setTime(0, 0, 0, 1, 1, 2000);
        timeConfig.currentTime = EPOCH_2000;
        timeConfig.ntpSynced = false;
        timeConfig.ntpServer = "";

        if (timeConfig.currentTime < EPOCH_2000) {
            timeConfig.currentTime = EPOCH_2000;
        }

        DisplayUpdate update;
        update.type = DisplayUpdate::TIME_UPDATE;
        xQueueSend(displayQueue, &update, 0);

        xSemaphoreGive(timeMutex);
        
        Serial.printf("Time reset to: %ld (01/01/2000 00:00:00 UTC)\n", EPOCH_2000);
    }

    // ============================================
    // SAVE TO RTC IF AVAILABLE
    // ============================================
    if (rtcAvailable) {
        Serial.println("\nSaving time to RTC hardware...");
        saveTimeToRTC();
        
        delay(500);
        
        DateTime rtcNow = rtc.now();
        Serial.println("RTC Verification:");
        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                    rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                    rtcNow.day(), rtcNow.month(), rtcNow.year());
        
        bool rtcValid = (
            rtcNow.year() >= 2000 && rtcNow.year() <= 2100 &&
            rtcNow.month() >= 1 && rtcNow.month() <= 12 &&
            rtcNow.day() >= 1 && rtcNow.day() <= 31
        );
        
        if (rtcValid) {
            Serial.println("RTC saved successfully");
        } else {
            Serial.println("RTC save FAILED");
        }
    }

    Serial.println("Time reset complete");

    // ============================================
    // UPDATE DISPLAY & DISCONNECT WIFI
    // ============================================
    updateCityDisplay();
    WiFi.disconnect(true);

    // ============================================
    // DETEKSI TIPE AKSES (Local AP vs Remote)
    // ============================================
    IPAddress clientIP = request->client()->remoteIP();
    IPAddress apIP = WiFi.softAPIP();
    IPAddress apSubnet = WiFi.softAPSubnetMask();
    
    IPAddress apNetwork(
        apIP[0] & apSubnet[0],
        apIP[1] & apSubnet[1],
        apIP[2] & apSubnet[2],
        apIP[3] & apSubnet[3]
    );
    
    IPAddress clientNetwork(
        clientIP[0] & apSubnet[0],
        clientIP[1] & apSubnet[1],
        clientIP[2] & apSubnet[2],
        clientIP[3] & apSubnet[3]
    );
    
    bool isLocalAP = (apNetwork == clientNetwork);

    Serial.println("\n========================================");
    Serial.println("FACTORY RESET COMPLETE");
    Serial.println("========================================");
    Serial.println("Client IP: " + clientIP.toString());
    Serial.println("Access: " + String(isLocalAP ? "Local AP" : "Remote WiFi"));

    startCountdown("factory_reset", "Pengaturan ulang perangkat", 60);
    
    if (isLocalAP) {
        Serial.println("Countdown started (client will see countdown)");
    } else {
        Serial.println("Countdown started (remote client will lose connection)");
    }
    Serial.println("Device will restart in 60 seconds...");
    Serial.println("========================================\n");
    
    request->send(200, "text/plain", "OK");
    
    // ============================================
    // SCHEDULE RESTART TASK
    // ============================================
    xTaskCreate(
        [](void* param) {
            for (int i = 60; i > 0; i--) {
                if (i == 60 || i == 20 || i == 10 || i <= 5) {
                    Serial.printf("Factory reset restarting in %d seconds...\n", i);
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
                
            Serial.println("Restarting NOW...");
            Serial.flush();
            delay(1000);
            
            ESP.restart();
            vTaskDelete(NULL);
        },
        "FactoryResetTask",
        2048,
        NULL,
        1,
        NULL
    );
  });

  // ========================================
  // API DATA - REAL-TIME ENDPOINT
  // ========================================
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest * request) {
    char timeStr[20], dateStr[20], dayStr[15];

    time_t now_t;
    struct tm timeinfo;

    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      now_t = timeConfig.currentTime;
      xSemaphoreGive(timeMutex);
    } else {
      time( & now_t);
    }

    localtime_r( & now_t, & timeinfo);

    sprintf(timeStr, "%02d:%02d:%02d",
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);

    sprintf(dateStr, "%02d/%02d/%04d",
      timeinfo.tm_mday,
      timeinfo.tm_mon + 1,
      timeinfo.tm_year + 1900);

    const char * dayNames[] = {
      "Sunday",
      "Monday",
      "Tuesday",
      "Wednesday",
      "Thursday",
      "Friday",
      "Saturday"
    };
    strcpy(dayStr, dayNames[timeinfo.tm_wday]);

    bool isWiFiConnected = (WiFi.status() == WL_CONNECTED && wifiConfig.isConnected);

    char jsonBuffer[1024];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{"
      "\"time\":\"%s\","
      "\"date\":\"%s\","
      "\"day\":\"%s\","
      "\"timestamp\":%lu,"
      "\"prayerTimes\":{"
      "\"imsak\":\"%s\","
      "\"subuh\":\"%s\","
      "\"terbit\":\"%s\","
      "\"zuhur\":\"%s\","
      "\"ashar\":\"%s\","
      "\"maghrib\":\"%s\","
      "\"isya\":\"%s\""
      "},"
      "\"location\":{"
      "\"city\":\"%s\","
      "\"cityId\":\"%s\","
      "\"displayName\":\"%s\","
      "\"latitude\":\"%s\","
      "\"longitude\":\"%s\""
      "},"
      "\"device\":{"
      "\"wifiConnected\":%s,"
      "\"apIP\":\"%s\","
      "\"ntpSynced\":%s,"
      "\"ntpServer\":\"%s\","
      "\"freeHeap\":%d,"
      "\"uptime\":%lu"
      "}"
      "}",
      timeStr, dateStr, dayStr, (unsigned long) now_t,
      prayerConfig.imsakTime.c_str(),
      prayerConfig.subuhTime.c_str(),
      prayerConfig.terbitTime.c_str(),
      prayerConfig.zuhurTime.c_str(),
      prayerConfig.asharTime.c_str(),
      prayerConfig.maghribTime.c_str(),
      prayerConfig.isyaTime.c_str(),
      prayerConfig.selectedCity.c_str(),
      prayerConfig.selectedCity.c_str(),
      prayerConfig.selectedCityName.c_str(),
      prayerConfig.latitude.c_str(),
      prayerConfig.longitude.c_str(),
      isWiFiConnected ? "true" : "false",
      WiFi.softAPIP().toString().c_str(),
      timeConfig.ntpSynced ? "true" : "false",
      timeConfig.ntpServer.c_str(),
      ESP.getFreeHeap(),
      millis() / 1000
    );

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", jsonBuffer);
    resp -> addHeader("Cache-Control", "no-cache");
    resp -> addHeader("Content-Length", String(strlen(jsonBuffer)));
    request -> send(resp);
  });

  // ========================================
  // COUNTDOWN STATUS API
  // ========================================
  server.on("/api/countdown", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "{";

    if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      int remaining = getRemainingSeconds();

      json += "\"active\":" + String(countdownState.isActive ? "true" : "false") + ",";
      json += "\"remaining\":" + String(remaining) + ",";
      json += "\"total\":" + String(countdownState.totalSeconds) + ",";
      json += "\"message\":\"" + countdownState.message + "\",";
      json += "\"reason\":\"" + countdownState.reason + "\"";

      xSemaphoreGive(countdownMutex);
    } else {
      json += "\"active\":false,";
      json += "\"remaining\":0,";
      json += "\"total\":0,";
      json += "\"message\":\"\",";
      json += "\"reason\":\"\"";
    }

    json += "}";

    AsyncWebServerResponse * resp = request -> beginResponse(200, "application/json", json);
    resp -> addHeader("Cache-Control", "no-cache");
    request -> send(resp);
  });

  // ========================================
  // ERROR PAGES - 404 HANDLER
  // ========================================
  server.on("/notfound", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<title>404 - Not Found</title>";
    html += "<style>";
    html += "body{margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;align-items:center;justify-content:center}";
    html += ".container{max-width:500px;margin:20px;background:white;padding:50px 40px;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,0.3);text-align:center}";
    html += ".error-code{font-size:120px;font-weight:800;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);-webkit-background-clip:text;-webkit-text-fill-color:transparent;margin:0;line-height:1}";
    html += "h2{color:#333;font-size:28px;margin:20px 0 10px;font-weight:600}";
    html += "p{color:#666;font-size:16px;line-height:1.6;margin:20px 0 30px}";
    html += ".btn{display:inline-block;padding:14px 40px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;text-decoration:none;border-radius:50px;font-weight:600;font-size:16px;transition:all 0.3s;box-shadow:0 4px 15px rgba(102,126,234,0.4)}";
    html += ".btn:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(102,126,234,0.6)}";
    html += ".icon{font-size:80px;margin-bottom:20px}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='icon'>ðŸ”</div>";
    html += "<div class='error-code'>404</div>";
    html += "<h2>Page Not Found</h2>";
    html += "<p>The page you're looking for doesn't exist or you don't have permission to access it. Please return to the home page.</p>";
    html += "<a href='/' class='btn'>â† Back to Home</a>";
    html += "</div></body></html>";

    request -> send(404, "text/html", html);
  });

  server.onNotFound([](AsyncWebServerRequest * request) {
    String url = request -> url();
    IPAddress clientIP = request -> client() -> remoteIP();

    Serial.printf("\n[404] Client: %s | URL: %s\n",
      clientIP.toString().c_str(), url.c_str());

    if (url.startsWith("/assets/") || url.endsWith(".css") || url.endsWith(".js") ||
      url.endsWith(".png") || url.endsWith(".jpg") || url.endsWith(".jpeg") ||
      url.endsWith(".gif") || url.endsWith(".ico") || url.endsWith(".svg") ||
      url.endsWith(".woff") || url.endsWith(".woff2") || url.endsWith(".ttf")) {

      request -> send(404, "text/plain", "File not found");
      return;
    }

    Serial.println("Invalid URL, redirecting to /notfound");
    request -> redirect("/notfound");
  });
}

// ============================================
// Utility Functions
// ============================================
bool init_littlefs() {
  Serial.println("Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return false;
  }
  Serial.println("LittleFS Mounted");
  return true;
}

void printStackReport() {
  Serial.println("\n========================================");
  Serial.println("STACK USAGE ANALYSIS");
  Serial.println("========================================");

  struct TaskInfo {
    TaskHandle_t handle;
    const char *name;
    uint32_t size;
  };

  TaskInfo tasks[] = {
    { uiTaskHandle, "UI", UI_TASK_STACK_SIZE },
    { webTaskHandle, "Web", WEB_TASK_STACK_SIZE },
    { wifiTaskHandle, "WiFi", WIFI_TASK_STACK_SIZE },
    { ntpTaskHandle, "NTP", NTP_TASK_STACK_SIZE },
    { prayerTaskHandle, "Prayer", PRAYER_TASK_STACK_SIZE },
    { rtcTaskHandle, "RTC", RTC_TASK_STACK_SIZE }
  };

  uint32_t totalAllocated = 0;
  uint32_t totalUsed = 0;
  uint32_t totalFree = 0;

  for (int i = 0; i < 6; i++) { 
    if (tasks[i].handle) {
      UBaseType_t hwm = uxTaskGetStackHighWaterMark(tasks[i].handle);
      
      uint32_t free = hwm * sizeof(StackType_t);
      uint32_t used = tasks[i].size - free;
      float percent = (used * 100.0) / tasks[i].size;

      totalAllocated += tasks[i].size;
      totalUsed += used;
      totalFree += free;

      Serial.printf("%-10s: %5d/%5d (%5.1f%%) [Free: %5d] ",
                    tasks[i].name, used, tasks[i].size, percent, free);

      if (percent < 40) Serial.println("BOROS - bisa dikurangi");
      else if (percent < 60) Serial.println("OPTIMAL");
      else if (percent < 75) Serial.println("PAS");
      else if (percent < 90) Serial.println("TINGGI - monitor terus");
      else if (percent < 95) Serial.println("BAHAYA - harus dinaikkan");
      else Serial.println("KRITIS - segera naikkan");
    } else {
      Serial.printf("%-10s: TASK NOT RUNNING\n", tasks[i].name);
    }
  }

  Serial.println("========================================");
  Serial.printf("Total Allocated: %d bytes (%.1f KB)\n",
                totalAllocated, totalAllocated / 1024.0);
  Serial.printf("Total Used:      %d bytes (%.1f KB)\n",
                totalUsed, totalUsed / 1024.0);
  Serial.printf("Total Free:      %d bytes (%.1f KB)\n",
                totalFree, totalFree / 1024.0);
  Serial.printf("Efficiency:      %.1f%%\n",
                (totalUsed * 100.0) / totalAllocated);
  
  bool hasCritical = false;
  for (int i = 0; i < 6; i++) {
    if (tasks[i].handle) {
      UBaseType_t hwm = uxTaskGetStackHighWaterMark(tasks[i].handle);
      uint32_t free = hwm * sizeof(StackType_t);
      uint32_t used = tasks[i].size - free;
      float percent = (used * 100.0) / tasks[i].size;
      
      if (percent >= 90) {
        if (!hasCritical) {
          Serial.println("========================================");
          Serial.println("CRITICAL TASKS:");
          hasCritical = true;
        }
        Serial.printf("   %s: %.1f%% (free: %d bytes)\n", 
                      tasks[i].name, percent, free);
      }
    }
  }
  
  if (hasCritical) {
    Serial.println("   ACTION: Increase stack size ASAP");
  }
  
  Serial.println("========================================\n");
}

// ============================================
// LVGL Callback Functions
// ============================================
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
    
    bool validTouch = false;
    
    if (spiMutex != NULL && xSemaphoreTake(spiMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (touch.touched()) {
        TS_Point p = touch.getPoint();
        if (p.z > 200) {
          lastX = map(p.x, TS_MIN_X, TS_MAX_X, 0, SCREEN_WIDTH);
          lastY = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, SCREEN_HEIGHT);
          lastX = constrain(lastX, 0, SCREEN_WIDTH - 1);
          lastY = constrain(lastY, 0, SCREEN_HEIGHT - 1);
          
          data->point.x = lastX;
          data->point.y = lastY;
          validTouch = true;
        }
      }
      xSemaphoreGive(spiMutex);
    }
    
    if (validTouch) {
      data->state = LV_INDEV_STATE_PR;
      touchPressed = true;
      return;
    }
  }
  
  data->state = LV_INDEV_STATE_REL;
  touchPressed = false;
}

// ============================================
// FreeRTOS Task Functions
// ============================================
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
            checkPrayerTime(); // Cek apakah waktu shalat masuk
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
    
    handleBlinking();
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void wifiTask(void *parameter) {
    esp_task_wdt_add(NULL);
    
    Serial.println("\n========================================");
    Serial.println("WiFi Task Started - Event-Driven Mode");
    Serial.println("========================================\n");

    bool autoUpdateDone = false;
    unsigned long lastMonitor = 0;

    while (true) {
        esp_task_wdt_reset();

        // ========================================
        // WAIT FOR WIFI EVENTS (Event-driven)
        // ========================================
        EventBits_t bits = xEventGroupWaitBits(
            wifiEventGroup,
            WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | WIFI_GOT_IP_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(5000)
        );

        // ========================================
        // EVENT: WiFi Disconnected
        // ========================================
        if (bits & WIFI_DISCONNECTED_BIT) {
            xEventGroupClearBits(wifiEventGroup, WIFI_DISCONNECTED_BIT);
            
            Serial.print("Handling disconnect event...");
            
            // ============================================
            // SAFETY
            // ============================================
            if (wifiRestartInProgress || apRestartInProgress) {
                Serial.println("\nWiFi/AP restart in progress - skipping auto-reconnect");
                Serial.println("Reason: Avoid conflict with manual restart operation");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            
            IPAddress apIP = WiFi.softAPIP();
            if (apIP == IPAddress(0, 0, 0, 0)) {
                Serial.print("AP died during disconnect Restarting...");
                
                WiFi.softAPdisconnect(false);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                Serial.print("AP restored: " + WiFi.softAPIP().toString());
            } else {
                Serial.println("AP still alive: " + apIP.toString());
            }
            
            autoUpdateDone = false;
            ntpSyncInProgress = false;
            ntpSyncCompleted = false;
            
            if (wifiConfig.routerSSID.length() > 0) {
                Serial.print("Attempting reconnect to: " + wifiConfig.routerSSID);
                WiFi.begin(wifiConfig.routerSSID.c_str(), 
                          wifiConfig.routerPassword.c_str());
                wifiState = WIFI_CONNECTING;
            }
        }

        // ========================================
        // EVENT: WiFi Got IP (Connected)
        // ========================================

        if (bits & WIFI_GOT_IP_BIT) {
            if (!autoUpdateDone && wifiConfig.isConnected) {
                // ============================================
                // TUNGGU NTP SYNC START
                // ============================================
                if (!ntpSyncInProgress && !ntpSyncCompleted) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    if (!ntpSyncInProgress && !ntpSyncCompleted) {
                        Serial.println("WARNING: NTP sync not started yet");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                    continue;
                }

                // ============================================
                // TUNGGU NTP SYNC SELESAI
                // ============================================
                if (ntpSyncInProgress) {
                    int ntpWaitCounter = 0;
                    unsigned long waitStartTime = millis();
                    const unsigned long maxWaitTime = 30000; // 30 detik
                    
                    while (ntpSyncInProgress && (millis() - waitStartTime < maxWaitTime)) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        ntpWaitCounter++;
                        
                        if (ntpWaitCounter % 10 == 0) {  // Log setiap 5 detik
                            Serial.printf("Waiting for NTP sync to complete... (%lu ms)\n", 
                                        millis() - waitStartTime);
                        }
                        
                        esp_task_wdt_reset();
                    }
                    
                    if (ntpSyncInProgress) {
                        Serial.println("NTP sync timeout - proceeding anyway");
                    }
                    
                    continue;
                }

                // ============================================
                // NTP SELESAI, UPDATE PRAYER TIMES
                // ============================================
                if (ntpSyncCompleted && timeConfig.ntpSynced) {
                    Serial.println("\n========================================");
                    Serial.println("NTP SYNC COMPLETED - UPDATING PRAYER TIMES");
                    Serial.println("========================================");

                    vTaskDelay(pdMS_TO_TICKS(2000));

                    if (prayerConfig.latitude.length() > 0 && 
                        prayerConfig.longitude.length() > 0) {
                        
                        char timeStr[20], dateStr[20];
                        time_t now_t;
                        struct tm timeinfo;

                        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            now_t = timeConfig.currentTime;
                            xSemaphoreGive(timeMutex);
                        } else {
                            time(&now_t);
                        }

                        localtime_r(&now_t, &timeinfo);

                        sprintf(timeStr, "%02d:%02d:%02d",
                                timeinfo.tm_hour,
                                timeinfo.tm_min,
                                timeinfo.tm_sec);
                        sprintf(dateStr, "%02d/%02d/%04d",
                                timeinfo.tm_mday,
                                timeinfo.tm_mon + 1,
                                timeinfo.tm_year + 1900);

                        Serial.println("Verified Time: " + String(timeStr));
                        Serial.println("Verified Date: " + String(dateStr));
                        
                        if (timeinfo.tm_year + 1900 >= 2024) {
                            Serial.println("Time is valid - fetching prayer times...");
                            Serial.println("City: " + prayerConfig.selectedCity);
                            Serial.println("Coordinates: " + prayerConfig.latitude + 
                                        ", " + prayerConfig.longitude);
                            Serial.println("");

                            esp_task_wdt_reset();
                            
                            getPrayerTimesByCoordinates(
                                prayerConfig.latitude,
                                prayerConfig.longitude);

                            Serial.println("Prayer times update completed");
                        } else {
                            Serial.println("ERROR: Time still invalid (year < 2024)");
                            Serial.println("Skipping prayer times update");
                        }
                        
                        Serial.println("========================================\n");

                    } else {
                        Serial.println("\n========================================");
                        Serial.println("PRAYER TIMES AUTO-UPDATE SKIPPED");
                        Serial.println("========================================");
                        Serial.println("Reason: No city coordinates available");
                        Serial.println("========================================\n");
                    }

                    autoUpdateDone = true;
                }
            }
            
            // Monitoring log setiap 60 detik
            if (millis() - lastMonitor > 60000) {
                lastMonitor = millis();
                Serial.printf("WiFi: Connected | RSSI: %d dBm | IP: %s\n",
                            WiFi.RSSI(),
                            WiFi.localIP().toString().c_str());
            }
        }

        // ========================================
        // Cek apakah perlu koneksi pertama kali
        // ========================================
        if (wifiState == WIFI_IDLE && wifiConfig.routerSSID.length() > 0) {
            bool isConnected = (bits & WIFI_CONNECTED_BIT) != 0;
            
            if (!isConnected) {
                Serial.print("Initial WiFi connect...");
                Serial.print("   SSID: " + wifiConfig.routerSSID);
                
                WiFi.begin(wifiConfig.routerSSID.c_str(), 
                          wifiConfig.routerPassword.c_str());
                wifiState = WIFI_CONNECTING;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ntpTask(void *parameter) {
    esp_task_wdt_add(NULL);
    
    while (true) {
        esp_task_wdt_reset();
        
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
        
        if (notifyValue == 0) {
            esp_task_wdt_reset();
            continue;
        }
        
        ntpSyncInProgress = true;
        ntpSyncCompleted = false;
        
        Serial.println("\n========================================");
        Serial.println("AUTO NTP SYNC STARTED");
        Serial.println("========================================");

        bool syncSuccess = false;
        time_t ntpTime = 0;
        String usedServer = "";
        
        // ============================================
        // STEP 1: SYNC WITH UTC+0 FIRST (NO TIMEZONE)
        // ============================================
        Serial.println("Step 1: Syncing with NTP servers (UTC+0)...");
        Serial.println("Reason: Get raw UTC time first, apply timezone AFTER success");
        Serial.println("");
        
        // Use UTC+0 for initial sync - NO TIMEZONE YET!
        configTzTime("UTC0", 
                     ntpServers[0], 
                     ntpServers[1], 
                     ntpServers[2]);
        
        // ============================================
        // STEP 2: WAIT FOR NTP SYNC (UTC TIME ONLY)
        // ============================================
        time_t now = 0;
        struct tm timeinfo = {0};
        int retry = 0;
        const int retry_count = 40;
        
        while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
            vTaskDelay(pdMS_TO_TICKS(250));
            
            time(&now);
            gmtime_r(&now, &timeinfo);  // Use gmtime_r to get UTC
            
            if (retry % 4 == 0) {
                Serial.printf("Waiting for NTP sync... (%d/%d) [%.1fs]\n", 
                             retry, retry_count, retry * 0.25);
            }
            
            esp_task_wdt_reset();
        }
        
        syncSuccess = (timeinfo.tm_year >= (2024 - 1900));
        
        if (syncSuccess) {
            ntpTime = now;  // This is UTC time
            usedServer = String(ntpServers[0]);
            
            Serial.println("\n========================================");
            Serial.println("STEP 1 SUCCESS: NTP SYNC COMPLETED (UTC)");
            Serial.println("========================================");
            Serial.printf("UTC Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("Sync duration: %.1f seconds\n", retry * 0.25);
            Serial.printf("NTP Server: %s\n", usedServer.c_str());
            Serial.println("========================================");
            
            // ============================================
            // STEP 3: APPLY TIMEZONE OFFSET (AFTER SUCCESS)
            // ============================================
            Serial.println("\n========================================");
            Serial.println("STEP 2: APPLYING TIMEZONE OFFSET");
            Serial.println("========================================");
            
            int currentOffset;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                currentOffset = timezoneOffset;
                xSemaphoreGive(settingsMutex);
            } else {
                currentOffset = 7; // Default to UTC+7
                Serial.println("WARNING: Cannot get timezone from settings, using default UTC+7");
            }
            
            Serial.printf("Timezone Setting: UTC%+d\n", currentOffset);
            Serial.printf("Offset: %+d hours (%+ld seconds)\n", 
                         currentOffset, (long)(currentOffset * 3600));
            
            // Manual timezone adjustment to UTC time
            time_t localTime = ntpTime + (currentOffset * 3600);
            
            struct tm localTimeinfo;
            localtime_r(&localTime, &localTimeinfo);
            
            Serial.println("");
            Serial.println("Time Conversion:");
            Serial.printf("   Before: %02d:%02d:%02d %02d/%02d/%04d (UTC+0)\n",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("   After:  %02d:%02d:%02d %02d/%02d/%04d (UTC%+d)\n",
                        localTimeinfo.tm_hour, localTimeinfo.tm_min, localTimeinfo.tm_sec,
                        localTimeinfo.tm_mday, localTimeinfo.tm_mon + 1, localTimeinfo.tm_year + 1900,
                        currentOffset);
            Serial.println("========================================");
            
            // Use the timezone-adjusted time
            ntpTime = localTime;
            
        } else {
            Serial.println("\n========================================");
            Serial.println("NTP SYNC FAILED");
            Serial.println("========================================");
            Serial.printf("Timeout after %.1f seconds\n", retry * 0.25);
            Serial.println("All NTP servers failed to respond");
            Serial.println("Possible causes:");
            Serial.println("  1. No internet connection");
            Serial.println("  2. Firewall blocking NTP (port 123)");
            Serial.println("  3. Router DNS issues");
            Serial.println("  4. ISP blocking NTP");
            Serial.println("");
            Serial.println("System will continue with current time");
            Serial.println("========================================");
        }
        
        // ============================================
        // STEP 4: SAVE TIME TO SYSTEM (IF SUCCESS)
        // ============================================
        if (syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("STEP 3: SAVING TIME TO SYSTEM");
            Serial.println("========================================");
            
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                timeConfig.currentTime = ntpTime;
                setTime(timeConfig.currentTime);
                timeConfig.ntpSynced = true;
                timeConfig.ntpServer = usedServer;
                xSemaphoreGive(timeMutex);
                
                Serial.println("Time saved to system memory");
                
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
                
                Serial.println("Display update queued");
            } else {
                Serial.println("ERROR: Failed to acquire timeMutex");
                Serial.println("Time NOT saved to system");
            }
            
            Serial.println("========================================");
        }
        
        // ============================================
        // STEP 5: SAVE TO RTC (IF AVAILABLE & SUCCESS)
        // ============================================
        if (rtcAvailable && syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("STEP 4: SAVING TIME TO RTC HARDWARE");
            Serial.println("========================================");
            
            if (isRTCValid()) {
                Serial.println("RTC Status: VALID - Ready to save");
                Serial.println("Saving NTP time to RTC...");
                
                saveTimeToRTC();
                
                // Verify save
                vTaskDelay(pdMS_TO_TICKS(500));
                
                if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    DateTime rtcNow = rtc.now();
                    xSemaphoreGive(i2cMutex);
                    
                    Serial.println("");
                    Serial.println("RTC Verification:");
                    Serial.printf("   RTC Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                                rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                                rtcNow.day(), rtcNow.month(), rtcNow.year());
                    
                    if (isRTCTimeValid(rtcNow)) {
                        Serial.println("   Status: RTC saved successfully");
                        Serial.println("   Time will persist across restarts");
                    } else {
                        Serial.println("   Status: âœ— RTC save FAILED");
                        Serial.println("   RTC hardware may be faulty");
                    }
                } else {
                    Serial.println("   Status: Cannot verify (I2C busy)");
                }
            } else {
                Serial.println("RTC Status: INVALID - Cannot save");
                Serial.println("Possible issues:");
                Serial.println("  - Battery dead/disconnected");
                Serial.println("  - Hardware failure");
                Serial.println("  - Time corruption");
                Serial.println("");
                Serial.println("NTP time will NOT be saved to RTC");
                Serial.println("Time will reset on restart");
            }
            
            Serial.println("========================================");
        } else if (syncSuccess && !rtcAvailable) {
            Serial.println("\n========================================");
            Serial.println("RTC NOT AVAILABLE");
            Serial.println("========================================");
            Serial.println("RTC hardware not detected on boot");
            Serial.println("Time will reset to 01/01/2000 on restart");
            Serial.println("Consider installing DS3231 RTC module");
            Serial.println("========================================");
        }

        // ============================================
        // STEP 6: AUTO PRAYER TIMES UPDATE
        // ============================================
        if (syncSuccess && wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("STEP 5: AUTO PRAYER TIMES UPDATE");
            Serial.println("========================================");
            Serial.println("Reason: NTP sync completed successfully");
            Serial.println("");
            
            if (prayerConfig.latitude.length() > 0 && 
                prayerConfig.longitude.length() > 0) {
                
                Serial.println("City Configuration:");
                Serial.println("   City: " + prayerConfig.selectedCity);
                Serial.println("   Latitude: " + prayerConfig.latitude);
                Serial.println("   Longitude: " + prayerConfig.longitude);
                Serial.println("");
                Serial.println("Starting prayer times API request...");
                
                esp_task_wdt_reset();
                
                // Call prayer times API
                getPrayerTimesByCoordinates(
                    prayerConfig.latitude,
                    prayerConfig.longitude
                );
                
                esp_task_wdt_reset();
                
                Serial.println("Prayer times updated successfully");
                Serial.println("========================================");
                
            } else {
                Serial.println("SKIPPED: No city coordinates available");
                
                if (prayerConfig.latitude.length() == 0) {
                    Serial.println("   - Latitude is empty");
                }
                if (prayerConfig.longitude.length() == 0) {
                    Serial.println("   - Longitude is empty");
                }
                
                Serial.println("");
                Serial.println("Action required:");
                Serial.println("   1. Open web interface");
                Serial.println("   2. Navigate to Settings");
                Serial.println("   3. Select your city");
                Serial.println("========================================");
            }
        } else if (syncSuccess && !wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("PRAYER TIMES UPDATE SKIPPED");
            Serial.println("========================================");
            Serial.println("Reason: WiFi not connected");
            Serial.println("Prayer times will update when WiFi connects");
            Serial.println("========================================");
        }

        Serial.println("\n========================================");
        Serial.println("NTP TASK CYCLE COMPLETED");
        Serial.println("========================================");
        Serial.printf("Result: %s\n", syncSuccess ? "SUCCESS" : "FAILED");
        if (syncSuccess) {
            Serial.printf("Final Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                        hour(ntpTime), minute(ntpTime), second(ntpTime),
                        day(ntpTime), month(ntpTime), year(ntpTime));
            Serial.printf("Timezone: UTC%+d\n", timezoneOffset);
            Serial.printf("NTP Server: %s\n", usedServer.c_str());
        }
        Serial.println("========================================\n");
        
        ntpSyncInProgress = false;
        ntpSyncCompleted = syncSuccess;
        
        esp_task_wdt_reset();
    }
}

void webTask(void *parameter) {
  esp_task_wdt_add(NULL);
  Serial.println("\n========================================");
  Serial.println("WEB TASK STARTING");
  Serial.println("========================================");

  setupServerRoutes();
  server.begin();

  Serial.println("Web server started");
  Serial.println("Port: 80");
  Serial.println("========================================\n");

  unsigned long lastReport = 0;
  unsigned long lastAPCheck = 0;
  unsigned long lastMemCheck = 0;
  unsigned long lastStackReport = 0;
  unsigned long lastCleanup = 0;

  size_t initialHeap = ESP.getFreeHeap();
  size_t lowestHeap = initialHeap;

  while (true) {
    esp_task_wdt_reset();

    vTaskDelay(pdMS_TO_TICKS(5000));

    unsigned long now = millis();
    
    if (now - lastCleanup > 300000) {
      Serial.println("Cleaning up web server connections...");
      lastCleanup = now;
    }

    if (now - lastStackReport > 120000) {
      lastStackReport = now;
      printStackReport();
    }

    if (now - lastMemCheck > 30000) {
      lastMemCheck = now;

      size_t currentHeap = ESP.getFreeHeap();
      if (currentHeap < lowestHeap) {
        lowestHeap = currentHeap;
      }

      size_t heapDiff = initialHeap - currentHeap;

      Serial.printf("\nMEMORY STATUS:\n");
      Serial.printf("Initial: %d bytes (%.2f KB)\n", initialHeap, initialHeap / 1024.0);
      Serial.printf("Current: %d bytes (%.2f KB)\n", currentHeap, currentHeap / 1024.0);
      Serial.printf("Lowest:  %d bytes (%.2f KB)\n", lowestHeap, lowestHeap / 1024.0);
      Serial.printf("Lost:    %d bytes (%.2f KB)\n", heapDiff, heapDiff / 1024.0);

      if (heapDiff > 20480) {
        Serial.println("WARNING: Possible memory leak detected");
        Serial.println("Consider restarting if memory continues dropping");
      } else if (currentHeap < 50000) {
        Serial.println("WARNING: Low memory Consider restarting soon");
      } else {
        Serial.println("Memory stable");
      }
      Serial.println();
    }

    if (now - lastAPCheck > 5000) {
      lastAPCheck = now;

      wifi_mode_t mode;
      esp_wifi_get_mode(&mode);

      if (mode != WIFI_MODE_APSTA) {
        Serial.println("\nCRITICAL: WiFi mode changed");
        Serial.printf("Current mode: %d (should be %d)\n", mode, WIFI_MODE_APSTA);
        Serial.println("Forcing back to AP_STA...");

        WiFi.mode(WIFI_AP_STA);
        delay(100);

        WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
        WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
        delay(100);

        Serial.println("AP restored: " + String(wifiConfig.apSSID));
      }
    }
  }
}

void prayerTask(void *parameter) {
    esp_task_wdt_add(NULL);
    
    static bool hasUpdatedToday = false;
    static int lastDay = -1;
    static bool waitingForMidnightNTP = false;
    static unsigned long midnightNTPStartTime = 0;

    while (true) {
        esp_task_wdt_reset();
        
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        
        // ============================================
        // HANDLE BACKGROUND PRAYER UPDATE (dari web)
        // ============================================
        if (needPrayerUpdate && pendingPrayerLat.length() > 0 && pendingPrayerLon.length() > 0) {
            Serial.println("Memproses update waktu sholat dari web interface...");
            esp_task_wdt_reset();
            getPrayerTimesByCoordinates(pendingPrayerLat, pendingPrayerLon);
            esp_task_wdt_reset();
            
            needPrayerUpdate = false;
            pendingPrayerLat = "";
            pendingPrayerLon = "";
            
            Serial.println("Background update selesai");
        }

        // ============================================
        // MIDNIGHT UPDATE LOGIC - DENGAN NTP SYNC
        // ============================================
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int currentHour = hour(timeConfig.currentTime);
            int currentMinute = minute(timeConfig.currentTime);
            int currentDay = day(timeConfig.currentTime);
            int currentYear = year(timeConfig.currentTime);
            time_t currentTimestamp = timeConfig.currentTime;
            bool ntpSynced = timeConfig.ntpSynced;

            xSemaphoreGive(timeMutex);

            if (currentDay != lastDay) {
                hasUpdatedToday = false;
                lastDay = currentDay;
                waitingForMidnightNTP = false;
            }

            // ================================
            // DETEKSI MIDNIGHT & TRIGGER NTP
            // ================================
            if (currentHour == 0 && currentMinute < 5 && 
                !hasUpdatedToday && 
                !waitingForMidnightNTP &&
                wifiConfig.isConnected) {
                
                Serial.println("\n========================================");
                Serial.println("MIDNIGHT DETECTED - STARTING SEQUENCE");
                Serial.println("========================================");
                Serial.printf("Waktu: %02d:%02d:%02d\n", currentHour, currentMinute, second(timeConfig.currentTime));
                Serial.printf("Tanggal Sekarang: %02d/%02d/%04d\n", currentDay, month(timeConfig.currentTime), currentYear);
                Serial.println("");
                
                // ================================
                // TRIGGER NTP SYNC DULU
                // ================================
                Serial.println("Triggering NTP Sync...");
                Serial.println("Alasan: Memastikan waktu akurat sebelum update");
                
                if (ntpTaskHandle != NULL) {
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;
                    
                    xTaskNotifyGive(ntpTaskHandle);
                    
                    waitingForMidnightNTP = true;
                    midnightNTPStartTime = millis();
                    
                    Serial.println("NTP sync triggered successfully");
                    Serial.println("Menunggu NTP sync selesai...");
                    Serial.println("========================================\n");
                } else {
                    Serial.println("ERROR: NTP Task handle NULL");
                    Serial.println("Skipping midnight update");
                    Serial.println("========================================\n");
                    hasUpdatedToday = true;
                }
            }

            // ================================
            // TUNGGU NTP SYNC SELESAI
            // ================================
            if (waitingForMidnightNTP) {
                unsigned long waitTime = millis() - midnightNTPStartTime;
                const unsigned long MAX_WAIT_TIME = 30000;
                
                if (ntpSyncCompleted) {
                    Serial.println("\n========================================");
                    Serial.println("NTP SYNC COMPLETED");
                    Serial.println("========================================");
                    
                    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        currentTimestamp = timeConfig.currentTime;
                        currentYear = year(timeConfig.currentTime);
                        currentDay = day(timeConfig.currentTime);
                        int currentMonth = month(timeConfig.currentTime);
                        currentHour = hour(timeConfig.currentTime);
                        currentMinute = minute(timeConfig.currentTime);
                        int currentSecond = second(timeConfig.currentTime);
                        xSemaphoreGive(timeMutex);
                        
                        Serial.printf("Waktu Baru: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
                        Serial.printf("Tanggal Baru: %02d/%02d/%04d\n", currentDay, currentMonth, currentYear);
                        Serial.printf("Timestamp: %ld\n", currentTimestamp);
                        Serial.println("");
                    }
                    
                    // ================================
                    // UPDATE PRAYER TIMES
                    // ================================
                    if (prayerConfig.latitude.length() > 0 && 
                        prayerConfig.longitude.length() > 0) {
                        
                        if (currentYear >= 2024 && currentTimestamp >= 946684800) {
                            Serial.println("Updating Prayer Times...");
                            Serial.println("Status Waktu: VALID");
                            Serial.println("Kota: " + prayerConfig.selectedCity);
                            Serial.println("Koordinat: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                            Serial.println("");
                            
                            esp_task_wdt_reset();
                            getPrayerTimesByCoordinates(
                                prayerConfig.latitude, 
                                prayerConfig.longitude
                            );
                            esp_task_wdt_reset();
                            
                            Serial.println("\nMidnight update sequence COMPLETED");
                        } else {
                            Serial.println("WARNING: Waktu masih invalid setelah NTP");
                            Serial.printf("   Tahun: %d (min: 2024)\n", currentYear);
                            Serial.printf("   Timestamp: %ld (min: 946684800)\n", currentTimestamp);
                            Serial.println("   Menggunakan waktu sholat yang sudah ada");
                        }
                    } else {
                        Serial.println("WARNING: Tidak ada koordinat kota");
                        Serial.println("   Menggunakan waktu sholat yang sudah ada");
                    }
                    
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else if (waitTime > MAX_WAIT_TIME) {
                    Serial.println("\n========================================");
                    Serial.println("NTP SYNC TIMEOUT");
                    Serial.println("========================================");
                    Serial.printf("Waktu tunggu: %lu ms (max: %lu ms)\n", waitTime, MAX_WAIT_TIME);
                    Serial.println("Status NTP:");
                    Serial.printf("   ntpSyncInProgress: %s\n", ntpSyncInProgress ? "true" : "false");
                    Serial.printf("   ntpSyncCompleted: %s\n", ntpSyncCompleted ? "false" : "false");
                    Serial.println("");
                    Serial.println("Keputusan: Menggunakan waktu sholat yang sudah ada");
                    Serial.println("Tidak melakukan update (waktu mungkin tidak akurat)");
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else {
                    if (waitTime % 5000 < 1000) {
                        Serial.printf("Menunggu NTP sync... (%lu/%lu ms)\n", 
                                     waitTime, MAX_WAIT_TIME);
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void rtcSyncTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(60000); // Every 1 minute
    
    Serial.println("\n========================================");
    Serial.println("RTC SYNC TASK STARTED");
    Serial.println("========================================");
    Serial.println("Function: Sync system time FROM RTC");
    Serial.println("Interval: Every 1 minute");
    Serial.println("Purpose: Backup time source when WiFi unavailable");
    Serial.println("========================================\n");
    
    while (true) {
        if (rtcAvailable) {
            // ================================
            // CEK RTC VALID DULU
            // ================================
            if (!isRTCValid()) {
                Serial.println("\n[RTC Sync] Skipped - RTC invalid");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            // ================================
            // BACA RTC TIME
            // ================================
            DateTime rtcTime;
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                rtcTime = rtc.now();
                xSemaphoreGive(i2cMutex);
            } else {
                Serial.println("\n[RTC Sync] Skipped - Cannot acquire I2C mutex");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            // ================================
            // VALIDASI RTC TIME
            // ================================
            if (!isRTCTimeValid(rtcTime)) {
                Serial.println("\n[RTC Sync] Skipped - RTC time invalid");
                Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                             rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                             rtcTime.day(), rtcTime.month(), rtcTime.year());
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            // ================================
            // COMPARE DENGAN SYSTEM TIME
            // ================================
            time_t systemTime;
            bool ntpSynced;
            
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                systemTime = timeConfig.currentTime;
                ntpSynced = timeConfig.ntpSynced;
                xSemaphoreGive(timeMutex);
            } else {
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            time_t rtcUnix = rtcTime.unixtime();
            int timeDiff = abs(systemTime - rtcUnix);
            
            // ================================
            // DECISION LOGIC
            // ================================
            bool shouldSync = false;
            String reason = "";
            
            if (timeDiff <= 2) {
            } else if (!ntpSynced && timeDiff > 2) {
                shouldSync = true;
                reason = "NTP not synced, using RTC as primary source";
                
            } else if (ntpSynced && systemTime > rtcUnix) {
                shouldSync = false;
                reason = "System time newer (from NTP), skip RTC sync";
                
                Serial.println("\n[RTC Sync] Skipped");
                Serial.println("Reason: " + reason);
                Serial.printf("   System: %02d:%02d:%02d %02d/%02d/%04d (from NTP)\n",
                             hour(systemTime), minute(systemTime), second(systemTime),
                             day(systemTime), month(systemTime), year(systemTime));
                Serial.printf("   RTC:    %02d:%02d:%02d %02d/%02d/%04d (older)\n",
                             rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                             rtcTime.day(), rtcTime.month(), rtcTime.year());
                Serial.println("   Action: RTC will be updated on next NTP sync\n");
                
            } else {
                shouldSync = true;
                reason = "RTC time more accurate, correcting system time";
            }
            
            // ================================
            // SYNC SYSTEM TIME DARI RTC
            // ================================
            if (shouldSync) {
                if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
                    timeConfig.currentTime = rtcUnix;
                    setTime(rtcUnix);
                    xSemaphoreGive(timeMutex);
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                    
                    Serial.println("\n========================================");
                    Serial.println("SYSTEM TIME SYNCED FROM RTC");
                    Serial.println("========================================");
                    Serial.println("Reason: " + reason);
                    Serial.printf("Old System: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 hour(systemTime), minute(systemTime), second(systemTime),
                                 day(systemTime), month(systemTime), year(systemTime));
                    Serial.printf("New System: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                 rtcTime.day(), rtcTime.month(), rtcTime.year());
                    Serial.printf("Time diff: %d seconds\n", timeDiff);
                    Serial.println("========================================\n");
                }
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void clockTickTask(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);
    
    static int autoSyncCounter = 0; 
    
    const time_t EPOCH_2000 = 946684800;
    
    while (true) {
        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // ================================
            // Prevent timestamp sebelum 01/01/2000
            // ================================
            if (timeConfig.currentTime < EPOCH_2000) { 
                Serial.println("\nCLOCK TASK WARNING:");
                Serial.printf("  Invalid timestamp detected: %ld\n", timeConfig.currentTime);
                Serial.println("  This is before 01/01/2000 00:00:00");
                Serial.println("  Forcing reset to: 01/01/2000 00:00:00");
                
                // RESET KE 01/01/2000 00:00:00
                setTime(0, 0, 0, 1, 1, 2000);
                timeConfig.currentTime = now();
                
                if (timeConfig.currentTime < EPOCH_2000) {
                    Serial.println("TimeLib.h issue - using hardcoded timestamp");
                    timeConfig.currentTime = EPOCH_2000; // 946684800
                }
                
                Serial.printf("Time corrected to: %ld (01/01/2000 00:00:00)\n\n", 
                             timeConfig.currentTime);
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
                    Serial.println("\nAuto NTP sync (hourly)");
                    xTaskNotifyGive(ntpTaskHandle);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================
// WiFi & AP Restart Tasks
// ============================================
void restartWiFiTask(void *parameter) {
    // ============================================
    // DEBOUNCING
    // ============================================
    unsigned long now = millis();
    if (now - lastWiFiRestartRequest < RESTART_DEBOUNCE_MS) {
        unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastWiFiRestartRequest);
        
        Serial.println("\n========================================");
        Serial.println("WiFi RESTART REJECTED - TOO FAST");
        Serial.println("========================================");
        Serial.printf("Reason: Last restart was %lu ms ago\n", now - lastWiFiRestartRequest);
        Serial.printf("Minimum interval: %lu ms\n", RESTART_DEBOUNCE_MS);
        Serial.printf("Please wait: %lu ms (%.1f seconds)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    lastWiFiRestartRequest = now;
    
    // ============================================
    // LOCK
    // ============================================
    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
        if (wifiRestartMutex == NULL) {
            Serial.println("ERROR: Failed to create wifiRestartMutex");
            vTaskDelete(NULL);
            return;
        }
    }
    
    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("\n========================================");
        Serial.println("WiFi RESTART BLOCKED");
        Serial.println("========================================");
        Serial.println("Reason: Another WiFi/AP restart in progress");
        Serial.println("Action: Request ignored for safety");
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    if (wifiRestartInProgress || apRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("WiFi RESTART ABORTED");
        Serial.println("========================================");
        Serial.printf("WiFi restart in progress: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("AP restart in progress: %s\n", apRestartInProgress ? "YES" : "NO");
        Serial.println("========================================\n");
        
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    wifiRestartInProgress = true;
    
    Serial.println("\n========================================");
    Serial.println("SAFE WiFi RESTART SEQUENCE STARTED");
    Serial.println("========================================");
    Serial.println("Protection: Debouncing + Mutex Lock Active");
    Serial.println("Mode: Safe reconnect (no mode switching)");
    Serial.println("========================================\n");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // ============================================
    // SIMPAN KREDENSIAL DULU
    // ============================================
    Serial.println("Preparing for reconnect...");
    
    String ssid, password;
    if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ssid = wifiConfig.routerSSID;
        password = wifiConfig.routerPassword;
        wifiConfig.isConnected = false;
        wifiState = WIFI_IDLE;
        reconnectAttempts = 0;
        xSemaphoreGive(wifiMutex);
        
        Serial.println("   Credentials loaded from memory");
        Serial.println("   SSID: " + ssid);
        Serial.println("   Connection state reset");
    } else {
        Serial.println("   ERROR: Failed to acquire wifiMutex");
        wifiRestartInProgress = false;
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    // ============================================
    // DISCONNECT DENGAN AMAN
    // ============================================
    Serial.println("\nDisconnecting old WiFi...");
    WiFi.disconnect(false, false); // Keep config, don't erase
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("   Disconnected (config preserved)");
    
    // ============================================
    // CEK & RESTORE AP JIKA MATI
    // ============================================
    Serial.println("\nVerifying AP status...");
    
    IPAddress apIP = WiFi.softAPIP();
    if (apIP == IPAddress(0, 0, 0, 0)) {
        Serial.println("   WARNING: AP died during disconnect");
        Serial.println("   Restoring AP...");
        
        WiFi.softAPdisconnect(false);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
        bool apStarted = WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (apStarted) {
            Serial.println("   AP restored successfully");
            Serial.println("   AP IP: " + WiFi.softAPIP().toString());
        } else {
            Serial.println("   ERROR: Failed to restore AP");
        }
    } else {
        Serial.println("   AP still alive: " + apIP.toString());
    }
    
    // ============================================
    // RECONNECT WiFi
    // ============================================
    if (ssid.length() > 0) {
        Serial.println("\nReconnecting to WiFi...");
        Serial.println("   Target SSID: " + ssid);
        Serial.println("   Initiating connection...");
        
        WiFi.begin(ssid.c_str(), password.c_str());
        
        Serial.println("\n========================================");
        Serial.println("WiFi RECONNECT INITIATED");
        Serial.println("========================================");
        Serial.println("Status: Connection request sent");
        Serial.println("Monitor: WiFi Task will handle connection");
        Serial.println("Expected: See WiFi events in serial log");
        Serial.println("========================================\n");
    } else {
        Serial.println("\n========================================");
        Serial.println("WiFi RECONNECT FAILED");
        Serial.println("========================================");
        Serial.println("Reason: No SSID configured in memory");
        Serial.println("Action: Configure WiFi via web interface");
        Serial.println("========================================\n");
    }
    
    // Delay sebelum release lock
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // ============================================
    // RELEASE LOCK & CLEANUP
    // ============================================
    wifiRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);
    
    Serial.println("WiFi restart sequence completed");
    Serial.println("Lock released - system ready for next request\n");
    
    vTaskDelete(NULL);
}

// ============================================
// WiFi & AP Restart Tasks
void restartAPTask(void *parameter) {
    // Lock check (tetap)
    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
    }
    
    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("AP RESTART BLOCKED");
        vTaskDelete(NULL);
        return;
    }
    
    if (apRestartInProgress || wifiRestartInProgress) {
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    apRestartInProgress = true;
    
    Serial.println("\n========================================");
    Serial.println("AP RESTART TASK STARTED");
    Serial.println("========================================");
    Serial.println("Waiting for countdown to complete (60 seconds)...");
    Serial.println("========================================\n");
    
    // TUNGGU 60 DETIK
    for (int i = 60; i > 0; i--) {
        if (i % 10 == 0 || i <= 5) {
            Serial.printf("Countdown: %d seconds remaining...\n", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    Serial.println("\n========================================");
    Serial.println("COUNTDOWN COMPLETED - RESTARTING AP NOW");
    Serial.println("========================================\n");

    delay(500);
    
    // ============================================
    // FORCE DISCONNECT ALL CLIENTS - AGGRESSIVE MODE
    // ============================================
    Serial.println("Force disconnecting all AP clients...");
    
    int clientsBefore = WiFi.softAPgetStationNum();
    Serial.printf("Clients connected: %d\n", clientsBefore);
    
    if (clientsBefore > 0) {
        Serial.println("\nStep 1/4: Sending deauth frames to all clients...");
        esp_wifi_deauth_sta(0);  // 0 = broadcast to all clients
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        int afterDeauth = WiFi.softAPgetStationNum();
        Serial.printf("After deauth: %d clients\n", afterDeauth);
        
        Serial.println("\nStep 2/4: Soft disconnect AP...");
        WiFi.softAPdisconnect(true);
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        int afterSoftDisconnect = WiFi.softAPgetStationNum();
        Serial.printf("After soft disconnect: %d clients\n", afterSoftDisconnect);
        
        if (afterSoftDisconnect > 0) {
            Serial.println("\nStep 3/4: Hard reset - WiFi mode switch...");
            Serial.printf("%d stubborn clients detected\n", afterSoftDisconnect);
            
            // Mode switch untuk force reset
            WiFi.mode(WIFI_STA);
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            int afterModeSwitch = WiFi.softAPgetStationNum();
            Serial.printf("After mode switch: %d clients\n", afterModeSwitch);
            
            WiFi.mode(WIFI_AP_STA);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        Serial.println("\nStep 4/4: Final verification...");
        int finalCheck = WiFi.softAPgetStationNum();
        Serial.printf("Final client count: %d\n", finalCheck);
        
        if (finalCheck > 0) {
            Serial.println("\nWARNING: Some clients still detected");
            Serial.println("This is normal - ghost entries will clear");
            Serial.println("Proceeding with AP restart...");
        } else {
            Serial.println("\nSUCCESS: All clients disconnected");
        }
        
    } else {
        Serial.println("No clients connected - quick disconnect");
        WiFi.softAPdisconnect(true);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    
    Serial.println("\n========================================");
    Serial.println("AP SHUTDOWN COMPLETE");
    Serial.println("========================================");
    Serial.println("Old AP is now offline");
    Serial.println("All clients forcefully disconnected");
    Serial.println("========================================\n");
    
    // ============================================
    // LOAD NEW CONFIGURATION
    // ============================================
    char savedSSID[33];
    char savedPassword[65];
    IPAddress savedAPIP, savedGateway, savedSubnet;
    
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strncpy(savedSSID, wifiConfig.apSSID, sizeof(savedSSID));
        strncpy(savedPassword, wifiConfig.apPassword, sizeof(savedPassword));
        savedAPIP = wifiConfig.apIP;
        savedGateway = wifiConfig.apGateway;
        savedSubnet = wifiConfig.apSubnet;
        xSemaphoreGive(settingsMutex);
        
        Serial.println("New configuration loaded:");
        Serial.println("  SSID: " + String(savedSSID));
        Serial.println("  IP: " + savedAPIP.toString());
    } else {
        Serial.println("ERROR: Cannot load config - using defaults");
        strncpy(savedSSID, DEFAULT_AP_SSID, sizeof(savedSSID));
        strncpy(savedPassword, DEFAULT_AP_PASSWORD, sizeof(savedPassword));
        savedAPIP = IPAddress(192, 168, 4, 1);
        savedGateway = IPAddress(192, 168, 4, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);
    }
    
    // ============================================
    // START NEW AP
    // ============================================
    Serial.println("\nConfiguring new AP network...");
    bool configSuccess = WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
    
    if (!configSuccess) {
        Serial.println("WARNING: softAPConfig failed - using defaults");
        savedAPIP = IPAddress(192, 168, 4, 1);
        savedGateway = IPAddress(192, 168, 4, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);
        WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    Serial.println("Starting new AP broadcast...");
    bool apStarted = WiFi.softAP(savedSSID, savedPassword);
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // ============================================
    // VERIFY NEW AP
    // ============================================
    if (apStarted) {
        IPAddress newAPIP = WiFi.softAPIP();
        String currentSSID = WiFi.softAPSSID();
        
        Serial.println("\n========================================");
        Serial.println("AP RESTART SUCCESS");
        Serial.println("========================================");
        Serial.println("SSID: \"" + currentSSID + "\" (ACTIVE)");
        Serial.println("IP: " + newAPIP.toString());
        Serial.println("MAC: " + WiFi.softAPmacAddress());
        Serial.println("");
        Serial.println("CLIENT ACTION REQUIRED:");
        Serial.println("  1. Search for: \"" + currentSSID + "\"");
        Serial.println("  2. Connect with password");
        Serial.println("  3. Open: http://" + newAPIP.toString());
        Serial.println("========================================\n");
        
    } else {
        Serial.println("\n========================================");
        Serial.println("AP RESTART FAILED");
        Serial.println("========================================");
        Serial.println("Rolling back to default AP...");
        
        strcpy(savedSSID, DEFAULT_AP_SSID);
        strcpy(savedPassword, DEFAULT_AP_PASSWORD);
        savedAPIP = IPAddress(192, 168, 4, 1);
        savedGateway = IPAddress(192, 168, 4, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);
        
        WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        WiFi.softAP(savedSSID, savedPassword);
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        Serial.println("Rollback to: " + String(savedSSID));
        Serial.println("========================================\n");
    }
    
    // ============================================
    // FINAL STABILIZATION
    // ============================================
    Serial.println("Waiting for AP to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    IPAddress finalIP = WiFi.softAPIP();
    int finalClients = WiFi.softAPgetStationNum();
    
    Serial.println("\n========================================");
    Serial.println("FINAL VERIFICATION");
    Serial.println("========================================");
    Serial.println("AP Status: " + String(finalIP != IPAddress(0,0,0,0) ? "ACTIVE ✅" : "DEAD ❌"));
    Serial.println("SSID: " + WiFi.softAPSSID());
    Serial.println("IP: " + finalIP.toString());
    Serial.println("Connected Clients: " + String(finalClients));
    
    if (finalClients > 0) {
        Serial.println("\n" + String(finalClients) + " client(s) auto-reconnected!");
    } else {
        Serial.println("\nNo clients yet - waiting for manual reconnection");
    }
    Serial.println("========================================\n");
    
    // ============================================
    // CLEANUP
    // ============================================
    apRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);
    
    Serial.println("AP restart task completed\n");
    vTaskDelete(NULL);
}

// ================================
// SETUP - ESP32 CORE 3.x
// ================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("ESP32 Islamic Prayer Clock");
  Serial.println("LVGL 9.2.0 + FreeRTOS");
  Serial.println("CONCURRENT ACCESS OPTIMIZED");
  Serial.println("VERSION 2.1 - MULTI-CLIENT");
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
  i2cMutex = xSemaphoreCreateMutex();

  displayQueue = xQueueCreate(20, sizeof(DisplayUpdate));

  Serial.println("Semaphores & Queue created");

  // ================================
  // LITTLEFS & LOAD SETTINGS
  // ================================

  if (wifiConfig.apIP == IPAddress(0, 0, 0, 0)) {
    wifiConfig.apIP = IPAddress(192, 168, 4, 1);
    wifiConfig.apGateway = IPAddress(192, 168, 4, 1);
    wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);
  }

  init_littlefs();
  loadWiFiCredentials();
  loadPrayerTimes();
  loadCitySelection();
  loadMethodSelection();
  loadTimezoneConfig();
  loadBuzzerConfig();

  Wire.begin(/*RTC_SDA, RTC_SCL*/);
  delay(500);

  Wire.beginTransmission(0x68);
  Wire.endTransmission();

  // ================================
  // RTC DS3231 INIT
  // ================================
  rtcAvailable = initRTC();

  if (rtcAvailable) {
      Serial.println("\nRTC is available");
      Serial.println("Time loaded from RTC successfully");
      Serial.println("Time will persist across restarts");
  } else {
      Serial.println("\nRTC not available - time will reset on restart");
      
      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
          const time_t EPOCH_2000 = 946684800;
          setTime(0, 0, 0, 1, 1, 2000);
          timeConfig.currentTime = EPOCH_2000;
          
          if (timeConfig.currentTime < EPOCH_2000) {
              timeConfig.currentTime = EPOCH_2000;
          }
          
          xSemaphoreGive(timeMutex);
          Serial.printf("Initial time set to: %ld (01/01/2000 00:00:00 UTC)\n", EPOCH_2000);
      }
  }

  // ================================
  // TOUCH INIT
  // ================================
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  Serial.println("Touch initialized");

  ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcWrite(BUZZER_CHANNEL, 0); // Mati dulu
  Serial.println("Buzzer initialized (GPIO26)");

  // ================================
  // LVGL INIT
  // ================================
  lv_init();
  lv_tick_set_cb([]() {
    return (uint32_t)millis();
  });

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

  setupWiFiEvents();
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  // ================================
  // SET HOSTNAME SEBELUM OPERASI WIFI LAINNYA
  // ================================
  WiFi.setHostname("JWS-Indonesia");
  delay(200);
  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());

  WiFi.setSleep(WIFI_PS_NONE);

  esp_wifi_set_ps(WIFI_PS_NONE);

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_max_tx_power(78);

  WiFi.setAutoReconnect(true);

  WiFi.persistent(false);

  Serial.println("WiFi Mode: AP + STA");
  Serial.println("WiFi Sleep: DOUBLE DISABLED");
  Serial.println("Arduino: WIFI_PS_NONE");
  Serial.println("ESP-IDF: WIFI_PS_NONE");
  Serial.println("WiFi Power: Maximum (19.5dBm)");
  Serial.println("Auto Reconnect: Enabled");
  Serial.println("Persistent: Disabled");
  Serial.println("========================================\n");

  WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
  WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
  delay(100);

  Serial.printf("AP Started: %s\n", wifiConfig.apSSID);
  Serial.printf("Password: %s\n", wifiConfig.apPassword);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());

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
    Serial.println("City: " + prayerConfig.selectedCity);
    Serial.println("Imsak: " + prayerConfig.imsakTime);
    Serial.println("Subuh: " + prayerConfig.subuhTime);
    Serial.println("Terbit: " + prayerConfig.terbitTime);
    Serial.println("Zuhur: " + prayerConfig.zuhurTime);
    Serial.println("Ashar: " + prayerConfig.asharTime);
    Serial.println("Maghrib: " + prayerConfig.maghribTime);
    Serial.println("Isya: " + prayerConfig.isyaTime);

    DisplayUpdate update;
    update.type = DisplayUpdate::PRAYER_UPDATE;
    xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
  } else {
    Serial.println("\nNo city selected");
    Serial.println("Please select city via web interface");
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
  Serial.println("UI Task (Core 1)");

  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFi",
    WIFI_TASK_STACK_SIZE,
    NULL,
    WIFI_TASK_PRIORITY,
    &wifiTaskHandle,
    0  // Core 0
  );
  Serial.println("WiFi Task (Core 0)");

  xTaskCreatePinnedToCore(
    ntpTask,
    "NTP",
    NTP_TASK_STACK_SIZE,
    NULL,
    NTP_TASK_PRIORITY,
    &ntpTaskHandle,
    0  // Core 0
  );
  Serial.println("NTP Task (Core 0)");

  xTaskCreatePinnedToCore(
    webTask,
    "Web",
    WEB_TASK_STACK_SIZE,
    NULL,
    WEB_TASK_PRIORITY,
    &webTaskHandle,
    0  // Core 0
  );
  Serial.println("Web Task (Core 0)");

  xTaskCreatePinnedToCore(
    prayerTask,
    "Prayer",
    PRAYER_TASK_STACK_SIZE,
    NULL,
    PRAYER_TASK_PRIORITY,
    &prayerTaskHandle,
    0  // Core 0
  );
  Serial.println("Prayer Task (Core 0)");

  if (prayerTaskHandle) {
      esp_task_wdt_add(prayerTaskHandle);
      Serial.print("Prayer Task WDT registered");
  }

  xTaskCreatePinnedToCore(
    clockTickTask,
    "Clock",
    CLOCK_TASK_STACK_SIZE,
    NULL,
    CLOCK_TASK_PRIORITY,
    NULL,
    0  // Core 0
  );
  Serial.println("Clock Task (Core 0)");

  // ================================
  // RTC SYNC TASK
  // ================================
  if (rtcAvailable) {
    xTaskCreatePinnedToCore(
      rtcSyncTask,
      "RTC Sync",
      RTC_TASK_STACK_SIZE, 
      NULL,
      RTC_TASK_PRIORITY,
      &rtcTaskHandle,
      0  // Core 0
    );
    Serial.println("RTC Sync Task (Core 0)");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  // ================================
  // REGISTER TASKS TO WATCHDOG
  // ================================
  if (wifiTaskHandle) {
    esp_task_wdt_add(wifiTaskHandle);
    Serial.println("WiFi Task WDT");
  }
  if (webTaskHandle) {
    esp_task_wdt_add(webTaskHandle);
    Serial.println("Web Task WDT");
  }

  if (ntpTaskHandle) {
    esp_task_wdt_add(ntpTaskHandle);
    Serial.println("NTP Task WDT");
  }

  Serial.println("All tasks started\n");

  // ================================
  // STARTUP COMPLETE
  // ================================
  Serial.println("========================================");
  Serial.println("SYSTEM READY");
  Serial.println("========================================");
  Serial.println(" Multi-client concurrent access enabled");
  Serial.println(" WiFi sleep disabled for better response");
  Serial.println("========================================\n");

  if (wifiConfig.routerSSID.length() > 0) {
    Serial.println("WiFi configured, will auto-connect...");
    Serial.println("SSID: " + wifiConfig.routerSSID);
  } else {
    Serial.println("Connect to AP to configure:");
    Serial.println("1. WiFi: " + String(wifiConfig.apSSID));
    Serial.println("2. Password: " + String(wifiConfig.apPassword));
    Serial.println("3. Browser: http://192.168.4.1");
    Serial.println("4. Set WiFi & select city");
  }

  if (prayerConfig.selectedCity.length() == 0) {
    Serial.println("\nREMINDER: Select city via web interface");
  }

  Serial.println("\nBoot complete - Ready for connections");
  Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
