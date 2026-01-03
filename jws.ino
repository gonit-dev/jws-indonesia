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
#include "driver/i2s.h"
#include "SD.h"

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

// Pin Audio PCM5012A
#define I2S_BCLK 25
#define I2S_LRC  32
#define I2S_DOUT 33

// Pin SD Card
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_CLK   18

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 1024
#define AUDIO_VOLUME 70

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
#define UI_TASK_STACK_SIZE 12288       // LVGL + EEZ rendering
#define WIFI_TASK_STACK_SIZE 3072      // Event-driven + reconnect (small headroom)
#define NTP_TASK_STACK_SIZE 4096       // Hanya NTP sync
#define WEB_TASK_STACK_SIZE 5120       // AsyncWebServer + concurrent requests
#define PRAYER_TASK_STACK_SIZE 16384   // HTTP + JSON parsing
#define RTC_TASK_STACK_SIZE 2048       // Simple I2C
#define CLOCK_TASK_STACK_SIZE 2048     // Simple time increment
#define AUDIO_TASK_STACK_SIZE 4096     // Audio adzan

// Task Priorities (0 = lowest, higher number = higher priority)
#define UI_TASK_PRIORITY 3             // Highest (display responsiveness)
#define WIFI_TASK_PRIORITY 2           // High (network stability)
#define NTP_TASK_PRIORITY 2            // High (time sync)
#define WEB_TASK_PRIORITY 1            // Low (background web server)
#define PRAYER_TASK_PRIORITY 1         // Low (daily update)
#define RTC_TASK_PRIORITY 1            // Low (backup sync)
#define CLOCK_TASK_PRIORITY 2          // High (time accuracy)
#define AUDIO_TASK_PRIORITY 0          // Low (Audio adzan)

// Task Handles
TaskHandle_t rtcTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t ntpTaskHandle = NULL;
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t prayerTaskHandle = NULL;
TaskHandle_t clockTaskHandle = NULL;
TaskHandle_t buzzerTestTaskHandle = NULL; 

// ================================
// SEMAPHORES & MUTEXES
// ================================
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t timeMutex;
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t settingsMutex;
SemaphoreHandle_t spiMutex;
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t sdMutex; 

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
  String subuhTime = "00:00";
  String terbitTime = "00:00";
  String zuhurTime = "00:00";
  String asharTime = "00:00";
  String maghribTime = "00:00";
  String isyaTime = "00:00";
  String imsakTime = "00:00";
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

struct AdzanState {
  bool isPlaying;
  String currentPrayer;
  time_t startTime;
  time_t deadlineTime;
  bool canTouch;
};

AdzanState adzanState = {false, "", 0, 0, false};
SemaphoreHandle_t audioMutex = NULL;
TaskHandle_t audioTaskHandle = NULL;

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
const unsigned long WIFI_CHECK_INTERVAL = 5000;
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
unsigned long wifiDisconnectedTime = 0;

bool fastReconnectMode = false;
unsigned long lastFastScan = 0;
const unsigned long FAST_SCAN_INTERVAL = 3000;

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

TaskHandle_t restartTaskHandle = NULL;
TaskHandle_t resetTaskHandle = NULL;

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

const unsigned long BLINK_DURATION = 60000;
const unsigned long BLINK_INTERVAL = 500;

// ================================
// WIFI RESTART PROTECTION - TAMBAHAN BARU
// ================================
static SemaphoreHandle_t wifiRestartMutex = NULL;
static bool wifiRestartInProgress = false;
static bool apRestartInProgress = false;

// Debouncing timestamps
static unsigned long lastWiFiRestartRequest = 0;
static unsigned long lastAPRestartRequest = 0;
const unsigned long RESTART_DEBOUNCE_MS = 3000;

// ============================================
// FORWARD DECLARATIONS
// ============================================

// ============================================
// Display & UI Functions
// ============================================
void updateCityDisplay();
void updateTimeDisplay();
void updatePrayerDisplay();
void hideAllUIElements();
void showAllUIElements();

// ============================================
// Countdown Helper Functions
// ============================================
void startCountdown(String reason, String message, int seconds);
void stopCountdown();
int getRemainingSeconds();

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
void saveAdzanState();
void loadAdzanState();
int getAdzanRemainingSeconds();
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
void sendJSONResponse(AsyncWebServerRequest *request, const String &json);

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
void audioTask(void *parameter);

// ============================================
// WiFi & AP Restart Tasks
// ============================================
void restartWiFiTask(void *parameter);
void restartAPTask(void *parameter);

// ============================================
// Audio Functions
// ============================================
bool initI2S();
bool initSDCard();
bool isAudioFileValid(String filepath);
bool playWAV(String filepath);
void stopAudio();

// ============================================
// Display & UI Functions
// ============================================
void updateCityDisplay() {
  char displayText[64] = "--";
  
  if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (prayerConfig.selectedCity.length() > 0) {
      size_t len = prayerConfig.selectedCity.length();
      if (len >= sizeof(displayText)) {
        len = sizeof(displayText) - 1;
      }
      
      strncpy(displayText, prayerConfig.selectedCity.c_str(), len);
      displayText[len] = '\0';
      
      char* pos = strstr(displayText, "Kabupaten ");
      if (pos) {
        memmove(pos + 4, pos + 10, strlen(pos + 10) + 1);
        memcpy(pos, "Kab ", 4);
      }
      
      pos = strstr(displayText, "District ");
      if (pos) {
        memmove(pos + 5, pos + 9, strlen(pos + 9) + 1);
        memcpy(pos, "Dist ", 5);
      }
    }

    if (objects.city_time) {
      lv_label_set_text(objects.city_time, displayText);
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

// ============================================
// Prayer Time Blink Functions
// ============================================
void checkPrayerTime() {
  time_t now_t = timeConfig.currentTime;
  struct tm timeinfo;
  localtime_r(&now_t, &timeinfo);
  
  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  
  if (timeinfo.tm_sec == 0 && !blinkState.isBlinking && !adzanState.canTouch) {
    String current = String(currentTime);
    String prayerName = "";
    
    if (current == prayerConfig.imsakTime && buzzerConfig.imsakEnabled) prayerName = "imsak";
    else if (current == prayerConfig.subuhTime && buzzerConfig.subuhEnabled) prayerName = "subuh";
    else if (current == prayerConfig.terbitTime && buzzerConfig.terbitEnabled) prayerName = "terbit";
    else if (current == prayerConfig.zuhurTime && buzzerConfig.zuhurEnabled) prayerName = "zuhur";
    else if (current == prayerConfig.asharTime && buzzerConfig.asharEnabled) prayerName = "ashar";
    else if (current == prayerConfig.maghribTime && buzzerConfig.maghribEnabled) prayerName = "maghrib";
    else if (current == prayerConfig.isyaTime && buzzerConfig.isyaEnabled) prayerName = "isya";
    
    if (prayerName.length() > 0) {
      startBlinking(prayerName);
      
      adzanState.canTouch = true;
      adzanState.currentPrayer = prayerName;
      adzanState.startTime = now_t;
      adzanState.deadlineTime = now_t + 600;
      saveAdzanState();
      
      Serial.println("ADZAN AKTIF: " + prayerName + " (10 menit)");
    }
  }
  
  if (adzanState.canTouch && getAdzanRemainingSeconds() <= 0) {
    Serial.println("ADZAN EXPIRED: " + adzanState.currentPrayer);
    adzanState.canTouch = false;
    adzanState.currentPrayer = "";
    saveAdzanState();
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
  Serial.print("PRAYER TIME ENTER: ");
  Serial.println(upperName);
  Serial.println("========================================");
  Serial.println("Starting to blink for 1 minute...");
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
    
    Serial.println("Flashing finished - all prayer times appear normal");
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
  esp_task_wdt_reset();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - keeping existing prayer times");
    return;
  }

  UBaseType_t stackBefore = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("\n[Prayer API] Stack available: %d bytes\n", stackBefore * 4);
  
  if (stackBefore < 2000) {
    Serial.println("ERROR: Insufficient stack for HTTP request!");
    Serial.println("Aborting prayer times update to prevent crash");
    return;
  }

  time_t now_t;
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    now_t = timeConfig.currentTime;
    xSemaphoreGive(timeMutex);
  } else {
    now_t = time(nullptr);
  }

  if (now_t < 946684800) {
    Serial.println("\nPRAYER TIMES UPDATE BLOCKED");
    Serial.println("Reason: Invalid system time");
    return;
  }

  char dateStr[12];
  sprintf(dateStr, "%02d-%02d-%04d", day(now_t), month(now_t), year(now_t));

  int currentMethod = methodConfig.methodId;
  String url = "http://api.aladhan.com/v1/timings/" + String(dateStr) + 
               "?latitude=" + lat + "&longitude=" + lon + "&method=" + String(currentMethod);

  Serial.println("\nFetching prayer times...");
  Serial.println("Date: " + String(dateStr));
  Serial.println("URL: " + url);

  HTTPClient http;
  WiFiClient client;

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.begin(client, url);
  http.setTimeout(15000);

  esp_task_wdt_reset();

  int httpResponseCode = http.GET();
  Serial.println("Response code: " + String(httpResponseCode));

  esp_task_wdt_reset();

  if (httpResponseCode == 200) {
    String payload = http.getString();

    esp_task_wdt_reset();
    
    UBaseType_t stackAfterHTTP = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("[Prayer API] Stack after HTTP: %d bytes\n", stackAfterHTTP * 4);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    esp_task_wdt_reset();

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
      if (tempSubuh.length() != 5 || tempSubuh.indexOf(':') != 2) allValid = false;
      if (tempTerbit.length() != 5 || tempTerbit.indexOf(':') != 2) allValid = false;
      if (tempZuhur.length() != 5 || tempZuhur.indexOf(':') != 2) allValid = false;
      if (tempAshar.length() != 5 || tempAshar.indexOf(':') != 2) allValid = false;
      if (tempMaghrib.length() != 5 || tempMaghrib.indexOf(':') != 2) allValid = false;
      if (tempIsya.length() != 5 || tempIsya.indexOf(':') != 2) allValid = false;
      if (tempImsak.length() != 5 || tempImsak.indexOf(':') != 2) allValid = false;

      if (allValid) {
        prayerConfig.subuhTime = tempSubuh;
        prayerConfig.terbitTime = tempTerbit;
        prayerConfig.zuhurTime = tempZuhur;
        prayerConfig.asharTime = tempAshar;
        prayerConfig.maghribTime = tempMaghrib;
        prayerConfig.isyaTime = tempIsya;
        prayerConfig.imsakTime = tempImsak;

        Serial.println("\nPrayer times updated successfully");
        savePrayerTimes();

        DisplayUpdate update;
        update.type = DisplayUpdate::PRAYER_UPDATE;
        xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
      } else {
        Serial.println("Invalid prayer times data");
      }
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("HTTP request failed: %d\n", httpResponseCode);
  }

  http.end();
  client.stop();
  
  // FINAL STACK CHECK
  UBaseType_t stackAfter = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("[Prayer API] Stack after cleanup: %d bytes\n", stackAfter * 4);
  
  esp_task_wdt_reset();
  
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
  
  vTaskDelay(pdMS_TO_TICKS(50));
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

void saveAdzanState() {
  fs::File file = LittleFS.open("/adzan_state.txt", "w");
  if (file) {
    file.println(adzanState.currentPrayer);
    file.println(adzanState.canTouch ? "1" : "0");
    file.println((unsigned long)adzanState.startTime);
    file.println((unsigned long)adzanState.deadlineTime);
    file.close();
  }
}

void loadAdzanState() {
  if (!LittleFS.exists("/adzan_state.txt")) return;
  
  fs::File file = LittleFS.open("/adzan_state.txt", "r");
  if (!file) return;
  
  adzanState.currentPrayer = file.readStringUntil('\n');
  adzanState.currentPrayer.trim();
  
  String touchStr = file.readStringUntil('\n');
  touchStr.trim();
  
  String startStr = file.readStringUntil('\n');
  startStr.trim();
  adzanState.startTime = (time_t)startStr.toInt();
  
  String deadlineStr = file.readStringUntil('\n');
  deadlineStr.trim();
  adzanState.deadlineTime = (time_t)deadlineStr.toInt();
  
  file.close();
  
  if (touchStr == "1" && adzanState.currentPrayer.length() > 0) {
    time_t now = timeConfig.currentTime;
    int remaining = getAdzanRemainingSeconds();
    
    if (remaining > 0) {
      adzanState.canTouch = true;
      Serial.println("ADZAN RESTORED: " + adzanState.currentPrayer);
      Serial.printf("Sisa: %d detik (%d menit)\n", remaining, remaining/60);
    } else {
      adzanState.canTouch = false;
      adzanState.currentPrayer = "";
      Serial.println("ADZAN EXPIRED");
    }
  }
}

int getAdzanRemainingSeconds() {
  if (!adzanState.canTouch) return 0;
  int remaining = (int)(adzanState.deadlineTime - timeConfig.currentTime);
  return remaining > 0 ? remaining : 0;
}

void saveCitySelection() {
    if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        fs::File file = LittleFS.open("/city_selection.txt", "w");
        if (file) {
            file.println(prayerConfig.selectedCity);
            file.println(prayerConfig.selectedCityName);
            file.println(prayerConfig.latitude);
            file.println(prayerConfig.longitude);
            file.flush();
            file.close();
        }
        xSemaphoreGive(settingsMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    
    if (displayQueue != NULL) {
        DisplayUpdate update;
        update.type = DisplayUpdate::PRAYER_UPDATE;
        xQueueSend(displayQueue, &update, 0);
    }
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
  
  vTaskDelay(pdMS_TO_TICKS(50));
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
    
    Serial.println("Testing RTC functionality...");
    rtc.adjust(DateTime(2024, 12, 16, 10, 30, 0));
    delay(2000);
    
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
        Serial.println("Temperature sensor works: " + String(rtc.getTemperature()) + "Â°C");
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
            timeConfig.currentTime = EPOCH_2000;
            
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

void sendJSONResponse(AsyncWebServerRequest *request, const String &json) {
    AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
    resp->addHeader("Content-Length", String(json.length()));
    resp->addHeader("Connection", "keep-alive");
    resp->addHeader("Cache-Control", "no-cache");
    request->send(resp);
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
    String ip = isWiFiConnected ? wifiConfig.localIP.toString() : "0.0.0.0";

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

    sendJSONResponse(request, String(jsonBuffer));
  });

  server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
      if (restartTaskHandle != NULL) {
          vTaskDelete(restartTaskHandle);
          restartTaskHandle = NULL;
          vTaskDelay(pdMS_TO_TICKS(100));
      }
      
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
      
      startCountdown("device_restart", "Memulai Ulang Perangkat", 60);
      
      request->send(200, "text/plain", "OK");
      
      xTaskCreate(
          [](void* param) {
              for (int i = 60; i > 0; i--) {
                  if (i == 35) {
                      WiFi.mode(WIFI_OFF);
                      vTaskDelay(pdMS_TO_TICKS(500));
                      
                      if (prayerTaskHandle != NULL) {
                          vTaskSuspend(prayerTaskHandle);
                      }
                      if (ntpTaskHandle != NULL) {
                          vTaskSuspend(ntpTaskHandle);
                      }
                      if (wifiTaskHandle != NULL) {
                          vTaskSuspend(wifiTaskHandle);
                      }
                      vTaskDelay(pdMS_TO_TICKS(1000));
                      
                      server.end();
                      vTaskDelay(pdMS_TO_TICKS(500));
                      
                      if (rtcAvailable) {
                          saveTimeToRTC();
                      }
                      vTaskDelay(pdMS_TO_TICKS(500));
                      
                      ledcWrite(TFT_BL, 0);
                      tft.fillScreen(TFT_BLACK);
                      vTaskDelay(pdMS_TO_TICKS(500));
                  }
                  
                  vTaskDelay(pdMS_TO_TICKS(1000));
              }
              
              Serial.flush();
              delay(1000);
              ESP.restart();
          },
          "DeviceRestartTask",
          5120,
          NULL,
          1,
          &restartTaskHandle
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

    sendJSONResponse(request, json);
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

      request -> send(429, "text/plain", msg);
      return;
    }

    if (request -> hasParam("ssid", true) && request -> hasParam("password", true)) {
      String newSSID = request -> getParam("ssid", true) -> value();
      String newPassword = request -> getParam("password", true) -> value();

      Serial.println("\n========================================");
      Serial.println("Save WiFi Credentials");
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
      unsigned long now = millis();
      if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
          unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);
          request->send(429, "text/plain", "Please wait " + String(waitTime / 1000) + " seconds");
          return;
      }

      bool updateNetworkConfig = false;
      if (request->hasParam("updateNetworkConfig", true)) {
          String mode = request->getParam("updateNetworkConfig", true)->value();
          updateNetworkConfig = (mode == "true");
      }

      Serial.println("\n========================================");
      Serial.println(updateNetworkConfig ? "UPDATE AP NETWORK CONFIG" : "UPDATE AP SSID/PASSWORD");
      Serial.println("========================================");

      if (updateNetworkConfig) {
          Serial.println("Mode: Network configuration only");
          
          IPAddress newAPIP = wifiConfig.apIP;
          IPAddress newGateway = wifiConfig.apGateway;
          IPAddress newSubnet = wifiConfig.apSubnet;

          if (request->hasParam("apIP", true)) {
              String ipStr = request->getParam("apIP", true)->value();
              ipStr.trim();
              
              if (ipStr.length() > 0) {
                  IPAddress tempIP;
                  if (tempIP.fromString(ipStr)) {
                      newAPIP = tempIP;
                      Serial.println("New AP IP: " + newAPIP.toString());
                  } else {
                      Serial.println("Invalid IP format, keeping: " + newAPIP.toString());
                  }
              }
          }

          if (request->hasParam("gateway", true)) {
              String gwStr = request->getParam("gateway", true)->value();
              gwStr.trim();
              
              if (gwStr.length() > 0) {
                  IPAddress tempGW;
                  if (tempGW.fromString(gwStr)) {
                      newGateway = tempGW;
                      Serial.println("New Gateway: " + newGateway.toString());
                  } else {
                      Serial.println("Invalid Gateway, keeping: " + newGateway.toString());
                  }
              }
          }

          if (request->hasParam("subnet", true)) {
              String snStr = request->getParam("subnet", true)->value();
              snStr.trim();
              
              if (snStr.length() > 0) {
                  IPAddress tempSN;
                  if (tempSN.fromString(snStr)) {
                      newSubnet = tempSN;
                      Serial.println("New Subnet: " + newSubnet.toString());
                  } else {
                      Serial.println("Invalid Subnet, keeping: " + newSubnet.toString());
                  }
              }
          }

          wifiConfig.apIP = newAPIP;
          wifiConfig.apGateway = newGateway;
          wifiConfig.apSubnet = newSubnet;
          
          saveAPCredentials();

          Serial.println("\nCONFIG SAVED:");
          Serial.println("  IP: " + newAPIP.toString() + " (updated)");
          Serial.println("  Gateway: " + newGateway.toString() + " (updated)");
          Serial.println("  Subnet: " + newSubnet.toString() + " (updated)");

      } else {
          Serial.println("Mode: SSID/Password only");
          Serial.println("Network config will remain unchanged");
          
          if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
              Serial.println("ERROR: Missing SSID or password");
              request->send(400, "text/plain", "Missing ssid or password");
              return;
          }

          String newSSID = request->getParam("ssid", true)->value();
          String newPass = request->getParam("password", true)->value();
          
          newSSID.trim();
          newPass.trim();

          if (newSSID.length() == 0) {
              Serial.println("ERROR: SSID cannot be empty");
              request->send(400, "text/plain", "SSID cannot be empty");
              return;
          }

          if (newPass.length() > 0 && newPass.length() < 8) {
              Serial.println("ERROR: Password must be at least 8 characters");
              request->send(400, "text/plain", "Password minimal 8 karakter");
              return;
          }

          newSSID.toCharArray(wifiConfig.apSSID, 33);
          newPass.toCharArray(wifiConfig.apPassword, 65);
          
          saveAPCredentials();

          Serial.println("\nCONFIG SAVED:");
          Serial.println("  SSID: " + String(wifiConfig.apSSID) + " (updated)");
          Serial.println("  Password: ******** (updated)");
          Serial.println("  IP: " + wifiConfig.apIP.toString() + " (unchanged)");
          Serial.println("  Gateway: " + wifiConfig.apGateway.toString() + " (unchanged)");
          Serial.println("  Subnet: " + wifiConfig.apSubnet.toString() + " (unchanged)");
      }

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
      
      Serial.println("CLIENT INFO:");
      Serial.println("  IP: " + clientIP.toString());
      Serial.println("  Access: " + String(isLocalAP ? "Local AP" : "Remote WiFi"));

      if (isLocalAP) {
          startCountdown("ap_restart", "Memulai Ulang Access Point", 60);
          Serial.println("Countdown started (client on local AP)");
      } else {
          Serial.println("Client on remote network - no countdown needed");
      }

      Serial.println("========================================\n");

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

    sendJSONResponse(request, json);
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

    sendJSONResponse(request, json);
  });

  server.on("/settimezone", HTTP_POST, [](AsyncWebServerRequest * request) {
      Serial.println("\n========================================");
      Serial.println("Save Timezone");
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

        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          timeConfig.ntpSynced = false;
          xSemaphoreGive(timeMutex);
        }

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

      String response = "{";
      response += "\"success\":true,";
      response += "\"offset\":" + String(offset) + ",";
      response += "\"ntpTriggered\":" + String(ntpTriggered ? "true" : "false") + ",";
      response += "\"prayerTimesWillUpdate\":" + String(prayerWillUpdate ? "true" : "false");
      response += "}";

      request -> send(200, "application/json", response);
      
      vTaskDelay(pdMS_TO_TICKS(50));
      
      saveTimezoneConfig();
      
      Serial.println("========================================");
      Serial.println("SUCCESS: Timezone saved");
      Serial.println("Offset: UTC" + String(offset >= 0 ? "+" : "") + String(offset));
      Serial.println("========================================\n");
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

    sendJSONResponse(request, json);
  });

  server.on("/setcity", HTTP_POST, [](AsyncWebServerRequest * request) {
      if (!request->hasParam("city", true)) {
          request->send(400, "application/json", "{\"error\":\"Missing city parameter\"}");
          return;
      }

      String cityApi = request->getParam("city", true)->value();
      String cityName = request->hasParam("cityName", true) 
          ? request->getParam("cityName", true)->value() 
          : cityApi;
      String lat = request->hasParam("lat", true) 
          ? request->getParam("lat", true)->value() 
          : "";
      String lon = request->hasParam("lon", true) 
          ? request->getParam("lon", true)->value() 
          : "";

      // Bounds checking
      if (cityApi.length() > 100) cityApi = cityApi.substring(0, 100);
      if (cityName.length() > 100) cityName = cityName.substring(0, 100);
      if (lat.length() > 20) lat = lat.substring(0, 20);
      if (lon.length() > 20) lon = lon.substring(0, 20);

      bool willUpdate = (WiFi.status() == WL_CONNECTED && lat.length() > 0 && lon.length() > 0);

      char response[256];
      snprintf(response, sizeof(response),
          "{\"success\":true,\"city\":\"%s\",\"updating\":%s}",
          cityName.c_str(),
          willUpdate ? "true" : "false"
      );

      request->send(200, "application/json", response);

      vTaskDelay(pdMS_TO_TICKS(50));

      // Baru update internal state
      if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          prayerConfig.selectedCity = cityApi;
          prayerConfig.selectedCityName = cityName;
          prayerConfig.latitude = lat;
          prayerConfig.longitude = lon;
          xSemaphoreGive(settingsMutex);
      }

      saveCitySelection();

      if (willUpdate) {
          vTaskDelay(pdMS_TO_TICKS(100));
          
          if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              needPrayerUpdate = true;
              pendingPrayerLat = lat;
              pendingPrayerLon = lon;
              xSemaphoreGive(settingsMutex);
              
              if (prayerTaskHandle != NULL) {
                  xTaskNotifyGive(prayerTaskHandle);
              }
          }
      }
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

    sendJSONResponse(request, json);
  });

  server.on("/setmethod", HTTP_POST, [](AsyncWebServerRequest * request) {
      Serial.println("\n========================================");
      Serial.println("Save Calculation Method");
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

      bool willFetchPrayerTimes = false;

      if (WiFi.status() == WL_CONNECTED) {
          if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
              Serial.println("Triggering prayer times update with new method...");
              Serial.println("City: " + prayerConfig.selectedCity);
              Serial.println("Method: " + methodName);
              
              willFetchPrayerTimes = true;
          } else {
              Serial.println("No coordinates available");
          }
      } else {
          Serial.println("WiFi not connected");
      }

      String response = "{";
      response += "\"success\":true,";
      response += "\"methodId\":" + String(methodId) + ",";
      response += "\"methodName\":\"" + methodName + "\",";
      response += "\"prayerTimesUpdating\":" + String(willFetchPrayerTimes ? "true" : "false");
      response += "}";

      request -> send(200, "application/json", response);
      
      vTaskDelay(pdMS_TO_TICKS(50));
      
      Serial.println("Writing to LittleFS...");
      saveMethodSelection();
      
      if (willFetchPrayerTimes) {
          vTaskDelay(pdMS_TO_TICKS(100));
          
          if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              needPrayerUpdate = true;
              pendingPrayerLat = prayerConfig.latitude;
              pendingPrayerLon = prayerConfig.longitude;
              xSemaphoreGive(settingsMutex);
              
              if (prayerTaskHandle != NULL) {
                  xTaskNotifyGive(prayerTaskHandle);
                  Serial.println("Prayer Task triggered for method change");
              }
          }

          Serial.println("Prayer times update initiated");
      }

      Serial.println("========================================");
      Serial.println("SUCCESS: Method saved successfully");
      Serial.println("Method: " + methodName);
      if (willFetchPrayerTimes) {
        Serial.println("Prayer times will update shortly...");
      }
      Serial.println("========================================\n");
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

    sendJSONResponse(request, json);
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

    sendJSONResponse(request, json);
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

  server.on("/testbuzzer", HTTP_POST, [](AsyncWebServerRequest * request) {
      if (!request->hasParam("volume", true)) {
          request->send(400, "text/plain", "Missing volume");
          return;
      }

      int volume = request->getParam("volume", true)->value().toInt();
      if (volume < 0) volume = 0;
      if (volume > 100) volume = 100;

      Serial.println("\n========================================");
      Serial.println("BUZZER TEST STARTED");
      Serial.println("========================================");
      Serial.printf("Volume: %d%%\n", volume);
      Serial.println("Duration: Manual stop or 30s timeout");
      Serial.println("========================================\n");

      if (buzzerTestTaskHandle != NULL) {
          vTaskDelete(buzzerTestTaskHandle);
          buzzerTestTaskHandle = NULL;
          ledcWrite(BUZZER_CHANNEL, 0);
          vTaskDelay(pdMS_TO_TICKS(100));
      }

      request->send(200, "text/plain", "OK");

      xTaskCreate(
          [](void* param) {
              int vol = *((int*)param);
              int pwmValue = map(vol, 0, 100, 0, 255);
              
              unsigned long startTime = millis();
              const unsigned long maxDuration = 30000;
              
              Serial.printf("Buzzer test loop starting (PWM: %d)\n", pwmValue);
              
              while ((millis() - startTime) < maxDuration) {
                  if (buzzerTestTaskHandle == NULL) {
                      Serial.println("Buzzer test stopped by user");
                      break;
                  }
                  
                  ledcWrite(BUZZER_CHANNEL, pwmValue);
                  vTaskDelay(pdMS_TO_TICKS(500));
                  
                  ledcWrite(BUZZER_CHANNEL, 0);
                  vTaskDelay(pdMS_TO_TICKS(500));
              }
              
              ledcWrite(BUZZER_CHANNEL, 0);
              Serial.println("Buzzer test completed");
              
              buzzerTestTaskHandle = NULL;
              delete (int*)param;
              vTaskDelete(NULL);
          },
          "BuzzerTest",
          2048,
          new int(volume),
          1,
          &buzzerTestTaskHandle
      );
  });

  server.on("/stopbuzzer", HTTP_POST, [](AsyncWebServerRequest * request) {
      Serial.println("\n========================================");
      Serial.println("BUZZER STOP REQUESTED");
      Serial.println("========================================\n");
      
      ledcWrite(BUZZER_CHANNEL, 0);
      
      if (buzzerTestTaskHandle != NULL) {
          vTaskDelete(buzzerTestTaskHandle);
          buzzerTestTaskHandle = NULL;
          Serial.println("Buzzer test task deleted");
      }
      
      request->send(200, "text/plain", "OK");
      Serial.println("Buzzer stopped successfully\n");
  });

  // ========================================
  // TAB RESET - FACTORY RESET
  // ========================================
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest * request) {
      if (resetTaskHandle != NULL) {
          vTaskDelete(resetTaskHandle);
          resetTaskHandle = NULL;
          vTaskDelay(pdMS_TO_TICKS(100));
      }
      
      Serial.println("FACTORY RESET STARTED");
      
      if (LittleFS.exists("/wifi_creds.txt")) {
          LittleFS.remove("/wifi_creds.txt");
      }
      if (LittleFS.exists("/prayer_times.txt")) {
          LittleFS.remove("/prayer_times.txt");
      }
      if (LittleFS.exists("/ap_creds.txt")) {
          LittleFS.remove("/ap_creds.txt");
      }
      if (LittleFS.exists("/city_selection.txt")) {
          LittleFS.remove("/city_selection.txt");
      }
      if (LittleFS.exists("/method_selection.txt")) {
          LittleFS.remove("/method_selection.txt");
      }
      if (LittleFS.exists("/timezone.txt")) {
          LittleFS.remove("/timezone.txt");
      }
      if (LittleFS.exists("/buzzer_config.txt")) {
          LittleFS.remove("/buzzer_config.txt");
      }
      if (LittleFS.exists("/adzan_state.txt")) {
          LittleFS.remove("/adzan_state.txt");
      }
      
      if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
          methodConfig.methodId = 5;
          methodConfig.methodName = "Egyptian General Authority of Survey";
          
          wifiConfig.routerSSID = "";
          wifiConfig.routerPassword = "";
          wifiConfig.isConnected = false;
          
          prayerConfig.imsakTime = "00:00";
          prayerConfig.subuhTime = "00:00";
          prayerConfig.terbitTime = "00:00";
          prayerConfig.zuhurTime = "00:00";
          prayerConfig.asharTime = "00:00";
          prayerConfig.maghribTime = "00:00";
          prayerConfig.isyaTime = "00:00";
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
      }
      
      if (rtcAvailable) {
          saveTimeToRTC();
      }
      
      updatePrayerDisplay();
      updateCityDisplay();
      
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
      
      startCountdown("factory_reset", "Pengaturan Ulang Perangkat", 60);
      
      request->send(200, "text/plain", "OK");
      
      xTaskCreate(
          [](void* param) {
              for (int i = 60; i > 0; i--) {
                  if (i == 35) {
                      WiFi.disconnect(true);
                      vTaskDelay(pdMS_TO_TICKS(500));
                      WiFi.mode(WIFI_OFF);
                      vTaskDelay(pdMS_TO_TICKS(500));
                      
                      if (prayerTaskHandle != NULL) {
                          vTaskSuspend(prayerTaskHandle);
                      }
                      if (ntpTaskHandle != NULL) {
                          vTaskSuspend(ntpTaskHandle);
                      }
                      if (wifiTaskHandle != NULL) {
                          vTaskSuspend(wifiTaskHandle);
                      }
                      vTaskDelay(pdMS_TO_TICKS(1000));
                      
                      server.end();
                      vTaskDelay(pdMS_TO_TICKS(500));
                      
                      ledcWrite(TFT_BL, 0);
                      tft.fillScreen(TFT_BLACK);
                      vTaskDelay(pdMS_TO_TICKS(500));
                  }
                  
                  vTaskDelay(pdMS_TO_TICKS(1000));
              }
              
              Serial.flush();
              delay(1000);
              ESP.restart();
          },
          "FactoryResetTask",
          5120,
          NULL,
          1,
          &resetTaskHandle
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

    sendJSONResponse(request, String(jsonBuffer));
  });

  // ========================================
  // COUNTDOWN STATUS API
  // ========================================
  server.on("/api/countdown", HTTP_GET, [](AsyncWebServerRequest * request) {
      String json = "{";

      if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int remaining = getRemainingSeconds();
        
        unsigned long serverMillis = millis();

        json += "\"active\":" + String(countdownState.isActive ? "true" : "false") + ",";
        json += "\"remaining\":" + String(remaining) + ",";
        json += "\"total\":" + String(countdownState.totalSeconds) + ",";
        json += "\"message\":\"" + countdownState.message + "\",";
        json += "\"reason\":\"" + countdownState.reason + "\",";
        json += "\"serverTime\":" + String(serverMillis);

        xSemaphoreGive(countdownMutex);
      } else {
        json += "\"active\":false,";
        json += "\"remaining\":0,";
        json += "\"total\":0,";
        json += "\"message\":\"\",";
        json += "\"reason\":\"\",";
        json += "\"serverTime\":" + String(millis());
      }

      json += "}";

      sendJSONResponse(request, json);
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
      html += "<div class='icon'>Ã°Å¸â€Â</div>";
      html += "<div class='error-code'>404</div>";
      html += "<h2>Page Not Found</h2>";
      html += "<p>The page you're looking for doesn't exist or you don't have permission to access it. Please return to the home page.</p>";
      html += "<a href='/' class='btn'>Ã¢â€ Â Back to Home</a>";
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

      if (percent < 40) Serial.println("WASTEFUL - can be reduced");
      else if (percent < 60) Serial.println("OPTIMAL");
      else if (percent < 75) Serial.println("FIT");
      else if (percent < 90) Serial.println("HIGH - monitor continuously");
      else if (percent < 95) Serial.println("DANGER - must be increased");
      else Serial.println("CRITICAL - increase immediately");
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

    if (validTouch && adzanState.canTouch && !touchPressed) {
      struct {String name; int x1,y1,x2,y2;} areas[] = {
        {"subuh",   200, 70,  310, 95},
        {"zuhur",   200, 130, 310, 155},
        {"ashar",   200, 160, 310, 185},
        {"maghrib", 200, 190, 310, 215},
        {"isya",    200, 220, 310, 240}
      };
      
      for (int i = 0; i < 5; i++) {
        if (areas[i].name == adzanState.currentPrayer &&
            lastX >= areas[i].x1 && lastX <= areas[i].x2 &&
            lastY >= areas[i].y1 && lastY <= areas[i].y2) {
          
          Serial.println("TOUCH ADZAN: " + areas[i].name);
          
          if (blinkState.isBlinking) stopBlinking();
          
          if (audioTaskHandle != NULL) {
            Serial.println("Audio system available - triggering playback");
            
            adzanState.isPlaying = true;
            adzanState.canTouch = false;
            
            xTaskNotifyGive(audioTaskHandle);
            
          } else {
            Serial.println("========================================");
            Serial.println("WARNING: Audio system not available");
            Serial.println("========================================");
            Serial.println("Reason: SD Card not detected or audio disabled");
            Serial.println("Action: Clearing adzan state immediately");
            Serial.println("========================================");
            
            // Langsung matikan state
            adzanState.isPlaying = false;
            adzanState.canTouch = false;
            adzanState.currentPrayer = "";
            
            // Simpan state kosong ke file
            saveAdzanState();
            
            Serial.println("Adzan state cleared - ready for next prayer time");
          }
          
          break;
        }
      }
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
            checkPrayerTime();
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
                // WAIT FOR NTP SYNC START
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
                // WAIT FOR NTP SYNC TO COMPLETE
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

                            esp_task_wdt_reset();
                        }
                        
                        esp_task_wdt_reset();
                    }
                    
                    if (ntpSyncInProgress) {
                        Serial.println("NTP sync timeout - proceeding anyway");
                    }
                    
                    continue;
                }

                // ============================================
                // NTP COMPLETE, UPDATE PRAYER TIMES
                // ============================================
                if (ntpSyncCompleted && timeConfig.ntpSynced) {
                    Serial.println("\n========================================");
                    Serial.println("NTP SYNC COMPLETED - TRIGGER PRAYER UPDATE");
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
                            Serial.println("Time is valid - triggering prayer times update...");
                            Serial.println("City: " + prayerConfig.selectedCity);
                            Serial.println("Coordinates: " + prayerConfig.latitude + 
                                        ", " + prayerConfig.longitude);
                            Serial.println("");
                            
                            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                                needPrayerUpdate = true;
                                pendingPrayerLat = prayerConfig.latitude;
                                pendingPrayerLon = prayerConfig.longitude;
                                xSemaphoreGive(settingsMutex);
                                
                                if (prayerTaskHandle != NULL) {
                                    xTaskNotifyGive(prayerTaskHandle);
                                    Serial.println("Prayer Task triggered - will update in background");
                                }
                            }
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
            
            if (millis() - lastMonitor > 60000) {
                lastMonitor = millis();
                Serial.printf("WiFi: Connected | RSSI: %d dBm | IP: %s\n",
                            WiFi.RSSI(),
                            WiFi.localIP().toString().c_str());
            }
        }

        // ========================================
        // Check if a first-time connection is required
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

        esp_task_wdt_reset();
        
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
        // SYNC WITH UTC+0 FIRST (NO TIMEZONE)
        // ============================================
        Serial.println("Syncing with NTP servers (UTC+0)...");
        Serial.println("Reason: Get raw UTC time first, apply timezone AFTER success");
        Serial.println("");
        
        // Use UTC+0 for initial sync - NO TIMEZONE YET!
        configTzTime("UTC0", 
                     ntpServers[0], 
                     ntpServers[1], 
                     ntpServers[2]);
        
        // ============================================
        // WAIT FOR NTP SYNC (UTC TIME ONLY)
        // ============================================
        time_t now = 0;
        struct tm timeinfo = {0};
        int retry = 0;
        const int retry_count = 40;
        
        while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
            vTaskDelay(pdMS_TO_TICKS(250));
            
            time(&now);
            gmtime_r(&now, &timeinfo);
            
            if (retry % 4 == 0) {
                Serial.printf("Waiting for NTP sync... (%d/%d) [%.1fs]\n", 
                             retry, retry_count, retry * 0.25);
            }
            
            if (retry % 4 == 0) {
                esp_task_wdt_reset();
            }
        }

        esp_task_wdt_reset();
        
        syncSuccess = (timeinfo.tm_year >= (2024 - 1900));
        
        if (syncSuccess) {
            ntpTime = now;  // This is UTC time
            usedServer = String(ntpServers[0]);
            
            Serial.println("\n========================================");
            Serial.println("NTP SYNC COMPLETED (UTC)");
            Serial.println("========================================");
            Serial.printf("UTC Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("Sync duration: %.1f seconds\n", retry * 0.25);
            Serial.printf("NTP Server: %s\n", usedServer.c_str());
            Serial.println("========================================");
            
            // ============================================
            // APPLY TIMEZONE OFFSET (AFTER SUCCESS)
            // ============================================
            Serial.println("\n========================================");
            Serial.println("APPLYING TIMEZONE OFFSET");
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
        // SAVE TIME TO SYSTEM (IF SUCCESS)
        // ============================================
        if (syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("SAVING TIME TO SYSTEM");
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
        // SAVE TO RTC (IF AVAILABLE & SUCCESS)
        // ============================================
        if (rtcAvailable && syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("SAVING TIME TO RTC HARDWARE");
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
                        Serial.println("   Status: RTC save FAILED");
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
        // AUTO PRAYER TIMES UPDATE
        // ============================================
        if (syncSuccess && wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("AUTO PRAYER TIMES UPDATE - TRIGGER");
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
                
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = true;
                    pendingPrayerLat = prayerConfig.latitude;
                    pendingPrayerLon = prayerConfig.longitude;
                    xSemaphoreGive(settingsMutex);
                    
                    Serial.println("Triggering Prayer Task for update...");
                    
                    if (prayerTaskHandle != NULL) {
                        xTaskNotifyGive(prayerTaskHandle);
                        Serial.println("Prayer Task notified - will update in background");
                    } else {
                        Serial.println("ERROR: Prayer Task handle NULL");
                    }
                }
                
                Serial.println("========================================");
                
            } else {
                Serial.println("SKIPPED: No city coordinates available");
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
  
  setupServerRoutes();
  server.begin();
  
  unsigned long lastCleanup = 0;
  unsigned long lastMemCheck = 0;
  unsigned long lastStackReport = 0;
  unsigned long lastAPCheck = 0;
  
  size_t initialHeap = ESP.getFreeHeap();
  size_t lowestHeap = initialHeap;
  size_t highestHeap = initialHeap;
  
  while (true) {
    esp_task_wdt_reset();
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    unsigned long now = millis();
    
    if (now - lastCleanup > 300000) {
      lastCleanup = now;
      
      if (restartTaskHandle != NULL) {
          eTaskState state = eTaskGetState(restartTaskHandle);
          if (state == eDeleted || state == eInvalid) {
              restartTaskHandle = NULL;
          }
      }
      
      if (resetTaskHandle != NULL) {
          eTaskState state = eTaskGetState(resetTaskHandle);
          if (state == eDeleted || state == eInvalid) {
              resetTaskHandle = NULL;
          }
      }
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
      if (currentHeap > highestHeap) {
        highestHeap = currentHeap;
      }
      
      int32_t usedFromLowest = initialHeap - lowestHeap;
      
      Serial.printf("\nMEMORY STATUS:\n");
      Serial.printf("Initial:  %6d bytes (%.2f KB)\n", 
                    initialHeap, initialHeap / 1024.0);
      Serial.printf("Current:  %6d bytes (%.2f KB)\n", 
                    currentHeap, currentHeap / 1024.0);
      Serial.printf("Lowest:   %6d bytes (%.2f KB)\n", 
                    lowestHeap, lowestHeap / 1024.0);
      Serial.printf("Highest:  %6d bytes (%.2f KB)\n", 
                    highestHeap, highestHeap / 1024.0);
      Serial.printf("Peak Usage: %6d bytes (%.2f KB)\n", 
                    usedFromLowest, usedFromLowest / 1024.0);
      
      if (usedFromLowest > 35000) {
        Serial.println("WARNING: High peak memory usage");
      } else if (usedFromLowest > 25000) {
        Serial.println("CAUTION: Moderate peak memory usage");
      } else {
        Serial.println("Memory status: Normal");
      }
      
      bool isCountdownActive = false;
      if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        isCountdownActive = countdownState.isActive;
        xSemaphoreGive(countdownMutex);
      }
      
      if (!isCountdownActive) {
        static size_t prevLowest = lowestHeap;
        if (prevLowest > lowestHeap && (prevLowest - lowestHeap) > 1000) {
          int32_t leaked = prevLowest - lowestHeap;
          Serial.printf("LEAK DETECTED: %d bytes lost since last check\n", leaked);
        }
        prevLowest = lowestHeap;
      }
      
      Serial.println();
    }
    
    if (now - lastAPCheck > 5000) {
      lastAPCheck = now;
      
      if (apRestartInProgress || wifiRestartInProgress) {
        continue;
      }
      
      wifi_mode_t mode;
      esp_wifi_get_mode(&mode);
      
      if (mode != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
        WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
      }
    }
  }
}


void prayerTask(void *parameter) {
    esp_task_wdt_add(NULL);
    
    Serial.println("\n========================================");
    Serial.println("PRAYER TASK STARTED");
    Serial.println("========================================");
    Serial.println("Stack Size: 12288 bytes");
    Serial.println("Waiting for triggers...");
    Serial.println("========================================\n");
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    static bool hasUpdatedToday = false;
    static int lastDay = -1;
    static bool waitingForMidnightNTP = false;
    static unsigned long midnightNTPStartTime = 0;

    while (true) {
        esp_task_wdt_reset();
        
        static unsigned long lastStackReport = 0;
        
        if (millis() - lastStackReport > 60000) {
            lastStackReport = millis();
            UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
            Serial.printf("[Prayer Task] Stack free: %d bytes\n", stackRemaining * 4);
            
            if (stackRemaining < 1000) {
                Serial.println("WARNING: Prayer Task stack critically low!");
            }
        }
        
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        
        if (needPrayerUpdate && pendingPrayerLat.length() > 0 && pendingPrayerLon.length() > 0) {
            esp_task_wdt_reset();
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Prayer Task: Skipping - WiFi not connected");
                
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = false;
                    xSemaphoreGive(settingsMutex);
                }
                continue;
            }
            
            time_t now_t;
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                now_t = timeConfig.currentTime;
                xSemaphoreGive(timeMutex);
            } else {
                now_t = time(nullptr);
            }
            
            if (now_t < 946684800) {
                Serial.println("Prayer Task: Skipping - Invalid system time");
                Serial.printf("Current timestamp: %ld (before 01/01/2000)\n", now_t);
                
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = false;
                    xSemaphoreGive(settingsMutex);
                }
                continue;
            }
            
            Serial.println("\n========================================");
            Serial.println("PRAYER TASK: Processing Update");
            Serial.println("========================================");
            Serial.printf("Stack before HTTP: %d bytes free\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("Coordinates: " + pendingPrayerLat + ", " + pendingPrayerLon);
            
            esp_task_wdt_reset();
            
            String tempLat = pendingPrayerLat;
            String tempLon = pendingPrayerLon;
            
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                needPrayerUpdate = false;
                pendingPrayerLat = "";
                pendingPrayerLon = "";
                xSemaphoreGive(settingsMutex);
            }
            
            getPrayerTimesByCoordinates(tempLat, tempLon);
            
            esp_task_wdt_reset();
            
            Serial.printf("Stack after HTTP: %d bytes free\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("Prayer Task: Update completed");
            Serial.println("========================================\n");
        }
        
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

            if (currentHour == 0 && currentMinute < 5 && 
                !hasUpdatedToday && 
                !waitingForMidnightNTP &&
                wifiConfig.isConnected) {
                
                Serial.println("\n========================================");
                Serial.println("MIDNIGHT DETECTED - STARTING SEQUENCE");
                Serial.println("========================================");
                Serial.printf("Time: %02d:%02d:%02d\n", currentHour, currentMinute, second(timeConfig.currentTime));
                Serial.printf("Date Now: %02d/%02d/%04d\n", currentDay, month(timeConfig.currentTime), currentYear);
                Serial.println("");
                
                Serial.println("Triggering NTP Sync...");
                Serial.println("Reason: Ensuring time is accurate before updating");
                
                if (ntpTaskHandle != NULL) {
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;
                    
                    xTaskNotifyGive(ntpTaskHandle);
                    
                    waitingForMidnightNTP = true;
                    midnightNTPStartTime = millis();
                    
                    Serial.println("NTP sync triggered successfully");
                    Serial.println("Waiting for NTP sync to complete...");
                    Serial.println("========================================\n");
                } else {
                    Serial.println("ERROR: NTP Task handle NULL");
                    Serial.println("Skipping midnight update");
                    Serial.println("========================================\n");
                    hasUpdatedToday = true;
                }
            }

            if (waitingForMidnightNTP) {
                unsigned long waitTime = millis() - midnightNTPStartTime;
                const unsigned long MAX_WAIT_TIME = 30000;

                if (waitTime % 5000 < 1000) {
                    esp_task_wdt_reset();
                }
                
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
                        
                        Serial.printf("New Time: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
                        Serial.printf("New Date: %02d/%02d/%04d\n", currentDay, currentMonth, currentYear);
                        Serial.printf("Timestamp: %ld\n", currentTimestamp);
                        Serial.println("");
                    }
                    
                    if (prayerConfig.latitude.length() > 0 && 
                        prayerConfig.longitude.length() > 0) {
                        
                        if (currentYear >= 2024 && currentTimestamp >= 946684800) {
                            Serial.println("Updating Prayer Times...");
                            Serial.println("Time Status: VALID");
                            Serial.println("City: " + prayerConfig.selectedCity);
                            Serial.println("Coordinate: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                            Serial.println("");
                            
                            esp_task_wdt_reset();
                            getPrayerTimesByCoordinates(
                                prayerConfig.latitude, 
                                prayerConfig.longitude
                            );
                            esp_task_wdt_reset();
                            
                            Serial.println("\nMidnight update sequence COMPLETED");
                        } else {
                            Serial.println("WARNING: Time is still invalid after NTP");
                            Serial.printf("   Year: %d (min: 2024)\n", currentYear);
                            Serial.printf("   Timestamp: %ld (min: 946684800)\n", currentTimestamp);
                            Serial.println("   Using existing prayer times");
                        }
                    } else {
                        Serial.println("WARNING: No city coordinates");
                        Serial.println("   Using existing prayer times");
                    }
                    
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else if (waitTime > MAX_WAIT_TIME) {
                    Serial.println("\n========================================");
                    Serial.println("NTP SYNC TIMEOUT");
                    Serial.println("========================================");
                    Serial.printf("Waiting time: %lu ms (max: %lu ms)\n", waitTime, MAX_WAIT_TIME);
                    Serial.println("NTP Status:");
                    Serial.printf("   ntpSyncInProgress: %s\n", ntpSyncInProgress ? "true" : "false");
                    Serial.printf("   ntpSyncCompleted: %s\n", ntpSyncCompleted ? "false" : "false");
                    Serial.println("");
                    Serial.println("Decision: Use existing prayer times");
                    Serial.println("Do not update (times may be inaccurate)");
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else {
                    if (waitTime % 5000 < 1000) {
                        Serial.printf("Waiting for NTP sync... (%lu/%lu ms)\n",
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
            // CHECK RTC VALID FIRST
            // ================================
            if (!isRTCValid()) {
                Serial.println("\n[RTC Sync] Skipped - RTC invalid");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            // ================================
            // READ RTC TIME
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
            // RTC TIME VALIDATION
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
            // COMPARE WITH SYSTEM TIME
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
            // SYNC SYSTEM TIME FROM RTC
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
    
    Serial.println("\nDisconnecting old WiFi...");
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("   Disconnected (config preserved)");
    
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
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    wifiRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);
    
    Serial.println("WiFi restart sequence completed");
    Serial.println("Lock released - system ready for next request\n");
    
    vTaskDelete(NULL);
}

// ============================================
// WiFi & AP Restart Tasks
// ============================================
void restartAPTask(void *parameter) {
    unsigned long now = millis();
    if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
        unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);
        
        Serial.println("\n========================================");
        Serial.println("AP RESTART REJECTED - TOO FAST");
        Serial.println("========================================");
        Serial.printf("Reason: Last restart was %lu ms ago\n", now - lastAPRestartRequest);
        Serial.printf("Minimum interval: %lu ms\n", RESTART_DEBOUNCE_MS);
        Serial.printf("Please wait: %lu ms (%.1f seconds)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    lastAPRestartRequest = now;
    
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
        Serial.println("AP RESTART BLOCKED");
        Serial.println("========================================");
        Serial.println("Reason: Another WiFi/AP restart in progress");
        Serial.println("Action: Request ignored for safety");
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    if (apRestartInProgress || wifiRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("AP RESTART ABORTED");
        Serial.println("========================================");
        Serial.printf("WiFi restart in progress: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("AP restart in progress: %s\n", apRestartInProgress ? "YES" : "NO");
        Serial.println("========================================\n");
        
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    apRestartInProgress = true;
    
    Serial.println("\n========================================");
    Serial.println("AP RESTART TASK STARTED");
    Serial.println("========================================");
    Serial.println("Countdown before AP shutdown");
    Serial.println("========================================\n");
    
    for (int i = 60; i > 0; i--) {
        if (i == 35) {
            Serial.println("\n========================================");
            Serial.println("SHUTTING DOWN AP");
            Serial.println("========================================");
            
            int clientsBefore = WiFi.softAPgetStationNum();
            Serial.printf("Clients connected: %d\n", clientsBefore);
            
            if (clientsBefore > 0) {
                Serial.println("Disconnecting clients...");
                esp_wifi_deauth_sta(0);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            WiFi.mode(WIFI_MODE_STA);
            WiFi.softAPdisconnect(true);
            
            Serial.println("AP shutdown complete");
            Serial.println("All clients disconnected");
            Serial.println("========================================\n");
        }
        
        if (i % 10 == 0 || i <= 5) {
            Serial.printf("AP will restart in %d seconds...\n", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Serial.println("\n========================================");
    Serial.println("COUNTDOWN COMPLETED - STARTING NEW AP NOW");
    Serial.println("========================================\n");
  
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

    WiFi.mode(WIFI_MODE_APSTA);
    
    Serial.println("\nConfiguring new AP network...");
    WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    Serial.println("Starting new AP broadcast...");
    bool apStarted = WiFi.softAP(savedSSID, savedPassword);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (apStarted) {
        IPAddress newAPIP = WiFi.softAPIP();
        String currentSSID = WiFi.softAPSSID();
        
        Serial.println("\n========================================");
        Serial.println("AP RESTART SUCCESS");
        Serial.println("========================================");
        Serial.println("SSID: \"" + currentSSID + "\" (ACTIVE)");
        Serial.println("IP: " + newAPIP.toString());
        Serial.println("MAC: " + WiFi.softAPmacAddress());
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("");
            Serial.println("WiFi Connection: PRESERVED");
            Serial.println("Router SSID: " + WiFi.SSID());
            Serial.println("Router IP: " + WiFi.localIP().toString());
        }
        
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
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        Serial.println("Rollback to: " + String(savedSSID));
        Serial.println("========================================\n");
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    apRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);
    
    Serial.println("\n========================================");
    Serial.println("AP RESTART PROTECTION RELEASED");
    Serial.println("========================================");
    Serial.println("apRestartInProgress: false");
    Serial.println("webTask monitoring: ACTIVE again");
    Serial.println("========================================\n");
    
    Serial.println("AP restart task completed\n");
    vTaskDelete(NULL);
}

bool initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = AUDIO_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  
  Serial.println("I2S OK");
  return true;
}

bool initSDCard() {
  SPIClass sdSPI(HSPI);
  
  sdSPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD FAIL");
    return false;
  }
  Serial.println("SD OK");
  return true;
}

bool isAudioFileValid(String filepath) {
  if (!SD.exists(filepath)) {
    Serial.println("File not found: " + filepath);
    return false;
  }
  
  File f = SD.open(filepath);
  if (!f || f.size() < 1024) {
    if(f) f.close();
    Serial.println("File corrupted/empty: " + filepath);
    return false;
  }
  
  f.close();
  return true;
}

bool playWAV(String filepath) {  
  File audioFile = SD.open(filepath);
  if (!audioFile) return false;
  
  audioFile.seek(44);
  
  Serial.println("PLAYING: " + filepath);
  
  uint8_t buffer[AUDIO_BUFFER_SIZE];
  size_t bytesRead, bytesWritten;
  
  unsigned long lastWDTReset = millis();
  
  while (audioFile.available() && adzanState.isPlaying) {
    bytesRead = audioFile.read(buffer, AUDIO_BUFFER_SIZE);
    
    for (size_t i = 0; i < bytesRead; i += 2) {
      int16_t sample = (int16_t)(buffer[i] | (buffer[i+1] << 8));
      sample = (sample * AUDIO_VOLUME) / 100;
      buffer[i] = sample & 0xFF;
      buffer[i+1] = (sample >> 8) & 0xFF;
    }
    
    esp_err_t result = i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, pdMS_TO_TICKS(100));
    
    if (result == ESP_ERR_TIMEOUT) {
      Serial.println("I2S timeout - skipping chunk");
    }
    
    if (millis() - lastWDTReset > 5000) {
      esp_task_wdt_reset();
      lastWDTReset = millis();
    }
    
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  
  audioFile.close();
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("AUDIO DONE");
  return true;
}

void stopAudio() {
  adzanState.isPlaying = false;
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void audioTask(void *parameter) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    if (adzanState.isPlaying && adzanState.currentPrayer.length() > 0) {
      String filepath = "/adzan/" + adzanState.currentPrayer + ".wav";
      
      if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (isAudioFileValid(filepath)) {
          Serial.println("Playing audio: " + filepath);
          
          playWAV(filepath);
        } else {
          Serial.println("Audio file not available: " + filepath);
        }
        xSemaphoreGive(sdMutex);
      }
      
      // Cleanup state
      adzanState.isPlaying = false;
      adzanState.canTouch = false;
      adzanState.currentPrayer = "";
      saveAdzanState();
      
      Serial.println("Adzan state cleared");
    }
  }
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
  Serial.println("VERSION 2.2 - STACK OPTIMIZED");
  Serial.println("========================================\n");

  // ================================
  // TURN OFF THE BACKLIGHT TOTALLY FIRST
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
  audioMutex = xSemaphoreCreateMutex();
  sdMutex = xSemaphoreCreateMutex();

  displayQueue = xQueueCreate(20, sizeof(DisplayUpdate));
  
  bool i2sOK = initI2S();
  bool sdOK = initSDCard();
  
  if (i2sOK && sdOK) {
    loadAdzanState();
    
    xTaskCreatePinnedToCore(
      audioTask, 
      "Audio", 
      AUDIO_TASK_STACK_SIZE, 
      NULL, 
      AUDIO_TASK_PRIORITY,
      &audioTaskHandle, 
      1
    );
    Serial.println("Audio Task OK");
  } else {
    Serial.println("Audio DISABLED");

    Serial.println("Clearing any pending adzan state...");
    
    adzanState.isPlaying = false;
    adzanState.canTouch = false;
    adzanState.currentPrayer = "";
    
    if (LittleFS.exists("/adzan_state.txt")) {
      LittleFS.remove("/adzan_state.txt");
      Serial.println("Adzan state file removed (no audio system)");
    }
    
    Serial.println("Adzan state cleaned - buzzer-only mode active");
  }

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
  // WIFI CONFIGURATION - UPDATED!
  // ================================
  Serial.println("\n========================================");
  Serial.println("WIFI CONFIGURATION");
  Serial.println("========================================");

  setupWiFiEvents();
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  Serial.println("Applying WiFi optimizations for router access...");
  
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  
  Serial.println("  Protocol: 802.11 b/g/n");
  Serial.println("  Bandwidth: 40MHz (HT40)");
  Serial.println("  TX Power: 19.5dBm (max)");

  // ================================
  // SET HOSTNAME SEBELUM OPERASI WIFI LAINNYA
  // ================================
  WiFi.setHostname("JWS-Indonesia");
  delay(200);
  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());

  WiFi.setSleep(WIFI_PS_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_wifi_set_max_tx_power(78);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.println("WiFi Mode: AP + STA");
  Serial.println("WiFi Sleep: DOUBLE DISABLED");
  Serial.println("  Arduino: WIFI_PS_NONE");
  Serial.println("  ESP-IDF: WIFI_PS_NONE");
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
  Serial.println("\n========================================");
  Serial.println("STARTING FREERTOS TASKS");
  Serial.println("========================================");

  xTaskCreatePinnedToCore(
    uiTask,
    "UI",
    UI_TASK_STACK_SIZE,
    NULL,
    UI_TASK_PRIORITY,
    &uiTaskHandle,
    1  // Core 1
  );
  Serial.printf("UI Task (Core 1) - Stack: %d bytes\n", UI_TASK_STACK_SIZE);

  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFi",
    WIFI_TASK_STACK_SIZE,
    NULL,
    WIFI_TASK_PRIORITY,
    &wifiTaskHandle,
    0  // Core 0
  );
  Serial.printf("WiFi Task (Core 0) - Stack: %d bytes\n", WIFI_TASK_STACK_SIZE);

  xTaskCreatePinnedToCore(
    ntpTask,
    "NTP",
    NTP_TASK_STACK_SIZE,
    NULL,
    NTP_TASK_PRIORITY,
    &ntpTaskHandle,
    0  // Core 0
  );
  Serial.printf("NTP Task (Core 0) - Stack: %d bytes\n", NTP_TASK_STACK_SIZE);

  xTaskCreatePinnedToCore(
    webTask,
    "Web",
    WEB_TASK_STACK_SIZE,
    NULL,
    WEB_TASK_PRIORITY,
    &webTaskHandle,
    0  // Core 0
  );
  Serial.printf("Web Task (Core 0) - Stack: %d bytes\n", WEB_TASK_STACK_SIZE);

  xTaskCreatePinnedToCore(
    prayerTask,
    "Prayer",
    PRAYER_TASK_STACK_SIZE,
    NULL,
    PRAYER_TASK_PRIORITY,
    &prayerTaskHandle,
    0  // Core 0
  );
  Serial.printf("Prayer Task (Core 0) - Stack: %d bytes\n", PRAYER_TASK_STACK_SIZE);

  if (prayerTaskHandle) {
      esp_task_wdt_add(prayerTaskHandle);
      Serial.println("WDT registered");
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
  Serial.printf("Clock Task (Core 0) - Stack: %d bytes\n", CLOCK_TASK_STACK_SIZE);

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
    Serial.printf("RTC Sync Task (Core 0) - Stack: %d bytes\n", RTC_TASK_STACK_SIZE);
  }

  // ================================
  // AUTO-RECOVERY TASK FOR PRAYER
  // ================================
  xTaskCreate(
    [](void* param) {
      const TickType_t checkInterval = pdMS_TO_TICKS(30000);  // Check every 30s
      
      Serial.println("Prayer Watchdog Task - Monitoring every 30s");
      
      while (true) {
        vTaskDelay(checkInterval);
        
        if (prayerTaskHandle != NULL) {
          eTaskState state = eTaskGetState(prayerTaskHandle);
          
          if (state == eDeleted || state == eInvalid) {
            Serial.println("\n========================================");
            Serial.println("CRITICAL: PRAYER TASK CRASHED");
            Serial.println("========================================");
            Serial.println("Detected state: " + String(state == eDeleted ? "DELETED" : "INVALID"));
            Serial.println("Action: Auto-restarting task...");
            Serial.println("========================================");
            
            // Recreate task
            xTaskCreatePinnedToCore(
              prayerTask,
              "Prayer",
              PRAYER_TASK_STACK_SIZE,
              NULL,
              PRAYER_TASK_PRIORITY,
              &prayerTaskHandle,
              0
            );
            
            if (prayerTaskHandle) {
              esp_task_wdt_add(prayerTaskHandle);
              Serial.println("\nPrayer Task restarted successfully");
              Serial.println("Stack: " + String(PRAYER_TASK_STACK_SIZE) + " bytes");
              Serial.println("WDT: Re-registered");
              Serial.println("========================================\n");
            } else {
              Serial.println("\nFAILED to restart Prayer Task!");
              Serial.println("System may be unstable");
              Serial.println("========================================\n");
            }
          }
        } else {
          // Task handle is NULL - try to create it
          Serial.println("\nWARNING: Prayer Task handle is NULL");
          Serial.println("Attempting to create task...");
          
          xTaskCreatePinnedToCore(
            prayerTask,
            "Prayer",
            PRAYER_TASK_STACK_SIZE,
            NULL,
            PRAYER_TASK_PRIORITY,
            &prayerTaskHandle,
            0
          );
          
          if (prayerTaskHandle) {
            esp_task_wdt_add(prayerTaskHandle);
            Serial.println("Prayer Task created successfully\n");
          }
        }
      }
    },
    "PrayerWatchdog",
    2048,
    NULL,
    1,  // Low priority
    NULL
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  // ================================
  // REGISTER TASKS TO WATCHDOG
  // ================================
  Serial.println("\nRegistering tasks to watchdog:");
  
  if (wifiTaskHandle) {
    esp_task_wdt_add(wifiTaskHandle);
    Serial.println("  WiFi Task");
  }
  if (webTaskHandle) {
    esp_task_wdt_add(webTaskHandle);
    Serial.println("  Web Task");
  }
  if (ntpTaskHandle) {
    esp_task_wdt_add(ntpTaskHandle);
    Serial.println("  NTP Task");
  }

  Serial.println("========================================\n");

  // ================================
  // MEMORY & STACK REPORT
  // ================================
  Serial.println("========================================");
  Serial.println("MEMORY REPORT");
  Serial.println("========================================");
  
  size_t freeHeap = ESP.getFreeHeap();
  size_t totalHeap = ESP.getHeapSize();
  size_t usedHeap = totalHeap - freeHeap;
  
  Serial.printf("Total Heap:  %d bytes (%.2f KB)\n", totalHeap, totalHeap / 1024.0);
  Serial.printf("Used Heap:   %d bytes (%.2f KB)\n", usedHeap, usedHeap / 1024.0);
  Serial.printf("Free Heap:   %d bytes (%.2f KB)\n", freeHeap, freeHeap / 1024.0);
  Serial.printf("Usage:       %.1f%%\n", (usedHeap * 100.0) / totalHeap);
  
  Serial.println("\nTask Stack Allocation:");
  uint32_t totalStack = UI_TASK_STACK_SIZE + WIFI_TASK_STACK_SIZE + 
                        NTP_TASK_STACK_SIZE + WEB_TASK_STACK_SIZE + 
                        PRAYER_TASK_STACK_SIZE + CLOCK_TASK_STACK_SIZE;
  if (rtcAvailable) totalStack += RTC_TASK_STACK_SIZE;
  
  Serial.printf("Total:       %d bytes (%.2f KB)\n", totalStack, totalStack / 1024.0);
  Serial.printf("Largest:     Prayer (%d bytes)\n", PRAYER_TASK_STACK_SIZE);
  Serial.println("========================================\n");

  // ================================
  // STARTUP COMPLETE
  // ================================
  Serial.println("========================================");
  Serial.println("SYSTEM READY");
  Serial.println("========================================");
  Serial.println("Multi-client concurrent access enabled");
  Serial.println("WiFi sleep disabled for better response");
  Serial.println("Router optimization active (keep-alive)");
  Serial.println("Prayer Task auto-recovery enabled");
  Serial.println("Stack monitoring active");
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
  Serial.println("Monitoring logs will appear below:");
  Serial.println("  - Stack usage reports every 60s");
  Serial.println("  - Prayer Task health checks every 30s");
  Serial.println("  - Memory reports every 30s (from webTask)");
  Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}