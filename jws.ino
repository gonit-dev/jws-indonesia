/*
 * ESP32-2432S024 + LVGL 9.2.0 + EEZ Studio - Islamic Prayer Clock
 * ARCHITECTURE: FreeRTOS Multi-Task Design - FULLY AUTOMATED
 * OPTIMIZED VERSION - Event-Driven + Built-in NTP
 */

#define PRODUCTION 1  // 1 = matikan serial print, 0 = aktifkan serial print

#include "Wire.h"
#include "RTClib.h"
#include "lvgl.h"
#include "TFT_eSPI.h"
#include "XPT2046_Touchscreen.h"
#include "SPI.h"
#include "LittleFS.h"
#include "ArduinoJson.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "TimeLib.h"
#include "time.h"
#include "HTTPClient.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "DFRobotDFPlayerMini.h"

// EEZ generated files
#include "src/ui.h"
#include "src/screens.h"
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

// Pin DFPlayer Mini (Hardware Serial)
#define DFPLAYER_TX 25  // ESP32 TX → DFPlayer RX
#define DFPLAYER_RX 32  // ESP32 RX → DFPlayer TX

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
#define WEB_TASK_STACK_SIZE 4096       // AsyncWebServer
#define PRAYER_TASK_STACK_SIZE 4096    // HTTP + JSON parsing
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
TaskHandle_t httpTaskHandle = NULL;
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

// Queue for display updates
QueueHandle_t displayQueue;
QueueHandle_t httpQueue; 

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
String DEFAULT_AP_SSID = "JWS-" + String(ESP.getEfuseMac(), HEX);
#define DEFAULT_AP_PASSWORD "12345678"
String hostname = "JWS-" + String(ESP.getEfuseMac(), HEX);

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
  
  int tuneImsak = 0;
  int tuneSubuh = 0;
  int tuneTerbit = 0;
  int tuneZuhur = 0;
  int tuneAshar = 0;
  int tuneMaghrib = 0;
  int tuneIsya = 0;
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

struct AlarmConfig {
  char alarmTime[6]; // "HH:MM"
  bool alarmEnabled;
};

// ================================
// ALARM STATE
// ================================
struct AlarmState {
  bool isRinging;           // Alarm sedang berbunyi
  unsigned long lastToggle; // Untuk kedip jam
  bool clockVisible;        // Status kedip jam
  // Simpan state notif shalat saat alarm aktif
  bool savedBlinkState;
  bool savedAdzanCanTouch;
};

AlarmState alarmState = {
  .isRinging = false,
  .lastToggle = 0,
  .clockVisible = true,
  .savedBlinkState = false,
  .savedAdzanCanTouch = false
};

SemaphoreHandle_t alarmMutex = NULL;
static int lastAlarmMinute = -1; // Cegah trigger berulang menit sama

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
DFRobotDFPlayerMini dfPlayer;
HardwareSerial dfSerial(2);
bool dfPlayerAvailable = false;

WiFiConfig wifiConfig;
TimeConfig timeConfig;
PrayerConfig prayerConfig;
MethodConfig methodConfig = { 5, "Egyptian General Authority of Survey" };
int timezoneOffset = 7;

BuzzerConfig buzzerConfig = {
  false, false, false, false, false, false, false,
  50
};

AlarmConfig alarmConfig = {
  "00:00",  // alarmTime
  false     // alarmEnabled
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
// HTTP REQUEST STRUCTURE
// ================================
struct HTTPRequest {
  String latitude;
  String longitude;
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
// BLINK TRIGGER GUARD
// ================================
static int lastBlinkMinute = -1;  // Format: jam*60 + menit

// ================================
// WIFI RESTART PROTECTION
// ================================
static SemaphoreHandle_t wifiRestartMutex = NULL;
static bool wifiRestartInProgress = false;
static bool apRestartInProgress = false;

// Debouncing timestamps
static unsigned long lastWiFiRestartRequest = 0;
static unsigned long lastAPRestartRequest = 0;
const unsigned long RESTART_DEBOUNCE_MS = 3000;

void updateCityDisplay();
void updateTimeDisplay();
void updatePrayerDisplay();
void hideAllUIElements();
void showAllUIElements();

void startCountdown(String reason, String message, int seconds);
void stopCountdown();
int getRemainingSeconds();

void checkPrayerTime();
void startBlinking(String prayerName);
void stopBlinking();
void handleBlinking();

void getPrayerTimesByCoordinates(String lat, String lon);
void savePrayerTimes();
void loadPrayerTimes();

void saveWiFiCredentials();
void loadWiFiCredentials();
void saveAPCredentials();
void setupWiFiEvents();

void saveTimezoneConfig();
void loadTimezoneConfig();
void saveBuzzerConfig();
void loadBuzzerConfig();
void saveAdzanState();
void loadAdzanState();
int getAdzanRemainingSeconds();

void saveAlarmConfig();
void loadAlarmConfig();
void checkAlarmTime();
void stopAlarm();
void handleAlarmBlink();
void saveCitySelection();
void loadCitySelection();
void saveMethodSelection();
void loadMethodSelection();

bool initRTC();
bool isRTCValid();
bool isRTCTimeValid(DateTime dt);
void saveTimeToRTC();

void setupServerRoutes();
void sendJSONResponse(AsyncWebServerRequest *request, const String &json);

bool init_littlefs();
void printStackReport();

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data);

void uiTask(void *parameter);
void wifiTask(void *parameter);
void ntpTask(void *parameter);
void webTask(void *parameter);
void prayerTask(void *parameter);
void rtcSyncTask(void *parameter);
void clockTickTask(void *parameter);
void audioTask(void *parameter);

void restartWiFiTask(void *parameter);
void restartAPTask(void *parameter);

bool initDFPlayer();
void setDFPlayerVolume(int vol);
void playDFPlayerAdzan(String prayerName);
bool isDFPlayerPlaying();

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
    Serial.println("Alasan: " + reason);
    Serial.println("Pesan: " + message);
    Serial.println("Durasi: " + String(seconds) + " detik");
    Serial.println("========================================\n");
  }
}

void stopCountdown() {
  if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    countdownState.isActive = false;
    countdownState.reason = "";
    xSemaphoreGive(countdownMutex);
    
    Serial.println("Hitung mundur dihentikan");
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
  // Jika alarm sedang aktif, tunda notif shalat
  if (alarmState.isRinging) return;

  time_t now_t = timeConfig.currentTime;
  struct tm timeinfo;
  localtime_r(&now_t, &timeinfo);
  
  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  
  int currentMinuteKey = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  
  if (timeinfo.tm_sec < 5                    // Window 5 detik (bukan hanya sec==0)
      && currentMinuteKey != lastBlinkMinute  // Belum di-trigger menit ini
      && !blinkState.isBlinking 
      && !adzanState.canTouch) {
    
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
      lastBlinkMinute = currentMinuteKey;  // Tandai sudah trigger menit ini
      startBlinking(prayerName);
      
      bool isAdzanPrayer = (prayerName != "imsak" && prayerName != "terbit");
      
      if (isAdzanPrayer && dfPlayerAvailable) {
        adzanState.canTouch = true;
        adzanState.currentPrayer = prayerName;
        adzanState.startTime = now_t;
        adzanState.deadlineTime = now_t + 600;
        saveAdzanState();
        Serial.println("ADZAN AKTIF: " + prayerName + " - sentuh layar untuk putar (10 menit)");
      } else {
        adzanState.canTouch = false;
        adzanState.currentPrayer = "";
        Serial.println("NOTIF AKTIF: " + prayerName + " - buzzer+blink only (no touch needed)");
      }
    }
  }
  
  if (adzanState.canTouch && getAdzanRemainingSeconds() <= 0) {
    Serial.println("ADZAN EXPIRED: " + adzanState.currentPrayer);
    adzanState.canTouch = false;
    adzanState.currentPrayer = "";
    adzanState.isPlaying = false;
    saveAdzanState();
    lastBlinkMinute = -1;
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
  Serial.print("WAKTU SHALAT: ");
  Serial.println(upperName);
  Serial.println("========================================");
  Serial.println("Mulai berkedip selama 1 menit...");
  Serial.println("========================================\n");
}

void stopBlinking() {
  if (blinkState.isBlinking) {
    blinkState.isBlinking = false;
    blinkState.activePrayer = "";
    
    ledcWrite(BUZZER_CHANNEL, 0);
    
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      if (objects.imsak_time) lv_obj_clear_flag(objects.imsak_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.subuh_time) lv_obj_clear_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.terbit_time) lv_obj_clear_flag(objects.terbit_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.zuhur_time) lv_obj_clear_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.ashar_time) lv_obj_clear_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.maghrib_time) lv_obj_clear_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
      if (objects.isya_time) lv_obj_clear_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
      xSemaphoreGive(displayMutex);
    } else {
      Serial.println("[stopBlinking] WARNING: displayMutex timeout - elemen mungkin masih hidden");
    }
    
    Serial.println("Kedip selesai - semua waktu shalat tampak normal");
  }
}

void handleBlinking() {
  if (!blinkState.isBlinking) return;
  
  unsigned long currentMillis = millis();
  
  if (currentMillis - blinkState.blinkStartTime >= BLINK_DURATION) {
    stopBlinking();
    return;  // stopBlinking() sudah matikan buzzer
  }
  
  if (currentMillis - blinkState.lastBlinkToggle >= BLINK_INTERVAL) {
    blinkState.lastBlinkToggle = currentMillis;
    blinkState.currentVisible = !blinkState.currentVisible;
    
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
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
          int pwmValue = map(buzzerConfig.volume, 0, 100, 0, 255);
          ledcWrite(BUZZER_CHANNEL, pwmValue);  // Buzzer ON sinkron dengan LCD tampil
        } else {
          lv_obj_add_flag(targetLabel, LV_OBJ_FLAG_HIDDEN);
          ledcWrite(BUZZER_CHANNEL, 0);          // Buzzer OFF sinkron dengan LCD hilang
        }
      }
      
      xSemaphoreGive(displayMutex);
    } else {
      blinkState.currentVisible = !blinkState.currentVisible;
    }
  }
}

// ============================================
// Prayer Times API Functions
// ============================================
void getPrayerTimesByCoordinates(String lat, String lon) {  
  Serial.println("\n[Tugas Shalat] Mengirim permintaan HTTP ke antrian...");
  
  HTTPRequest request;
  request.latitude = lat;
  request.longitude = lon;
  
  if (xQueueSend(httpQueue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println("[Tugas Shalat] Permintaan HTTP berhasil diantrekan");
  } else {
    Serial.println("[Tugas Shalat] Gagal mengantrekan permintaan HTTP (antrian penuh)");
  }
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
      Serial.println("Waktu shalat tersimpan");

      vTaskDelay(pdMS_TO_TICKS(100));
      if (LittleFS.exists("/prayer_times.txt")) {
        Serial.println("File waktu shalat terverifikasi");
      }
    } else {
      Serial.println("Gagal membuka prayer_times.txt untuk ditulis");
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
        Serial.println("Waktu shalat dimuat");
        Serial.println("Kota: " + prayerConfig.selectedCity);
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
        Serial.println("File WiFi terverifikasi");
      }
    } else {
      Serial.println("Gagal membuka wifi_creds.txt untuk ditulis");
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
          wifiConfig.apIP = IPAddress(192, 168, 100, 1);
        }
        
        if (file.available()) {
          String gwStr = file.readStringUntil('\n');
          gwStr.trim();
          wifiConfig.apGateway.fromString(gwStr);
        } else {
          wifiConfig.apGateway = IPAddress(192, 168, 100, 1);
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
      DEFAULT_AP_SSID.toCharArray(wifiConfig.apSSID, 33);
      strcpy(wifiConfig.apPassword, DEFAULT_AP_PASSWORD);
      wifiConfig.apIP = IPAddress(192, 168, 100, 1);
      wifiConfig.apGateway = IPAddress(192, 168, 100, 1);
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
        Serial.println("File AP terverifikasi");
      }
    } else {
      Serial.println("Gagal membuka ap_creds.txt untuk ditulis");
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
                Serial.print("STA Terhubung ke AP");
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
                String quality = rssi >= -50 ? "Sangat Baik" : 
                                rssi >= -60 ? "Baik" : 
                                rssi >= -70 ? "Cukup" : "Lemah";
                Serial.println("Sinyal: " + quality);
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
                    Serial.print("Memicu sinkronisasi NTP...");
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
                Serial.printf("Kode alasan: %d", info.wifi_sta_disconnected.reason);

                xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
                xEventGroupSetBits(wifiEventGroup, WIFI_DISCONNECTED_BIT);

                switch (info.wifi_sta_disconnected.reason) {
                    case WIFI_REASON_AUTH_EXPIRE:
                        Serial.println("Detail: Autentikasi kedaluwarsa");
                        break;
                    case WIFI_REASON_AUTH_LEAVE:
                        Serial.println("Detail: Terputus autentikasi");
                        break;
                    case WIFI_REASON_ASSOC_LEAVE:
                        Serial.println("Detail: Koneksi diputus");
                        break;
                    case WIFI_REASON_BEACON_TIMEOUT:
                        Serial.println("Detail: Timeout beacon (router tidak terjangkau)");
                        break;
                    case WIFI_REASON_NO_AP_FOUND:
                        Serial.println("Detail: AP tidak ditemukan");
                        break;
                    case WIFI_REASON_HANDSHAKE_TIMEOUT:
                        Serial.println("Detail: Timeout handshake");
                        break;
                    default:
                        Serial.printf("Detail: Alasan tidak diketahui (%d)", 
                                     info.wifi_sta_disconnected.reason);
                }

                wifiDisconnectedTime = millis();

                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    wifiConfig.isConnected = false;
                    wifiState = WIFI_IDLE;
                    xSemaphoreGive(wifiMutex);
                }
                
                Serial.println("Akan mencoba menghubungkan kembali...");
                Serial.println("========================================\n");
                break;
            }

            case ARDUINO_EVENT_WIFI_AP_START:
                Serial.print("AP Dimulai: " + String(WiFi.softAPSSID()));
                Serial.print("   IP AP: " + WiFi.softAPIP().toString());
                break;

            case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                Serial.print("Klien terhubung ke AP");
                Serial.print(String("   Total stations: ") + String(WiFi.softAPgetStationNum()));
                break;

            case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                Serial.print("Klien terputus dari AP");
                Serial.print(String("   Total stations: ") + String(WiFi.softAPgetStationNum()));
                break;
        }
    });

    Serial.print("WiFi Event Handler terdaftar (dengan proteksi restart)");
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
      Serial.println("Gagal menyimpan timezone");
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
          Serial.println("Offset timezone tidak valid di file, menggunakan default +7");
          timezoneOffset = 7;
        }

        file.close();
        Serial.println("Timezone loaded: UTC" + String(timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset));
      }
    } else {
      timezoneOffset = 7;
      Serial.println("Konfigurasi timezone tidak ditemukan - menggunakan default (UTC+7)");
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
        Serial.println("Konfigurasi buzzer dimuat");
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// ALARM CONFIG - Save / Load
// ============================================
void saveAlarmConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/alarm_config.txt", "w");
    if (file) {
      file.println(alarmConfig.alarmTime);
      file.println(alarmConfig.alarmEnabled ? "1" : "0");
      file.flush();
      file.close();
      Serial.println("Konfigurasi alarm disimpan: " + String(alarmConfig.alarmTime) + 
                     " | " + (alarmConfig.alarmEnabled ? "ON" : "OFF"));
    }
    xSemaphoreGive(settingsMutex);
  }
}

void loadAlarmConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    if (LittleFS.exists("/alarm_config.txt")) {
      fs::File file = LittleFS.open("/alarm_config.txt", "r");
      if (file) {
        String timeStr = file.readStringUntil('\n');
        timeStr.trim();
        if (timeStr.length() == 5) {
          strncpy(alarmConfig.alarmTime, timeStr.c_str(), 5);
          alarmConfig.alarmTime[5] = '\0';
        }
        String enabledStr = file.readStringUntil('\n');
        enabledStr.trim();
        alarmConfig.alarmEnabled = (enabledStr == "1");
        file.close();
        Serial.println("Konfigurasi alarm dimuat: " + String(alarmConfig.alarmTime) + 
                       " | " + (alarmConfig.alarmEnabled ? "ON" : "OFF"));
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// ALARM - Stop (dipanggil saat layar disentuh)
// ============================================
void stopAlarm() {
  if (!alarmState.isRinging) return;

  Serial.println("\n========================================");
  Serial.println("ALARM DIHENTIKAN (layar disentuh)");
  Serial.println("========================================");

  alarmState.isRinging = false;
  ledcWrite(BUZZER_CHANNEL, 0);

  // Kembalikan tampilan jam normal
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    if (objects.time_now) lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
    xSemaphoreGive(displayMutex);
  }
  alarmState.clockVisible = true;

  // Kembalikan state notif shalat yang tadi dimatikan
  // (blinkState & adzanState sudah di-restore secara otomatis karena kita tidak menghapusnya,
  //  hanya memblokir handleBlinking() selama alarm aktif)
  Serial.println("Notif shalat dikembalikan");
  Serial.println("========================================\n");
}

// ============================================
// ALARM - Check apakah saatnya berbunyi
// ============================================
void checkAlarmTime() {
  if (!alarmConfig.alarmEnabled) return;
  if (alarmState.isRinging) return;

  time_t now_t = timeConfig.currentTime;
  struct tm timeinfo;
  localtime_r(&now_t, &timeinfo);

  // Hanya trigger sekali per menit
  int currentMinuteKey = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  if (currentMinuteKey == lastAlarmMinute) return;
  if (timeinfo.tm_sec >= 5) return; // Hanya 5 detik pertama menit

  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  if (strcmp(currentTime, alarmConfig.alarmTime) == 0) {
    lastAlarmMinute = currentMinuteKey;

    Serial.println("\n========================================");
    Serial.println("ALARM AKTIF: " + String(alarmConfig.alarmTime));
    Serial.println("========================================");
    Serial.println("Kedip jam + buzzer dimulai");
    Serial.println("Notif shalat ditangguhkan sampai alarm mati");
    Serial.println("========================================\n");

    // Simpan state shalat yang sedang aktif (blinking & adzan)
    alarmState.savedBlinkState  = blinkState.isBlinking;
    alarmState.savedAdzanCanTouch = adzanState.canTouch;

    // Pause notif shalat: paksa stop blinking & adzan sementara
    if (blinkState.isBlinking) {
      // Hentikan kedip shalat (tampilkan semua waktu shalat kembali)
      blinkState.isBlinking = false;
      blinkState.activePrayer = "";
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (objects.imsak_time) lv_obj_clear_flag(objects.imsak_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.subuh_time) lv_obj_clear_flag(objects.subuh_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.terbit_time) lv_obj_clear_flag(objects.terbit_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.zuhur_time) lv_obj_clear_flag(objects.zuhur_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.ashar_time) lv_obj_clear_flag(objects.ashar_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.maghrib_time) lv_obj_clear_flag(objects.maghrib_time, LV_OBJ_FLAG_HIDDEN);
        if (objects.isya_time) lv_obj_clear_flag(objects.isya_time, LV_OBJ_FLAG_HIDDEN);
        xSemaphoreGive(displayMutex);
      }
    }
    // Pause adzan touch juga
    if (adzanState.canTouch) {
      adzanState.canTouch = false;
    }

    alarmState.isRinging = true;
    alarmState.lastToggle = millis();
    alarmState.clockVisible = true;
  }
}

// ============================================
// ALARM - Handle kedip jam + buzzer
// ============================================
void handleAlarmBlink() {
  if (!alarmState.isRinging) return;

  unsigned long currentMillis = millis();
  if (currentMillis - alarmState.lastToggle >= 500) { // Kedip 500ms
    alarmState.lastToggle = currentMillis;
    alarmState.clockVisible = !alarmState.clockVisible;

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
      if (objects.time_now) {
        if (alarmState.clockVisible) {
          lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
          int pwmValue = map(buzzerConfig.volume, 0, 100, 0, 255);
          ledcWrite(BUZZER_CHANNEL, pwmValue); // Buzzer ON
        } else {
          lv_obj_add_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
          ledcWrite(BUZZER_CHANNEL, 0); // Buzzer OFF
        }
      }
      xSemaphoreGive(displayMutex);
    }
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
      Serial.println("Konfigurasi buzzer disimpan");
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
            
            file.println(prayerConfig.tuneImsak);
            file.println(prayerConfig.tuneSubuh);
            file.println(prayerConfig.tuneTerbit);
            file.println(prayerConfig.tuneZuhur);
            file.println(prayerConfig.tuneAshar);
            file.println(prayerConfig.tuneMaghrib);
            file.println(prayerConfig.tuneIsya);
            
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
        
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneImsak = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneSubuh = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneTerbit = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneZuhur = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneAshar = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneMaghrib = tuneStr.toInt();
        }
        if (file.available()) {
          String tuneStr = file.readStringUntil('\n');
          tuneStr.trim();
          prayerConfig.tuneIsya = tuneStr.toInt();
        }

        file.close();
        Serial.println("Pemilihan kota dimuat: " + prayerConfig.selectedCity);
        Serial.println("Lat: " + prayerConfig.latitude + ", Lon: " + prayerConfig.longitude);
        Serial.printf("Tune: Imsak=%d, Subuh=%d, Terbit=%d, Zuhur=%d, Ashar=%d, Maghrib=%d, Isya=%d\n",
                     prayerConfig.tuneImsak, prayerConfig.tuneSubuh, prayerConfig.tuneTerbit,
                     prayerConfig.tuneZuhur, prayerConfig.tuneAshar, prayerConfig.tuneMaghrib,
                     prayerConfig.tuneIsya);
      }
    } else {
      prayerConfig.selectedCity = "";
      prayerConfig.selectedCityName = "";
      prayerConfig.latitude = "";
      prayerConfig.longitude = "";
      
      prayerConfig.tuneImsak = 0;
      prayerConfig.tuneSubuh = 0;
      prayerConfig.tuneTerbit = 0;
      prayerConfig.tuneZuhur = 0;
      prayerConfig.tuneAshar = 0;
      prayerConfig.tuneMaghrib = 0;
      prayerConfig.tuneIsya = 0;
      
      Serial.println("Pemilihan kota tidak ditemukan");
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
      Serial.println("Pemilihan metode tersimpan:");
      Serial.println("ID: " + String(methodConfig.methodId));
      Serial.println("Nama: " + methodConfig.methodName);
    } else {
      Serial.println("Gagal menyimpan pemilihan metode");
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
        Serial.println("Pemilihan metode dimuat:");
        Serial.println("ID: " + String(methodConfig.methodId));
        Serial.println("Nama: " + methodConfig.methodName);
      }
    } else {
      methodConfig.methodId = 5;
      methodConfig.methodName = "Egyptian General Authority of Survey";
      Serial.println("Pemilihan metode tidak ditemukan - menggunakan default (Mesir)");
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
        Serial.println("DS3231 tidak ditemukan!");
        Serial.println("Koneksi kabel:");
        Serial.println("  SDA -> GPIO21");
        Serial.println("  SCL -> GPIO22");
        Serial.println("  VCC -> 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("\nRunning without RTC");
        Serial.println("========================================\n");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            const time_t EPOCH_2000 = 946684800;
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = EPOCH_2000;
            
            if (timeConfig.currentTime < EPOCH_2000) {
                timeConfig.currentTime = EPOCH_2000;
            }
            
            xSemaphoreGive(timeMutex);
        }
        return false;
    }
    
    Serial.println("DS3231 detected on I2C");
    
    DateTime test = rtc.now();
    Serial.printf("Hasil uji: %02d:%02d:%02d %02d/%02d/%04d",
                 test.hour(), test.minute(), test.second(),
                 test.day(), test.month(), test.year());
    
    bool isValid = (
        test.year() >= 2000 && test.year() <= 2100 &&
        test.month() >= 1 && test.month() <= 12 &&
        test.day() >= 1 && test.day() <= 31 &&
        test.hour() >= 0 && test.hour() <= 23 &&
        test.minute() >= 0 && test.minute() <= 59 &&
        test.second() >= 0 && test.second() <= 59
    );
    
    if (!isValid) {
        Serial.println("\n*** RTC HARDWARE FAILURE ***");
        Serial.println("Chip DS3231 rusak!");
        Serial.println("Register waktu mengembalikan data rusak");
        Serial.println("Sensor suhu berfungsi: " + String(rtc.getTemperature()) + " °C");
        Serial.println("\nPossible causes:");
        Serial.println("  1. Counterfeit/clone DS3231 chip");
        Serial.println("  2. Crystal oscillator failure");
        Serial.println("  3. Internal SRAM corruption");
        Serial.println("\n>>> SOLUSI: BELI MODUL DS3231 BARU <<<");
        Serial.println("\nSistem akan berjalan tanpa RTC");
        Serial.println("Waktu akan direset ke 01/01/2000 setiap restart");
        Serial.println("Sinkronisasi NTP akan memperbaiki waktu saat WiFi terhubung");
        Serial.println("========================================\n");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            const time_t EPOCH_2000 = 946684800;
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = EPOCH_2000;
            
            if (timeConfig.currentTime < EPOCH_2000) {
                timeConfig.currentTime = EPOCH_2000;
            }
            
            xSemaphoreGive(timeMutex);
        }
        
        return false;
    }
    
    Serial.println("RTC hardware test PASSED");
    Serial.println("RTC berfungsi dengan benar");

    if (rtc.lostPower()) {
        Serial.println("\nWARNING: RTC lost power!");
        Serial.println("Baterai mungkin habis atau terputus");
        Serial.println("Waktu akan diatur dari sinkronisasi NTP");
        
        rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
    } else {
        Serial.println("\nBaterai cadangan RTC baik");
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
            Serial.println("Cek RTC: Baterai habis/kehilangan daya");
            return false;
        }
        
        DateTime rtcNow = rtc.now();
        xSemaphoreGive(i2cMutex);
        
        if (!isRTCTimeValid(rtcNow)) {
            Serial.println("Cek RTC: Waktu tidak valid");
            Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d",
                         rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                         rtcNow.day(), rtcNow.month(), rtcNow.year());
            return false;
        }
        
        return true;
    }
    
    Serial.println("Cek RTC: Tidak dapat mendapatkan mutex I2C");
    return false;
}

void saveTimeToRTC() {
    if (!rtcAvailable) {
        Serial.println("[Simpan RTC] Dilewati - RTC tidak tersedia");
        return;
    }
    
    time_t currentTime;
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentTime = timeConfig.currentTime;
        xSemaphoreGive(timeMutex);
    } else {
        Serial.println("[Simpan RTC] Gagal - Tidak dapat mendapatkan timeMutex");
        return;
    }
    
    if (currentTime < 946684800) {
        Serial.println("[Simpan RTC] Dilewati - Timestamp tidak valid (sebelum 2000)");
        Serial.printf("   Timestamp: %ld\n", currentTime);
        return;
    }
    
    uint16_t y = year(currentTime);
    uint8_t m = month(currentTime);
    uint8_t d = day(currentTime);
    uint8_t h = hour(currentTime);
    uint8_t min = minute(currentTime);
    uint8_t sec = second(currentTime);
    
    Serial.println("\n========================================");
    Serial.println("MENYIMPAN WAKTU KE RTC");
    Serial.println("========================================");
    Serial.printf("Waktu yang disimpan: %02d:%02d:%02d %02d/%02d/%04d",
                 h, min, sec, d, m, y);
    
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31 ||
        h > 23 || min > 59 || sec > 59) {
        Serial.println("ERROR: Komponen waktu tidak valid!");
        Serial.println("   Tidak dapat menyimpan ke RTC");
        Serial.println("========================================\n");
        return;
    }
    
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        DateTime dt(y, m, d, h, min, sec);
        
        rtc.adjust(dt);
        
        delay(100);
        
        xSemaphoreGive(i2cMutex);
        
        Serial.println("Data ditulis ke RTC");
    } else {
        Serial.println("ERROR: Tidak dapat mendapatkan mutex I2C");
        Serial.println("========================================\n");
        return;
    }
    
    delay(500);
    
    Serial.println("\nVerifying RTC write...");
    
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        DateTime verify = rtc.now();
        xSemaphoreGive(i2cMutex);
        
        Serial.printf("RTC sekarang menunjukkan: %02d:%02d:%02d %02d/%02d/%04d",
                     verify.hour(), verify.minute(), verify.second(),
                     verify.day(), verify.month(), verify.year());
        
        bool yearOk = (verify.year() == y);
        bool monthOk = (verify.month() == m);
        bool dayOk = (verify.day() == d);
        bool hourOk = (verify.hour() == h);
        bool minOk = (verify.minute() == min || verify.minute() == min + 1);
        
        bool saveSuccess = yearOk && monthOk && dayOk && hourOk && minOk;
        
        if (saveSuccess) {
            Serial.println("\nSIMPAN RTC BERHASIL");
            Serial.println("Komponen waktu cocok");
            Serial.println("Waktu akan bertahan saat restart");
            Serial.println("========================================\n");
        } else {
            Serial.println("\nSIMPAN RTC GAGAL");
            Serial.println("Verifikasi tidak cocok:");
            if (!yearOk) Serial.printf("   Tahun: ditulis %d, dibaca %d", y, verify.year());
            if (!monthOk) Serial.printf("   Bulan: ditulis %d, dibaca %d", m, verify.month());
            if (!dayOk) Serial.printf("   Hari: ditulis %d, dibaca %d", d, verify.day());
            if (!hourOk) Serial.printf("   Jam: ditulis %d, dibaca %d", h, verify.hour());
            if (!minOk) Serial.printf("   Menit: ditulis %d, dibaca %d", min, verify.minute());
            
            Serial.println("\nPossible causes:");
            Serial.println("  1. RTC battery weak/dead");
            Serial.println("  2. Faulty RTC hardware");
            Serial.println("  3. I2C communication issues");
            Serial.println("  4. Counterfeit DS3231 chip");
            Serial.println("\nRecommendation: Replace RTC module");
            Serial.println("========================================\n");
            
            static int failCount = 0;
            failCount++;
            if (failCount >= 3) {
                Serial.println("CRITICAL: RTC failed 3 times - disabling");
                rtcAvailable = false;
                failCount = 0;
            }
        }
    } else {
        Serial.println("ERROR: Tidak dapat mendapatkan mutex I2C untuk verifikasi");
        Serial.println("========================================\n");
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

  server.on("/css/foundation.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/css/foundation.min.css")) {
      request -> send(404, "text/plain", "CSS not found");
      return;
    }

    AsyncWebServerResponse * response = request -> beginResponse(
      LittleFS, "/css/foundation.min.css", "text/css");

    response -> addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response -> addHeader("Pragma", "no-cache");
    response -> addHeader("Expires", "0");
    response -> addHeader("Content-Type", "text/css; charset=utf-8");

    request -> send(response);
  });

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
      Serial.printf("Waktu tunggu: %lu ms", waitTime);
      Serial.println("========================================\n");

      request -> send(429, "text/plain", msg);
      return;
    }

    if (request -> hasParam("ssid", true) && request -> hasParam("password", true)) {
      String newSSID = request -> getParam("ssid", true) -> value();
      String newPassword = request -> getParam("password", true) -> value();

      Serial.println("\n========================================");
      Serial.println("Simpan Kredensial WiFi");
      Serial.println("========================================");
      Serial.println("SSID Baru: " + newSSID);

      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        wifiConfig.routerSSID = newSSID;
        wifiConfig.routerPassword = newPassword;
        xSemaphoreGive(wifiMutex);
      }

      saveWiFiCredentials();

      Serial.println("Credentials saved to LittleFS");
      Serial.println("WiFi akan menghubungkan ulang dalam 3 detik...");
      Serial.println("========================================\n");

      request -> send(200, "text/plain", "OK");

      xTaskCreate(
        restartWiFiTask,
        "WiFiRestart",
        5120,
        NULL,
        1,
        NULL
      );

    } else {
      request -> send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/setap", HTTP_POST, [](AsyncWebServerRequest * request) {
      unsigned long now = millis();
      if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
          unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);
          request->send(429, "text/plain", "Please wait " + String(waitTime / 1000) + " detik");
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
          Serial.println("Mode: Konfigurasi jaringan saja");
          
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
                      Serial.println("IP AP Baru: " + newAPIP.toString());
                  } else {
                      Serial.println("Format IP tidak valid, mempertahankan: " + newAPIP.toString());
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
                      Serial.println("Gateway Baru: " + newGateway.toString());
                  } else {
                      Serial.println("Gateway tidak valid, mempertahankan: " + newGateway.toString());
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
                      Serial.println("Subnet Baru: " + newSubnet.toString());
                  } else {
                      Serial.println("Subnet tidak valid, mempertahankan: " + newSubnet.toString());
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
          Serial.println("Mode: SSID/Password saja");
          Serial.println("Konfigurasi jaringan tidak akan berubah");
          
          if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
              Serial.println("ERROR: SSID atau password tidak ada");
              request->send(400, "text/plain", "Missing ssid or password");
              return;
          }

          String newSSID = request->getParam("ssid", true)->value();
          String newPass = request->getParam("password", true)->value();
          
          newSSID.trim();
          newPass.trim();

          if (newSSID.length() == 0) {
              Serial.println("ERROR: SSID tidak boleh kosong");
              request->send(400, "text/plain", "SSID cannot be empty");
              return;
          }

          if (newPass.length() > 0 && newPass.length() < 8) {
              Serial.println("ERROR: Password minimal 8 karakter");
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

      xTaskCreate(restartAPTask, "APRestart", 5120, NULL, 1, NULL);
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
      Serial.println("SINKRONISASI WAKTU BROWSER");
      Serial.println("========================================");
      Serial.printf("Diterima: %02d:%02d:%02d %02d/%02d/%04d\n", h, i, s, d, m, y);

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
        Serial.println("\nMenyimpan waktu ke hardware RTC...");

        saveTimeToRTC();

        delay(500);

        DateTime rtcNow = rtc.now();
        Serial.println("RTC Verification:");
        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d",
          rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
          rtcNow.day(), rtcNow.month(), rtcNow.year());

        bool rtcValid = (
          rtcNow.year() >= 2000 && rtcNow.year() <= 2100 &&
          rtcNow.month() >= 1 && rtcNow.month() <= 12 &&
          rtcNow.day() >= 1 && rtcNow.day() <= 31
        );

        if (rtcValid) {
          Serial.println("RTC saved successfully");
          Serial.println("Waktu akan bertahan saat restart");
        } else {
          Serial.println("Simpan RTC GAGAL - waktu tidak valid");
          Serial.println("Periksa baterai RTC atau koneksi I2C");
        }
      } else {
        Serial.println("\nRTC tidak tersedia - waktu akan direset saat restart");
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
      Serial.println("Simpan Timezone");
      Serial.println("========================================");

      if (!request -> hasParam("offset", true)) {
        Serial.println("ERROR: Parameter offset tidak ada");
        request -> send(400, "application/json",
          "{\"error\":\"Missing offset parameter\"}");
        return;
      }

      String offsetStr = request -> getParam("offset", true) -> value();
      offsetStr.trim();

      int offset = offsetStr.toInt();

      Serial.println("Received offset: " + String(offset));

      if (offset < -12 || offset > 14) {
        Serial.println("ERROR: Offset timezone tidak valid");
        request -> send(400, "application/json",
          "{\"error\":\"Invalid timezone offset (must be -12 to +14)\"}");
        return;
      }

      Serial.println("Menyimpan ke memori dan file...");

      if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        timezoneOffset = offset;
        xSemaphoreGive(settingsMutex);
        Serial.println("Memory updated");
      }

      bool ntpTriggered = false;
      bool prayerWillUpdate = false;

      if (wifiConfig.isConnected && ntpTaskHandle != NULL) {
        Serial.println("\n========================================");
        Serial.println("MEMICU ULANG SINKRONISASI NTP OTOMATIS");
        Serial.println("========================================");
        Serial.println("Reason: Timezone changed to UTC" + String(offset >= 0 ? "+" : "") + String(offset));

        if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
          Serial.println("Kota: " + prayerConfig.selectedCity);
          Serial.println("Koordinat: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
          Serial.println("");
          Serial.println("Tugas NTP akan otomatis:");
          Serial.println("  1. Sinkronisasi waktu dengan timezone baru");
          Serial.println("  2. Perbarui waktu shalat dengan tanggal yang benar");
          prayerWillUpdate = true;
        } else {
          Serial.println("Note: No city coordinates available");
          Serial.println("Hanya waktu yang akan disinkronkan (tanpa pembaruan waktu shalat)");
        }

        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          timeConfig.ntpSynced = false;
          xSemaphoreGive(timeMutex);
        }

        xTaskNotifyGive(ntpTaskHandle);
        ntpTriggered = true;

        Serial.println("Sinkronisasi ulang NTP berhasil dipicu");
        Serial.println("========================================\n");

      } else {
        Serial.println("\nTidak dapat memicu sinkronisasi NTP:");
        if (!wifiConfig.isConnected) {
          Serial.println("Alasan: WiFi tidak terhubung");
        }
        if (ntpTaskHandle == NULL) {
          Serial.println("Alasan: Tugas NTP tidak berjalan");
        }
        Serial.println("Timezone akan diterapkan saat koneksi berikutnya\n");
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

  server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/cities.json")) {
      Serial.println("cities.json tidak ditemukan");
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
      json += "\"hasSelection\":" + String(hasSelection ? "true" : "false") + ",";
      
      json += "\"tune\":{";
      json += "\"imsak\":" + String(prayerConfig.tuneImsak) + ",";
      json += "\"subuh\":" + String(prayerConfig.tuneSubuh) + ",";
      json += "\"terbit\":" + String(prayerConfig.tuneTerbit) + ",";
      json += "\"zuhur\":" + String(prayerConfig.tuneZuhur) + ",";
      json += "\"ashar\":" + String(prayerConfig.tuneAshar) + ",";
      json += "\"maghrib\":" + String(prayerConfig.tuneMaghrib) + ",";
      json += "\"isya\":" + String(prayerConfig.tuneIsya);
      json += "}";

      xSemaphoreGive(settingsMutex);
    } else {
      json += "\"selectedCity\":\"\",";
      json += "\"selectedCityApi\":\"\",";
      json += "\"latitude\":\"\",";
      json += "\"longitude\":\"\",";
      json += "\"hasSelection\":false,";
      json += "\"tune\":{\"imsak\":0,\"subuh\":0,\"terbit\":0,\"zuhur\":0,\"ashar\":0,\"maghrib\":0,\"isya\":0}";
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

      int tuneImsak = request->hasParam("tuneImsak", true) 
          ? request->getParam("tuneImsak", true)->value().toInt() 
          : 0;
      int tuneSubuh = request->hasParam("tuneSubuh", true) 
          ? request->getParam("tuneSubuh", true)->value().toInt() 
          : 0;
      int tuneTerbit = request->hasParam("tuneTerbit", true) 
          ? request->getParam("tuneTerbit", true)->value().toInt() 
          : 0;
      int tuneZuhur = request->hasParam("tuneZuhur", true) 
          ? request->getParam("tuneZuhur", true)->value().toInt() 
          : 0;
      int tuneAshar = request->hasParam("tuneAshar", true) 
          ? request->getParam("tuneAshar", true)->value().toInt() 
          : 0;
      int tuneMaghrib = request->hasParam("tuneMaghrib", true) 
          ? request->getParam("tuneMaghrib", true)->value().toInt() 
          : 0;
      int tuneIsya = request->hasParam("tuneIsya", true) 
          ? request->getParam("tuneIsya", true)->value().toInt() 
          : 0;

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

      if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          prayerConfig.selectedCity = cityApi;
          prayerConfig.selectedCityName = cityName;
          prayerConfig.latitude = lat;
          prayerConfig.longitude = lon;
          xSemaphoreGive(settingsMutex);
      }

      if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          prayerConfig.selectedCity = cityApi;
          prayerConfig.selectedCityName = cityName;
          prayerConfig.latitude = lat;
          prayerConfig.longitude = lon;
          
          prayerConfig.tuneImsak = tuneImsak;
          prayerConfig.tuneSubuh = tuneSubuh;
          prayerConfig.tuneTerbit = tuneTerbit;
          prayerConfig.tuneZuhur = tuneZuhur;
          prayerConfig.tuneAshar = tuneAshar;
          prayerConfig.tuneMaghrib = tuneMaghrib;
          prayerConfig.tuneIsya = tuneIsya;
          
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
          Serial.println("Ukuran JSON: " + jsonSizeStr + " bytes");
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
        Serial.printf("Nama file: %s", filename.c_str());

        if (filename != "cities.json") {
          Serial.printf("Nama file tidak valid: %s (harus cities.json)", filename.c_str());
          return;
        }

        if (LittleFS.exists("/cities.json")) {
          LittleFS.remove("/cities.json");
          Serial.println("cities.json lama dihapus");
        }

        uploadFile = LittleFS.open("/cities.json", "w");
        if (!uploadFile) {
          Serial.println("Gagal membuka file untuk ditulis");
          return;
        }

        totalSize = 0;
        uploadStartTime = millis();
        Serial.println("Writing to LittleFS...");
      }

      if (uploadFile) {
        size_t written = uploadFile.write(data, len);
        if (written != len) {
          Serial.printf("Ketidaksesuaian penulisan: %d/%d byte", written, len);
        }
        totalSize += written;

        if (totalSize % 5120 == 0 || final) {
          Serial.printf("Progress: %d byte (%.1f KB)",
            totalSize, totalSize / 1024.0);
        }
      }

      if (final) {
        if (uploadFile) {
          uploadFile.flush();
          uploadFile.close();

          unsigned long uploadDuration = millis() - uploadStartTime;

          Serial.println("\nUpload complete");
          Serial.printf("Ukuran total: %d byte (%.2f KB)",
            totalSize, totalSize / 1024.0);
          Serial.printf("Durasi: %lu ms", uploadDuration);

          vTaskDelay(pdMS_TO_TICKS(100));

          if (LittleFS.exists("/cities.json")) {
            fs::File verifyFile = LittleFS.open("/cities.json", "r");
            if (verifyFile) {
              size_t fileSize = verifyFile.size();

              char buffer[101];
              size_t bytesRead = verifyFile.readBytes(buffer, 100);
              buffer[bytesRead] = '\0';

              verifyFile.close();

              Serial.printf("File terverifikasi: %d byte", fileSize);
              Serial.println("First 100 chars:");
              Serial.println(buffer);

              String preview(buffer);
              if (preview.indexOf('[') >= 0 && preview.indexOf('{') >= 0) {
                Serial.println("JSON format looks valid");
              } else {
                Serial.println("Peringatan: File mungkin bukan JSON yang valid");
              }

              Serial.println("========================================\n");
            }
          } else {
            Serial.println("Verifikasi file gagal - file tidak ditemukan");
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
      Serial.println("Simpan Metode Perhitungan");
      Serial.print("IP Klien: ");
      Serial.println(request -> client() -> remoteIP().toString());
      Serial.println("========================================");

      if (!request -> hasParam("methodId", true) || !request -> hasParam("methodName", true)) {
        Serial.println("ERROR: Parameter tidak ada");

        int params = request -> params();
        Serial.printf("Parameter diterima (%d):", params);
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

      Serial.println("Data diterima:");
      Serial.println("Method ID: " + String(methodId));
      Serial.println("Method Name: " + methodName);

      if (methodId < 0 || methodId > 20) {
        Serial.println("ERROR: ID metode tidak valid");
        request -> send(400, "application/json",
          "{\"error\":\"Invalid method ID\"}");
        return;
      }

      if (methodName.length() == 0) {
        Serial.println("ERROR: Nama metode kosong");
        request -> send(400, "application/json",
          "{\"error\":\"Method name cannot be empty\"}");
        return;
      }

      if (methodName.length() > 100) {
        Serial.println("ERROR: Nama metode terlalu panjang");
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
              Serial.println("Memicu pembaruan waktu shalat dengan metode baru...");
              Serial.println("Kota: " + prayerConfig.selectedCity);
              Serial.println("Method: " + methodName);
              
              willFetchPrayerTimes = true;
          } else {
              Serial.println("No coordinates available");
          }
      } else {
          Serial.println("WiFi tidak terhubung");
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
                  Serial.println("Tugas Shalat dipicu untuk perubahan metode");
              }
          }

          Serial.println("Prayer times update initiated");
      }

      Serial.println("========================================");
      Serial.println("SUCCESS: Method saved successfully");
      Serial.println("Method: " + methodName);
      if (willFetchPrayerTimes) {
        Serial.println("Waktu shalat akan diperbarui sebentar lagi...");
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
    json += "\"alarm\":" + String(alarmConfig.alarmEnabled ? "true" : "false") + ",";
    json += "\"alarmTime\":\"" + String(alarmConfig.alarmTime) + "\",";
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
    else if (prayer == "alarm") {
      alarmConfig.alarmEnabled = enabled;
      lastAlarmMinute = -1; // reset trigger guard
      saveAlarmConfig();
      request->send(200, "text/plain", "OK");
      return;
    }

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
      Serial.printf("Volume: %d%%", volume);
      Serial.println("Durasi: Berhenti manual atau timeout 30 detik");
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
              
              Serial.printf("Loop uji buzzer dimulai (PWM: %d)", pwmValue);
              
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
      Serial.println("PERMINTAAN BERHENTI BUZZER");
      Serial.println("========================================\n");
      
      ledcWrite(BUZZER_CHANNEL, 0);
      
      if (buzzerTestTaskHandle != NULL) {
          vTaskDelete(buzzerTestTaskHandle);
          buzzerTestTaskHandle = NULL;
          Serial.println("Tugas uji buzzer dihapus");
      }
      
      request->send(200, "text/plain", "OK");
      Serial.println("Buzzer stopped successfully\n");
  });

  // ========================================
  // ALARM CONFIG ROUTES
  // ========================================
  server.on("/getalarmconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"alarmTime\":\"" + String(alarmConfig.alarmTime) + "\",";
    json += "\"alarmEnabled\":" + String(alarmConfig.alarmEnabled ? "true" : "false");
    json += "}";
    sendJSONResponse(request, json);
  });

  server.on("/setalarmconfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool changed = false;

    if (request->hasParam("alarmTime", true)) {
      String t = request->getParam("alarmTime", true)->value();
      t.trim();
      if (t.length() == 5 && t[2] == ':') {
        strncpy(alarmConfig.alarmTime, t.c_str(), 5);
        alarmConfig.alarmTime[5] = '\0';
        changed = true;
        Serial.println("Alarm time set: " + t);
      }
    }

    if (request->hasParam("alarmEnabled", true)) {
      String en = request->getParam("alarmEnabled", true)->value();
      alarmConfig.alarmEnabled = (en == "true" || en == "1");
      changed = true;
      Serial.println("Alarm enabled: " + String(alarmConfig.alarmEnabled ? "ON" : "OFF"));
      // Reset trigger guard saat setting diubah
      lastAlarmMinute = -1;
    }

    if (changed) saveAlarmConfig();
    request->send(200, "application/json", "{\"success\":true}");
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
      
      if (LittleFS.exists("/wifi_creds.txt"))       LittleFS.remove("/wifi_creds.txt");
      if (LittleFS.exists("/prayer_times.txt"))     LittleFS.remove("/prayer_times.txt");
      if (LittleFS.exists("/ap_creds.txt"))         LittleFS.remove("/ap_creds.txt");
      if (LittleFS.exists("/city_selection.txt"))   LittleFS.remove("/city_selection.txt");
      if (LittleFS.exists("/method_selection.txt")) LittleFS.remove("/method_selection.txt");
      if (LittleFS.exists("/timezone.txt"))         LittleFS.remove("/timezone.txt");
      if (LittleFS.exists("/buzzer_config.txt"))    LittleFS.remove("/buzzer_config.txt");
      if (LittleFS.exists("/adzan_state.txt"))      LittleFS.remove("/adzan_state.txt");
      if (LittleFS.exists("/alarm_config.txt"))     LittleFS.remove("/alarm_config.txt");
      
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

          prayerConfig.tuneImsak = 0;
          prayerConfig.tuneSubuh = 0;
          prayerConfig.tuneTerbit = 0;
          prayerConfig.tuneZuhur = 0;
          prayerConfig.tuneAshar = 0;
          prayerConfig.tuneMaghrib = 0;
          prayerConfig.tuneIsya = 0;
          
          DEFAULT_AP_SSID.toCharArray(wifiConfig.apSSID, 33);
          strcpy(wifiConfig.apPassword, DEFAULT_AP_PASSWORD);
          wifiConfig.apIP = IPAddress(192, 168, 100, 1);
          wifiConfig.apGateway = IPAddress(192, 168, 100, 1);
          wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);
          
          xSemaphoreGive(settingsMutex);
      }
      
      // Reset alarm ke default  ← TAMBAHAN
      strncpy(alarmConfig.alarmTime, "00:00", 5);
      alarmConfig.alarmTime[5] = '\0';
      alarmConfig.alarmEnabled = false;
      alarmState.isRinging = false;
      lastAlarmMinute = -1;
      ledcWrite(BUZZER_CHANNEL, 0);
      
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

    server.on("/notfound", HTTP_GET, [](AsyncWebServerRequest * request) {
      String html = "<!DOCTYPE html><html><head>";
      html += "<meta charset='UTF-8'>";
      html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
      html += "<title>404 - Halaman Tidak Ditemukan</title>";
      html += "<style>";
      html += "*{box-sizing:border-box;margin:0;padding:0}";
      html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f0f2f5;min-height:100vh;display:flex;align-items:center;justify-content:center}";
      html += ".wrap{display:flex;align-items:center;justify-content:center;padding:40px 20px}";
      html += ".card{background:#fff;border-radius:16px;padding:50px 40px;max-width:480px;width:100%;text-align:center;box-shadow:0 2px 12px rgba(0,0,0,0.08)}";
      html += ".code{font-size:80px;font-weight:700;color:#4a90d9;line-height:1;margin-bottom:16px}";
      html += ".divider{width:60px;height:3px;background:#4a90d9;border-radius:2px;margin:0 auto 20px}";
      html += "h2{font-size:20px;color:#333;font-weight:600;margin-bottom:10px}";
      html += "p{font-size:14px;color:#888;line-height:1.7;margin-bottom:30px}";
      html += ".btn{display:inline-block;padding:11px 36px;background:#4a90d9;color:#fff;text-decoration:none;border-radius:6px;font-size:14px;font-weight:500;transition:background 0.2s}";
      html += ".btn:hover{background:#357abd}";
      html += "</style></head><body>";
      html += "<div class='wrap'><div class='card'>";
      html += "<div class='code'>404</div>";
      html += "<div class='divider'></div>";
      html += "<h2>Halaman Tidak Ditemukan</h2>";
      html += "<p>Halaman yang Anda cari tidak tersedia atau tidak memiliki izin untuk diakses. Silakan kembali ke halaman utama.</p>";
      html += "<a href='/' class='btn'>Kembali ke Beranda</a>";
      html += "</div></div>";
      html += "</body></html>";
      request->send(404, "text/html", html);
    });

    server.onNotFound([](AsyncWebServerRequest * request) {
      String url = request -> url();
      IPAddress clientIP = request -> client() -> remoteIP();

      Serial.printf("[404] Klien: %s | URL: %s",
        clientIP.toString().c_str(), url.c_str());

      if (url.startsWith("/css/") || url.endsWith(".css") || url.endsWith(".js") ||
        url.endsWith(".png") || url.endsWith(".jpg") || url.endsWith(".jpeg") ||
        url.endsWith(".gif") || url.endsWith(".ico") || url.endsWith(".svg") ||
        url.endsWith(".woff") || url.endsWith(".woff2") || url.endsWith(".ttf")) {

        request -> send(404, "text/plain", "File not found");
        return;
      }

      Serial.println("URL tidak valid, mengalihkan ke /notfound");
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
  Serial.println("ANALISIS PENGGUNAAN STACK");
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
    { httpTaskHandle, "HTTP", 8192 },
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

      if (percent < 40) Serial.println("BOROS - dapat dikurangi");
      else if (percent < 60) Serial.println("OPTIMAL");
      else if (percent < 75) Serial.println("FIT");
      else if (percent < 90) Serial.println("HIGH - monitor continuously");
      else if (percent < 95) Serial.println("DANGER - must be increased");
      else Serial.println("CRITICAL - increase immediately");
    } else {
      Serial.printf("%-10s: TASK TIDAK BERJALAN", tasks[i].name);
    }
  }

  Serial.println("========================================");
  Serial.printf("Total dialokasikan: %d byte (%.1f KB)",
                totalAllocated, totalAllocated / 1024.0);
  Serial.printf("Total digunakan:    %d byte (%.1f KB)",
                totalUsed, totalUsed / 1024.0);
  Serial.printf("Total tersisa:      %d byte (%.1f KB)",
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
        Serial.printf("   %s: %.1f%% (tersisa: %d byte)", 
                      tasks[i].name, percent, free);
      }
    }
  }
  
  if (hasCritical) {
    Serial.println("   TINDAKAN: Tambah ukuran stack segera");
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

    if (validTouch && !touchPressed) {
      // ======================================
      // PRIORITAS 1: Matikan alarm dengan sentuh LCD manapun
      // ======================================
      if (alarmState.isRinging) {
        stopAlarm();
        // Jangan proses sentuhan lain
        data->state = LV_INDEV_STATE_PR;
        touchPressed = true;
        return;
      }
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
          
          if (audioTaskHandle != NULL) {
            Serial.println("Audio system available - triggering playback");
            
            adzanState.isPlaying = true;
            adzanState.canTouch = false;
            
            xTaskNotifyGive(audioTaskHandle);
            
          } else {
            Serial.println("========================================");
            Serial.println("PERINGATAN: Sistem audio tidak tersedia");
            Serial.println("========================================");
            Serial.println("Alasan: SD Card tidak terdeteksi atau audio dinonaktifkan");
            Serial.println("Action: Clearing adzan state immediately");
            Serial.println("========================================");
            
            adzanState.isPlaying = false;
            adzanState.canTouch = false;
            adzanState.currentPrayer = "";
            
            saveAdzanState();
            
            Serial.println("Status adzan dikosongkan - siap untuk waktu shalat berikutnya");
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
      
      if (update.type == DisplayUpdate::TIME_UPDATE) {
        checkPrayerTime();
        checkAlarmTime();
      }
    }
    
    handleBlinking();
    handleAlarmBlink();
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void wifiTask(void *parameter) {
    esp_task_wdt_add(NULL);
    
    Serial.println("\n========================================");
    Serial.println("Tugas WiFi Dimulai - Mode Event-Driven");
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
            
            Serial.println("\n========================================");
            Serial.println("WiFi Disconnected Event");
            Serial.println("========================================");
            
            // ============================================
            // SAFETY: SKIP EVENTS DURING RESTART
            // ============================================
            if (wifiRestartInProgress || apRestartInProgress) {
                Serial.println("WiFi/AP restart in progress - skipping auto-reconnect");
                Serial.println("Alasan: Hindari konflik dengan operasi restart manual");
                Serial.println("========================================\n");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            
            if (autoUpdateDone) {
                Serial.println("\n>>> RESET FLAG: autoUpdateDone = false <<<");
                Serial.println("Reason: WiFi disconnected");
                Serial.println("Efek: NTP & Shalat akan dipicu ulang saat koneksi berikutnya");
                autoUpdateDone = false;
            }
            
            IPAddress apIP = WiFi.softAPIP();
            if (apIP == IPAddress(0, 0, 0, 0)) {
                Serial.println("WARNING: AP died during disconnect");
                Serial.println("Restoring AP...");
                
                WiFi.softAPdisconnect(false);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                Serial.println("AP restored: " + WiFi.softAPIP().toString());
            } else {
                Serial.println("AP still alive: " + apIP.toString());
            }
            
            autoUpdateDone = false;
            ntpSyncInProgress = false;
            ntpSyncCompleted = false;
            
            wifiDisconnectedTime = millis();

            if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                wifiConfig.isConnected = false;
                wifiState = WIFI_IDLE;
                xSemaphoreGive(wifiMutex);
            }
            
            if (wifiConfig.routerSSID.length() > 0) {
                Serial.println("Attempting reconnect to: " + wifiConfig.routerSSID);
                
                esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), hostname.c_str());
                
                WiFi.begin(wifiConfig.routerSSID.c_str(), 
                          wifiConfig.routerPassword.c_str());
                wifiState = WIFI_CONNECTING;
            }
            
            Serial.println("Akan mencoba menghubungkan kembali...");
            Serial.println("========================================\n");
        }

        // ========================================
        // EVENT: WiFi Got IP (Connected)
        // ========================================
        if (bits & WIFI_GOT_IP_BIT) {
            if (!autoUpdateDone && wifiConfig.isConnected) {
                Serial.println("\n========================================");
                Serial.println("AUTO-UPDATE SEQUENCE STARTED");
                Serial.println("========================================");
                Serial.println("Trigger: WiFi just connected");
                Serial.println("autoUpdateDone: false (siap sinkronisasi)");
                
                // ============================================
                // WAIT FOR NTP SYNC START
                // ============================================
                if (!ntpSyncInProgress && !ntpSyncCompleted) {
                    Serial.println("Menunggu Tugas NTP dimulai...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    if (!ntpSyncInProgress && !ntpSyncCompleted) {
                        Serial.println("PERINGATAN: Sinkronisasi NTP belum dimulai");
                        Serial.println("Waiting additional 2 seconds...");
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
                    const unsigned long maxWaitTime = 30000;
                    
                    Serial.println("Menunggu sinkronisasi NTP selesai...");
                    
                    while (ntpSyncInProgress && (millis() - waitStartTime < maxWaitTime)) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        ntpWaitCounter++;
                        
                        if (ntpWaitCounter % 10 == 0) {
                            Serial.printf("Sinkronisasi NTP berjalan... (%lu ms berlalu)", 
                                        millis() - waitStartTime);
                            esp_task_wdt_reset();
                        }
                        
                        esp_task_wdt_reset();
                    }
                    
                    if (ntpSyncInProgress) {
                        Serial.println("Timeout sinkronisasi NTP - melanjutkan");
                    }
                    
                    continue;
                }

                // ============================================
                // NTP COMPLETE, UPDATE PRAYER TIMES
                // ============================================
                if (ntpSyncCompleted && timeConfig.ntpSynced) {
                    Serial.println("\n========================================");
                    Serial.println("SINKRONISASI NTP SELESAI");
                    Serial.println("========================================");
                    Serial.println("Berikutnya: Picu Pembaruan Waktu Shalat");

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

                        Serial.println("Waktu Saat Ini: " + String(timeStr));
                        Serial.println("Current Date: " + String(dateStr));
                        
                        if (timeinfo.tm_year + 1900 >= 2000) {
                            Serial.println("Waktu VALID - memicu pembaruan shalat");
                            Serial.println("Kota: " + prayerConfig.selectedCity);
                            Serial.println("Koordinat: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                            Serial.println("");
                            
                            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                                needPrayerUpdate = true;
                                pendingPrayerLat = prayerConfig.latitude;
                                pendingPrayerLon = prayerConfig.longitude;
                                xSemaphoreGive(settingsMutex);
                                
                                if (prayerTaskHandle != NULL) {
                                    xTaskNotifyGive(prayerTaskHandle);
                                    Serial.println("Tugas Shalat DIPICU");
                                    Serial.println("Status: Update in background");
                                } else {
                                    Serial.println("ERROR: Handle Tugas Shalat NULL");
                                }
                            }
                        } else {
                            Serial.println("ERROR: Waktu masih tidak valid (tahun < 2000)");
                            Serial.println("Skipping prayer times update");
                        }
                        
                        Serial.println("========================================\n");

                    } else {
                        Serial.println("\n========================================");
                        Serial.println("PRAYER UPDATE SKIPPED");
                        Serial.println("========================================");
                        Serial.println("Reason: No city coordinates available");
                        Serial.println("Action: Configure city via web interface");
                        Serial.println("========================================\n");
                    }

                    autoUpdateDone = true;
                    Serial.println(">>> autoUpdateDone = true <<<");
                    Serial.println("Status: Auto-update cycle completed");
                    Serial.println("Catatan: Akan direset saat pemutusan berikutnya\n");
                }
            }

            if (millis() - lastMonitor > 60000) {
                lastMonitor = millis();
                Serial.printf("[Monitor WiFi] Terhubung | RSSI: %d dBm | IP: %s",
                            WiFi.RSSI(),
                            WiFi.localIP().toString().c_str());
            }
        }

        // ========================================
        // CHECK FIRST-TIME CONNECTION
        // ========================================
        if (wifiState == WIFI_IDLE && wifiConfig.routerSSID.length() > 0) {
            bool isConnected = (bits & WIFI_CONNECTED_BIT) != 0;
            
            if (!isConnected) {
                Serial.println("\n[Tugas WiFi] Percobaan koneksi awal");
                Serial.println("SSID: " + wifiConfig.routerSSID);
                
                esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), hostname.c_str());
                
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
        
        if (restartTaskHandle != NULL || resetTaskHandle != NULL) {
            Serial.println("\n[Tugas NTP] Restart sistem terdeteksi - menangguhkan");
            vTaskSuspend(NULL);
            continue;
        }
        
        if (notifyValue == 0) {
            esp_task_wdt_reset();
            continue;
        }
        
        ntpSyncInProgress = true;
        ntpSyncCompleted = false;
        
        Serial.println("\n========================================");
        Serial.println("SINKRONISASI NTP OTOMATIS DIMULAI");
        Serial.println("========================================");

        bool syncSuccess = false;
        time_t ntpTime = 0;
        String usedServer = "";

        Serial.println("Menyinkronkan dengan server NTP (UTC+0)...");
        Serial.println("Alasan: Dapatkan waktu UTC mentah dulu, terapkan timezone SETELAH berhasil");
        Serial.println("");
        
        configTzTime("UTC0", 
                     ntpServers[0], 
                     ntpServers[1], 
                     ntpServers[2]);
        
        time_t now = 0;
        struct tm timeinfo = {0};
        int retry = 0;
        const int retry_count = 40;
        
        while (timeinfo.tm_year < (2000 - 1900) && ++retry < retry_count) {
            if (restartTaskHandle != NULL || resetTaskHandle != NULL) {
                Serial.println("\n[Tugas NTP] Shutdown terdeteksi di tengah sinkronisasi - membatalkan");
                ntpSyncInProgress = false;
                ntpSyncCompleted = false;
                vTaskSuspend(NULL);
                break;
            }
            
            vTaskDelay(pdMS_TO_TICKS(250));
            
            time(&now);
            gmtime_r(&now, &timeinfo);
            
            esp_task_wdt_reset();
            
            if (retry % 4 == 0) {
                Serial.printf("Menunggu sinkronisasi NTP... (%d/%d) [%.1f detik]", 
                             retry, retry_count, retry * 0.25);
            }
        }

        esp_task_wdt_reset();
        
        syncSuccess = (timeinfo.tm_year >= (2000 - 1900));
        
        if (syncSuccess) {
            ntpTime = now;
            usedServer = String(ntpServers[0]);
            
            Serial.println("\n========================================");
            Serial.println("SINKRONISASI NTP SELESAI (UTC)");
            Serial.println("========================================");
            Serial.printf("Waktu UTC: %02d:%02d:%02d %02d/%02d/%04d",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("Durasi sinkronisasi: %.1f detik", retry * 0.25);
            Serial.printf("Server NTP: %s", usedServer.c_str());
            Serial.println("========================================");
            
            Serial.println("\n========================================");
            Serial.println("APPLYING TIMEZONE OFFSET");
            Serial.println("========================================");
            
            int currentOffset;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                currentOffset = timezoneOffset;
                xSemaphoreGive(settingsMutex);
            } else {
                currentOffset = 7;
                Serial.println("PERINGATAN: Tidak dapat mendapatkan timezone dari pengaturan, menggunakan default UTC+7");
            }
            
            Serial.printf("Pengaturan Timezone: UTC%+d", currentOffset);
            Serial.printf("Offset: %+d jam (%+ld detik)", 
                         currentOffset, (long)(currentOffset * 3600));
            
            time_t localTime = ntpTime + (currentOffset * 3600);
            
            struct tm localTimeinfo;
            localtime_r(&localTime, &localTimeinfo);
            
            Serial.println("");
            Serial.println("Konversi Waktu:");
            Serial.printf("   Sebelum: %02d:%02d:%02d %02d/%02d/%04d (UTC+0)",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("   Sesudah: %02d:%02d:%02d %02d/%02d/%04d (UTC%+d)",
                        localTimeinfo.tm_hour, localTimeinfo.tm_min, localTimeinfo.tm_sec,
                        localTimeinfo.tm_mday, localTimeinfo.tm_mon + 1, localTimeinfo.tm_year + 1900,
                        currentOffset);
            Serial.println("========================================");
            
            ntpTime = localTime;
            
        } else {
            Serial.println("\n========================================");
            Serial.println("SINKRONISASI NTP GAGAL");
            Serial.println("========================================");
            Serial.printf("Timeout setelah %.1f detik\n", retry * 0.25);
            Serial.println("Semua server NTP gagal merespons");
            Serial.println("Kemungkinan penyebab:");
            Serial.println("  1. Tidak ada koneksi internet");
            Serial.println("  2. Firewall memblokir NTP (port 123)");
            Serial.println("  3. Masalah DNS router");
            Serial.println("  4. ISP memblokir NTP");
            Serial.println("");
            Serial.println("Sistem akan melanjutkan dengan waktu saat ini");
            Serial.println("========================================");
        }
        
        if (syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("MENYIMPAN WAKTU KE SISTEM");
            Serial.println("========================================");
            
            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                timeConfig.currentTime = ntpTime;
                setTime(timeConfig.currentTime);
                timeConfig.ntpSynced = true;
                timeConfig.ntpServer = usedServer;
                xSemaphoreGive(timeMutex);
                
                Serial.println("Waktu tersimpan ke memori sistem");
                
                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
                
                Serial.println("Display update queued");
            } else {
                Serial.println("ERROR: Gagal mendapatkan timeMutex");
                Serial.println("Waktu TIDAK tersimpan ke sistem");
            }
            
            Serial.println("========================================");
        }
        
        if (rtcAvailable && syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("MENYIMPAN WAKTU KE HARDWARE RTC");
            Serial.println("========================================");
            
            if (isRTCValid()) {
                Serial.println("Status RTC: VALID - Siap disimpan");
                Serial.println("Menyimpan waktu NTP ke RTC...");
                
                saveTimeToRTC();
                
                vTaskDelay(pdMS_TO_TICKS(500));
                
                if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    DateTime rtcNow = rtc.now();
                    xSemaphoreGive(i2cMutex);
                    
                    Serial.println("");
                    Serial.println("RTC Verification:");
                    Serial.printf("   Waktu RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                                rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                                rtcNow.day(), rtcNow.month(), rtcNow.year());
                    
                    if (isRTCTimeValid(rtcNow)) {
                        Serial.println("   Status: RTC saved successfully");
                        Serial.println("   Waktu akan bertahan saat restart");
                    } else {
                        Serial.println("   Status: Simpan RTC GAGAL");
                        Serial.println("   RTC hardware may be faulty");
                    }
                } else {
                    Serial.println("   Status: Cannot verify (I2C busy)");
                }
            } else {
                Serial.println("Status RTC: TIDAK VALID - Tidak dapat disimpan");
                Serial.println("Possible issues:");
                Serial.println("  - Battery dead/disconnected");
                Serial.println("  - Hardware failure");
                Serial.println("  - Kerusakan waktu");
                Serial.println("");
                Serial.println("Waktu NTP TIDAK akan disimpan ke RTC");
                Serial.println("Waktu akan direset saat restart");
            }
            
            Serial.println("========================================");
        } else if (syncSuccess && !rtcAvailable) {
            Serial.println("\n========================================");
            Serial.println("RTC TIDAK TERSEDIA");
            Serial.println("========================================");
            Serial.println("Hardware RTC tidak terdeteksi saat boot");
            Serial.println("Waktu akan direset ke 01/01/2000 saat restart");
            Serial.println("Consider installing DS3231 RTC module");
            Serial.println("========================================");
        }

        if (syncSuccess && wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("AUTO PRAYER TIMES UPDATE - TRIGGER");
            Serial.println("========================================");
            Serial.println("Alasan: Sinkronisasi NTP berhasil");
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
                    
                    Serial.println("Memicu Tugas Shalat untuk pembaruan...");
                    
                    if (prayerTaskHandle != NULL) {
                        xTaskNotifyGive(prayerTaskHandle);
                        Serial.println("Tugas Shalat diberi tahu - akan diperbarui di latar belakang");
                    } else {
                        Serial.println("ERROR: Handle Tugas Shalat NULL");
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
            Serial.println("Alasan: WiFi tidak terhubung");
            Serial.println("Waktu shalat akan diperbarui saat WiFi terhubung");
            Serial.println("========================================");
        }

        Serial.println("\n========================================");
        Serial.println("SIKLUS TUGAS NTP SELESAI");
        Serial.println("========================================");
        Serial.printf("Hasil: %s\n", syncSuccess ? "SUCCESS" : "FAILED");
        if (syncSuccess) {
            Serial.printf("Waktu akhir: %02d:%02d:%02d %02d/%02d/%04d\n",
                        hour(ntpTime), minute(ntpTime), second(ntpTime),
                        day(ntpTime), month(ntpTime), year(ntpTime));
            Serial.printf("Timezone: UTC%+d\n", timezoneOffset);
            Serial.printf("Server NTP: %s\n", usedServer.c_str());
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
      
      Serial.printf("STATUS MEMORI:\n");
      Serial.printf("Awal:     %6d byte (%.2f KB)\n", 
                    initialHeap, initialHeap / 1024.0);
      Serial.printf("Sekarang: %6d byte (%.2f KB)\n", 
                    currentHeap, currentHeap / 1024.0);
      Serial.printf("Terendah: %6d byte (%.2f KB)\n", 
                    lowestHeap, lowestHeap / 1024.0);
      Serial.printf("Tertinggi:%6d byte (%.2f KB)\n", 
                    highestHeap, highestHeap / 1024.0);
      Serial.printf("Puncak:   %6d byte (%.2f KB)\n", 
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
          Serial.printf("KEBOCORAN: %d byte hilang sejak pengecekan terakhir\n", leaked);
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
    Serial.println("TUGAS SHALAT DIMULAI");
    Serial.println("========================================");
    Serial.println("Ukuran Stack: 12288 byte");
    Serial.println("Menunggu pemicu...");
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
            Serial.printf("[Task Shalat] Stack tersisa: %d byte\n", stackRemaining * 4);
            
            if (stackRemaining < 1000) {
                Serial.println("PERINGATAN: Stack Tugas Shalat sangat rendah!");
            }
        }
        
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        
        if (needPrayerUpdate && pendingPrayerLat.length() > 0 && pendingPrayerLon.length() > 0) {
            esp_task_wdt_reset();
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Tugas Shalat: Dilewati - WiFi tidak terhubung");
                
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
                Serial.println("Tugas Shalat: Dilewati - Waktu sistem tidak valid");
                Serial.printf("Timestamp saat ini: %ld (sebelum 01/01/2000)\n", now_t);
                
                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = false;
                    xSemaphoreGive(settingsMutex);
                }
                continue;
            }
            
            Serial.println("\n========================================");
            Serial.println("TUGAS SHALAT: Memproses Pembaruan");
            Serial.println("========================================");
            Serial.printf("Stack sebelum HTTP: %d byte tersisa\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("Koordinat: " + pendingPrayerLat + ", " + pendingPrayerLon);
            
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
            
            Serial.printf("Stack setelah HTTP: %d byte tersisa\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("Tugas Shalat: Pembaruan selesai");
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
                Serial.printf("Waktu: %02d:%02d:%02d\n", currentHour, currentMinute, second(timeConfig.currentTime));
                Serial.printf("Tanggal: %02d/%02d/%04d\n", currentDay, month(timeConfig.currentTime), currentYear);
                Serial.println("");
                
                Serial.println("Memicu Sinkronisasi NTP...");
                Serial.println("Alasan: Memastikan waktu akurat sebelum memperbarui");
                
                if (ntpTaskHandle != NULL) {
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;
                    
                    xTaskNotifyGive(ntpTaskHandle);
                    
                    waitingForMidnightNTP = true;
                    midnightNTPStartTime = millis();
                    
                    Serial.println("Sinkronisasi NTP berhasil dipicu");
                    Serial.println("Menunggu sinkronisasi NTP selesai...");
                    Serial.println("========================================\n");
                } else {
                    Serial.println("ERROR: Handle Tugas NTP NULL");
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
                    Serial.println("SINKRONISASI NTP SELESAI");
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
                        
                        Serial.printf("Waktu baru: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
                        Serial.printf("Tanggal baru: %02d/%02d/%04d\n", currentDay, currentMonth, currentYear);
                        Serial.printf("Timestamp: %ld\n", currentTimestamp);
                        Serial.println("");
                    }
                    
                    if (prayerConfig.latitude.length() > 0 && 
                        prayerConfig.longitude.length() > 0) {
                        
                        if (currentYear >= 2000 && currentTimestamp >= 946684800) {
                            Serial.println("Updating Prayer Times...");
                            Serial.println("Status Waktu: VALID");
                            Serial.println("Kota: " + prayerConfig.selectedCity);
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
                            Serial.println("PERINGATAN: Waktu masih tidak valid setelah NTP");
                            Serial.printf("   Tahun: %d (min: 2000)\n", currentYear);
                            Serial.printf("   Timestamp: %ld (min: 946684800)\n", currentTimestamp);
                            Serial.println("   Menggunakan waktu shalat yang ada");
                        }
                    } else {
                        Serial.println("WARNING: No city coordinates");
                        Serial.println("   Menggunakan waktu shalat yang ada");
                    }
                    
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else if (waitTime > MAX_WAIT_TIME) {
                    Serial.println("\n========================================");
                    Serial.println("TIMEOUT SINKRONISASI NTP");
                    Serial.println("========================================");
                    Serial.printf("Waktu tunggu: %lu ms (maks: %lu ms)\n", waitTime, MAX_WAIT_TIME);
                    Serial.println("Status NTP:");
                    Serial.printf("   ntpSyncInProgress: %s\n", ntpSyncInProgress ? "true" : "false");
                    Serial.printf("   ntpSyncCompleted: %s\n", ntpSyncCompleted ? "false" : "false");
                    Serial.println("");
                    Serial.println("Keputusan: Gunakan waktu shalat yang ada");
                    Serial.println("Jangan perbarui (waktu mungkin tidak akurat)");
                    Serial.println("========================================\n");
                    
                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;
                    
                } else {
                    if (waitTime % 5000 < 1000) {
                        Serial.printf("Menunggu sinkronisasi NTP... (%lu/%lu ms)\n",
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
    Serial.println("TUGAS SINKRONISASI RTC DIMULAI");
    Serial.println("========================================");
    Serial.println("Fungsi: Sinkronisasi waktu sistem DARI RTC");
    Serial.println("Interval: Every 1 minute");
    Serial.println("Tujuan: Sumber waktu cadangan saat WiFi tidak tersedia");
    Serial.println("========================================\n");
    
    while (true) {
        if (rtcAvailable) {
            if (!isRTCValid()) {
                Serial.println("\n[Sinkronisasi RTC] Dilewati - RTC tidak valid");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }
            
            DateTime rtcTime;
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                rtcTime = rtc.now();
                xSemaphoreGive(i2cMutex);
            } else {
                Serial.println("\n[Sinkronisasi RTC] Dilewati - Tidak dapat mendapatkan mutex I2C");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }

            if (!isRTCTimeValid(rtcTime)) {
                Serial.println("\n[Sinkronisasi RTC] Dilewati - Waktu RTC tidak valid");
                Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                             rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                             rtcTime.day(), rtcTime.month(), rtcTime.year());
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }

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

            bool shouldSync = false;
            String reason = "";
            
            if (timeDiff <= 2) {
            } else if (!ntpSynced && timeDiff > 2) {
                shouldSync = true;
                reason = "NTP not synced, using RTC as primary source";
                
            } else if (ntpSynced && systemTime > rtcUnix) {
                shouldSync = false;
                reason = "System time newer (from NTP), skip RTC sync";
                
                Serial.println("\n[Sinkronisasi RTC] Dilewati");
                Serial.println("Alasan: " + reason);
                Serial.printf("   Sistem: %02d:%02d:%02d %02d/%02d/%04d (dari NTP)\n",
                             hour(systemTime), minute(systemTime), second(systemTime),
                             day(systemTime), month(systemTime), year(systemTime));
                Serial.printf("   RTC:    %02d:%02d:%02d %02d/%02d/%04d (lebih lama)\n",
                             rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                             rtcTime.day(), rtcTime.month(), rtcTime.year());
                Serial.println("   Aksi: RTC akan diperbarui pada sinkronisasi NTP berikutnya\n");
                
            } else {
                shouldSync = true;
                reason = "RTC time more accurate, correcting system time";
            }
            
            if (shouldSync) {
                if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
                    timeConfig.currentTime = rtcUnix;
                    setTime(rtcUnix);
                    xSemaphoreGive(timeMutex);
                    
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                    
                    Serial.println("\n========================================");
                    Serial.println("WAKTU SISTEM DISINKRONKAN DARI RTC");
                    Serial.println("========================================");
                    Serial.println("Alasan: " + reason);
                    Serial.printf("Sistem lama: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 hour(systemTime), minute(systemTime), second(systemTime),
                                 day(systemTime), month(systemTime), year(systemTime));
                    Serial.printf("Sistem baru: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                 rtcTime.day(), rtcTime.month(), rtcTime.year());
                    Serial.printf("Selisih waktu: %d detik\n", timeDiff);
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
            if (timeConfig.currentTime < EPOCH_2000) { 
                Serial.println("\nPERINGATAN TUGAS JAM:");
                Serial.printf("  Timestamp tidak valid: %ld\n", timeConfig.currentTime);
                Serial.println("  Ini sebelum 01/01/2000 00:00:00");
                Serial.println("  Memaksa reset ke: 01/01/2000 00:00:00");
                
                setTime(0, 0, 0, 1, 1, 2000);
                timeConfig.currentTime = now();
                
                if (timeConfig.currentTime < EPOCH_2000) {
                    Serial.println("TimeLib.h issue - using hardcoded timestamp");
                    timeConfig.currentTime = EPOCH_2000;
                }
                
                Serial.printf("Waktu dikoreksi ke: %ld (01/01/2000 00:00:00)\n", 
                             timeConfig.currentTime);
            } else {
                timeConfig.currentTime++;
            }
            
            xSemaphoreGive(timeMutex);
            
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
        }

        if (wifiConfig.isConnected) {
            autoSyncCounter++;
            if (autoSyncCounter >= 3600) {
                autoSyncCounter = 0;
                if (ntpTaskHandle != NULL) {
                    Serial.println("\nSinkronisasi NTP otomatis (per jam)");
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
        Serial.printf("Alasan: Restart terakhir %lu ms yang lalu\n", now - lastWiFiRestartRequest);
        Serial.printf("Interval minimum: %lu ms\n", RESTART_DEBOUNCE_MS);
        Serial.printf("Harap tunggu: %lu ms (%.1f detik)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    lastWiFiRestartRequest = now;
    
    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
        if (wifiRestartMutex == NULL) {
            Serial.println("ERROR: Gagal membuat wifiRestartMutex");
            vTaskDelete(NULL);
            return;
        }
    }
    
    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("\n========================================");
        Serial.println("WiFi RESTART BLOCKED");
        Serial.println("========================================");
        Serial.println("Reason: Another WiFi/AP restart in progress");
        Serial.println("Aksi: Permintaan diabaikan untuk keamanan");
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    if (wifiRestartInProgress || apRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("WiFi RESTART ABORTED");
        Serial.println("========================================");
        Serial.printf("Restart WiFi sedang berjalan: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("Restart AP sedang berjalan: %s\n", apRestartInProgress ? "YES" : "NO");
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
    Serial.println("Mode: Koneksi ulang aman (tanpa pergantian mode)");
    Serial.println("========================================\n");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    Serial.println("Mempersiapkan koneksi ulang...");
    
    String ssid, password;
    if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ssid = wifiConfig.routerSSID;
        password = wifiConfig.routerPassword;
        wifiConfig.isConnected = false;
        wifiState = WIFI_IDLE;
        reconnectAttempts = 0;
        xSemaphoreGive(wifiMutex);
        
        Serial.println("   Kredensial dimuat dari memori");
        Serial.println("   SSID: " + ssid);
        Serial.println("   Connection state reset");
    } else {
        Serial.println("   ERROR: Gagal mendapatkan wifiMutex");
        wifiRestartInProgress = false;
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    Serial.println("\nMemutuskan WiFi lama...");
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
            Serial.println("   ERROR: Gagal memulihkan AP");
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
        Serial.println("Monitor: Tugas WiFi akan menangani koneksi");
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
    Serial.println("Kunci dilepas - sistem siap untuk permintaan berikutnya\n");
    
    vTaskDelete(NULL);
}

void restartAPTask(void *parameter) {
    unsigned long now = millis();
    if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
        unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);
        
        Serial.println("\n========================================");
        Serial.println("AP RESTART REJECTED - TOO FAST");
        Serial.println("========================================");
        Serial.printf("Alasan: Restart terakhir %lu ms yang lalu\n", now - lastAPRestartRequest);
        Serial.printf("Interval minimum: %lu ms\n", RESTART_DEBOUNCE_MS);
        Serial.printf("Harap tunggu: %lu ms (%.1f detik)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    lastAPRestartRequest = now;
    
    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
        if (wifiRestartMutex == NULL) {
            Serial.println("ERROR: Gagal membuat wifiRestartMutex");
            vTaskDelete(NULL);
            return;
        }
    }
    
    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("\n========================================");
        Serial.println("AP RESTART BLOCKED");
        Serial.println("========================================");
        Serial.println("Reason: Another WiFi/AP restart in progress");
        Serial.println("Aksi: Permintaan diabaikan untuk keamanan");
        Serial.println("========================================\n");
        
        vTaskDelete(NULL);
        return;
    }
    
    if (apRestartInProgress || wifiRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("AP RESTART ABORTED");
        Serial.println("========================================");
        Serial.printf("Restart WiFi sedang berjalan: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("Restart AP sedang berjalan: %s\n", apRestartInProgress ? "YES" : "NO");
        Serial.println("========================================\n");
        
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }
    
    apRestartInProgress = true;
    
    Serial.println("\n========================================");
    Serial.println("TUGAS RESTART AP DIMULAI");
    Serial.println("========================================");
    Serial.println("Countdown before AP shutdown");
    Serial.println("========================================\n");
    
    for (int i = 60; i > 0; i--) {
        if (i == 35) {
            Serial.println("\n========================================");
            Serial.println("SHUTTING DOWN AP");
            Serial.println("========================================");
            
            int clientsBefore = WiFi.softAPgetStationNum();
            Serial.printf("Klien terhubung: %d\n", clientsBefore);
            
            if (clientsBefore > 0) {
                Serial.println("Disconnecting clients...");
                esp_wifi_deauth_sta(0);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            WiFi.mode(WIFI_MODE_STA);
            WiFi.softAPdisconnect(true);
            
            Serial.println("AP shutdown complete");
            Serial.println("Semua klien terputus");
            Serial.println("========================================\n");
        }
        
        if (i % 10 == 0 || i <= 5) {
            Serial.printf("AP akan restart dalam %d detik...\n", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Serial.println("\n========================================");
    Serial.println("HITUNG MUNDUR SELESAI - MEMULAI AP BARU SEKARANG");
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
        
        Serial.println("Konfigurasi baru dimuat:");
        Serial.println("  SSID: " + String(savedSSID));
        Serial.println("  IP: " + savedAPIP.toString());
    } else {
        Serial.println("ERROR: Tidak dapat memuat konfigurasi - menggunakan default");
        DEFAULT_AP_SSID.toCharArray(savedSSID, sizeof(savedSSID));
        strncpy(savedPassword, DEFAULT_AP_PASSWORD, sizeof(savedPassword));
        savedAPIP = IPAddress(192, 168, 100, 1);
        savedGateway = IPAddress(192, 168, 100, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);
    }

    WiFi.mode(WIFI_MODE_APSTA);
    
    Serial.println("\nMengonfigurasi jaringan AP baru...");
    WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    Serial.println("Memulai siaran AP baru...");
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
        
        DEFAULT_AP_SSID.toCharArray(savedSSID, sizeof(savedSSID));
        strcpy(savedPassword, DEFAULT_AP_PASSWORD);
        savedAPIP = IPAddress(192, 168, 100, 1);
        savedGateway = IPAddress(192, 168, 100, 1);
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
    
    Serial.println("Tugas restart AP selesai\n");
    vTaskDelete(NULL);
}

bool initDFPlayer() {
  Serial.println("\n========================================");
  Serial.println("INITIALIZING DFPlayer Mini");
  Serial.println("========================================");
  Serial.println("UART2: TX=GPIO25, RX=GPIO32");
  
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(500);
  
  if (!dfPlayer.begin(dfSerial, true, true)) {
    Serial.println("DFPlayer connection FAILED!");
    Serial.println("Periksa koneksi kabel:");
    Serial.println("  ESP32 TX (GPIO25) → DFPlayer RX");
    Serial.println("  ESP32 RX (GPIO32) → DFPlayer TX");
    Serial.println("  VCC → 5V");
    Serial.println("  GND → GND");
    Serial.println("  Speaker → SPK_1 & SPK_2 or Amplifier DAC_R & DAC_L");
    Serial.println("========================================\n");
    return false;
  }
  
  delay(200);
  
  dfPlayer.setTimeOut(500);
  dfPlayer.volume(15);
  dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  
  delay(200);
  
  int fileCount = dfPlayer.readFileCounts();
  
  Serial.println("DFPlayer initialized successfully!");
  Serial.println("UART2: TX=GPIO25, RX=GPIO32");
  Serial.println("Volume: 15/30");
  Serial.println("Files on SD: " + String(fileCount));
  Serial.println("========================================\n");
  
  return true;
}

void setDFPlayerVolume(int vol) {
  if (!dfPlayerAvailable) return;
  
  int dfVol = map(vol, 0, 100, 0, 30);
  dfPlayer.volume(dfVol);
  Serial.println("DFPlayer volume: " + String(dfVol) + "/30");
}

void playDFPlayerAdzan(String prayerName) {
  if (!dfPlayerAvailable) {
    Serial.println("DFPlayer tidak tersedia");
    return;
  }
  
  int trackNumber = 0;
  
  if (prayerName == "subuh") trackNumber = 1;
  else if (prayerName == "zuhur") trackNumber = 2;
  else if (prayerName == "ashar") trackNumber = 3;
  else if (prayerName == "maghrib") trackNumber = 4;
  else if (prayerName == "isya") trackNumber = 5;
  
  if (trackNumber == 0) {
    Serial.println("Nama shalat tidak valid: " + prayerName);
    return;
  }
  
  Serial.println("\n========================================");
  Serial.println("PLAYING ADZAN: " + prayerName);
  Serial.println("========================================");
  Serial.println("Track: " + String(trackNumber));
  Serial.println("File: /000" + String(trackNumber) + ".mp3");
  Serial.println("========================================\n");
  
  dfPlayer.play(trackNumber);
}

bool isDFPlayerPlaying() {
  if (!dfPlayerAvailable) return false;
  
  int state = dfPlayer.readState();
  return (state == 1);
}

void audioTask(void *parameter) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    if (adzanState.isPlaying && adzanState.currentPrayer.length() > 0) {
      Serial.println("Tugas audio dipicu untuk: " + adzanState.currentPrayer);
      
      playDFPlayerAdzan(adzanState.currentPrayer);
      
      unsigned long startTime = millis();
      const unsigned long maxDuration = 600000;
      
      while (isDFPlayerPlaying() && (millis() - startTime < maxDuration)) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_task_wdt_reset();
      }
      
      Serial.println("Adzan playback completed");
      
      adzanState.isPlaying = false;
      adzanState.canTouch = false;
      adzanState.currentPrayer = "";
      saveAdzanState();
      
      Serial.println("Adzan state cleared");
    }
  }
}

// ============================================
// HTTP Task - Dedicated untuk API Requests
// ============================================
void httpTask(void *parameter) {
  esp_task_wdt_add(NULL);
  
  Serial.println("\n========================================");
  Serial.println("TUGAS HTTP DIMULAI");
  Serial.println("========================================");
  Serial.println("Ukuran Stack: 8192 byte");
  Serial.println("Purpose: Handle prayer times API requests");
  Serial.println("========================================\n");
  
  HTTPRequest request;
  
  while (true) {
    esp_task_wdt_reset();
    
    if (xQueueReceive(httpQueue, &request, pdMS_TO_TICKS(10000)) == pdTRUE) {
      esp_task_wdt_reset();
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Tugas HTTP: WiFi tidak terhubung - melewati permintaan");
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      
      Serial.println("\n========================================");
      Serial.println("TUGAS HTTP: Memproses Waktu Shalat");
      Serial.println("========================================");
      Serial.println("Koordinat: " + request.latitude + ", " + request.longitude);
      
      time_t now_t;
      if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        now_t = timeConfig.currentTime;
        xSemaphoreGive(timeMutex);
      } else {
        now_t = time(nullptr);
      }
      
      if (now_t < 946684800) {
        Serial.println("Waktu sistem tidak valid - dilewati");
        continue;
      }
      
      char dateStr[12];
      sprintf(dateStr, "%02d-%02d-%04d", day(now_t), month(now_t), year(now_t));
      
      int currentMethod = methodConfig.methodId;
      String tuneParam = String(prayerConfig.tuneImsak) + "," +      // Imsak
                        String(prayerConfig.tuneSubuh) + "," +       // Fajr
                        String(prayerConfig.tuneTerbit) + "," +      // Sunrise
                        String(prayerConfig.tuneZuhur) + "," +       // Dhuhr
                        String(prayerConfig.tuneAshar) + "," +       // Asr
                        String(prayerConfig.tuneMaghrib) + "," +     // Maghrib
                        "0," +                                       // Sunset
                        String(prayerConfig.tuneIsya) + "," +        // Isha
                        "0";                                         // Midnight

      String url = "http://api.aladhan.com/v1/timings/" + String(dateStr) + 
                  "?latitude=" + request.latitude + 
                  "&longitude=" + request.longitude + 
                  "&method=" + String(currentMethod) +
                  "&tune=" + tuneParam;

      Serial.println("URL: " + url);
      
      HTTPClient http;
      WiFiClient client;
      
      http.begin(client, url);
      http.setTimeout(20000);
      
      esp_task_wdt_reset();
      
      int httpResponseCode = http.GET();
      
      esp_task_wdt_reset();
      
      Serial.println("Response code: " + String(httpResponseCode));
      
      if (httpResponseCode == 200) {
        String payload = http.getString();
        
        esp_task_wdt_reset();
        
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
            
            Serial.println("Prayer times updated successfully");
            savePrayerTimes();
            
            DisplayUpdate update;
            update.type = DisplayUpdate::PRAYER_UPDATE;
            xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
          } else {
            Serial.println("Data waktu shalat tidak valid");
          }
        } else {
          Serial.println("Error parse JSON: " + String(error.c_str()));
        }
      } else {
        Serial.println("HTTP request failed: " + String(httpResponseCode));
      }
      
      http.end();
      client.stop();
      
      Serial.println("Tugas HTTP: Permintaan selesai");
      Serial.println("========================================\n");
      
      esp_task_wdt_reset();
    }
    
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ================================
// SETUP - ESP32 CORE 3.x
// ================================
void setup() {
#if !PRODUCTION
  Serial.begin(115200);
  delay(1000);
#endif

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("ESP32 Islamic Prayer Clock");
  Serial.println("LVGL 9.2.0 + FreeRTOS");
  Serial.println("CONCURRENT ACCESS OPTIMIZED");
  Serial.println("VERSI 2.3 - TUGAS HTTP DIPISAHKAN");
  Serial.println("========================================\n");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  Serial.println("Backlight: OFF");

  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  Serial.println("TFT initialized");

  displayMutex = xSemaphoreCreateMutex();
  timeMutex = xSemaphoreCreateMutex();
  wifiMutex = xSemaphoreCreateMutex();
  settingsMutex = xSemaphoreCreateMutex();
  spiMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();
  audioMutex = xSemaphoreCreateMutex();
  alarmMutex = xSemaphoreCreateMutex();

  displayQueue = xQueueCreate(20, sizeof(DisplayUpdate));
  httpQueue = xQueueCreate(5, sizeof(HTTPRequest));
  
  Serial.println("Semaphores & Queues created");

  dfPlayerAvailable = initDFPlayer();

  if (dfPlayerAvailable) {
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
    Serial.println("Tugas Audio OK");
  } else {
    Serial.println("DFPlayer DISABLED - no audio playback");
    Serial.println("Clearing any pending adzan state...");
    
    adzanState.isPlaying = false;
    adzanState.canTouch = false;
    adzanState.currentPrayer = "";
    
    if (LittleFS.exists("/adzan_state.txt")) {
      LittleFS.remove("/adzan_state.txt");
      Serial.println("File status adzan dihapus (tidak ada sistem audio)");
    }
    
    Serial.println("Status adzan dibersihkan - mode hanya buzzer aktif");
  }

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
  loadAlarmConfig();

  Wire.begin(/*RTC_SDA, RTC_SCL*/);
  delay(500);

  Wire.beginTransmission(0x68);
  Wire.endTransmission();

  rtcAvailable = initRTC();

  if (rtcAvailable) {
      Serial.println("\nRTC tersedia");
      Serial.println("Waktu berhasil dimuat dari RTC");
      Serial.println("Waktu akan bertahan saat restart");
  } else {
      Serial.println("\nRTC tidak tersedia - waktu akan direset saat restart");
      
      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
          const time_t EPOCH_2000 = 946684800;
          setTime(0, 0, 0, 1, 1, 2000);
          timeConfig.currentTime = EPOCH_2000;
          
          if (timeConfig.currentTime < EPOCH_2000) {
              timeConfig.currentTime = EPOCH_2000;
          }
          
          xSemaphoreGive(timeMutex);
          Serial.printf("Waktu awal diatur: %ld (01/01/2000 00:00:00 UTC)\n", EPOCH_2000);
      }
  }

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  Serial.println("Touch initialized");

  ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcWrite(BUZZER_CHANNEL, 0);
  Serial.println("Buzzer initialized (GPIO26)");

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

  ui_init();
  Serial.println("EEZ UI initialized");

  hideAllUIElements();
  Serial.println("UI elements hidden");

  delay(100);
  lv_timer_handler();
  delay(100);
  lv_timer_handler();
  delay(100);

  tft.fillScreen(TFT_BLACK);

  Serial.println("UI rendered (black screen)");

  Serial.println("Starting backlight...");

  ledcAttach(TFT_BL, TFT_BL_FREQ, TFT_BL_RESOLUTION);
  ledcWrite(TFT_BL, TFT_BL_BRIGHTNESS);
  Serial.printf("Lampu latar ON: %d/255\n", TFT_BL_BRIGHTNESS);

  Serial.println("\n========================================");
  Serial.println("WIFI CONFIGURATION");
  Serial.println("========================================");

  setupWiFiEvents();

  WiFi.mode(WIFI_OFF);
  delay(500);

  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif != NULL) {
    esp_netif_set_hostname(sta_netif, hostname.c_str());
    Serial.print("Hostname diatur via ESP-IDF: ");
    Serial.println(hostname.c_str());
  } else {
    Serial.println("PERINGATAN: Tidak dapat mendapatkan handle STA netif");
  }

  WiFi.mode(WIFI_AP_STA);
  delay(100);

  Serial.println("Menerapkan optimasi WiFi untuk akses router...");

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.println("  Protocol: 802.11 b/g/n");
  Serial.println("  Bandwidth: 40MHz (HT40)");
  Serial.println("  TX Power: 19.5dBm (max)");

  WiFi.setSleep(WIFI_PS_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_wifi_set_max_tx_power(78);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.println("Mode WiFi: AP + STA");
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

  Serial.printf("AP Dimulai: %s\n", wifiConfig.apSSID);
  Serial.printf("Password: %s\n", wifiConfig.apPassword);
  Serial.print("IP AP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("MAC AP: %s\n", WiFi.softAPmacAddress().c_str());

  timeConfig.ntpServer = "pool.ntp.org";
  timeConfig.ntpSynced = false;

  if (prayerConfig.selectedCity.length() > 0) {
    Serial.println("\nSelected City: " + prayerConfig.selectedCity);
    Serial.println("\nLoaded Prayer Times:");
    Serial.println("Kota: " + prayerConfig.selectedCity);
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

  Serial.println("\nMengkonfigurasi Watchdog...");

  esp_task_wdt_deinit();

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 100000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
  if (wdt_err == ESP_OK) {
    Serial.println("Watchdog dikonfigurasi (timeout 100 detik)");
  } else {
    Serial.printf("Error inisialisasi Watchdog: %s\n", esp_err_to_name(wdt_err));
  }

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
  Serial.printf("Task UI (Core 1) - Stack: %d byte\n", UI_TASK_STACK_SIZE);

  // ================================
  // WIFI TASK
  // ================================
  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFi",
    WIFI_TASK_STACK_SIZE,
    NULL,
    WIFI_TASK_PRIORITY,
    &wifiTaskHandle,
    0  // Core 0
  );
  Serial.printf("Task WiFi (Core 0) - Stack: %d byte\n", WIFI_TASK_STACK_SIZE);

  // ================================
  // NTP TASK
  // ================================
  xTaskCreatePinnedToCore(
    ntpTask,
    "NTP",
    NTP_TASK_STACK_SIZE,
    NULL,
    NTP_TASK_PRIORITY,
    &ntpTaskHandle,
    0  // Core 0
  );
  Serial.printf("Task NTP (Core 0) - Stack: %d byte\n", NTP_TASK_STACK_SIZE);

  // ================================
  // WEB TASK
  // ================================
  xTaskCreatePinnedToCore(
    webTask,
    "Web",
    WEB_TASK_STACK_SIZE,
    NULL,
    WEB_TASK_PRIORITY,
    &webTaskHandle,
    0  // Core 0
  );
  Serial.printf("Task Web (Core 0) - Stack: %d byte\n", WEB_TASK_STACK_SIZE);

  // ================================
  // HTTP TASK
  // ================================
  xTaskCreatePinnedToCore(
    httpTask,
    "HTTP",
    8192,
    NULL,
    0,
    &httpTaskHandle,
    0 // Core 0
  );
  Serial.printf("Task HTTP (Core 0) - Stack: 8192 byte\n");
  
  if (httpTaskHandle) {
    esp_task_wdt_add(httpTaskHandle);
    Serial.println("  Tugas HTTP WDT terdaftar");
  }

  // ================================
  // PRAYER TASK
  // ================================
  xTaskCreatePinnedToCore(
    prayerTask,
    "Prayer",
    PRAYER_TASK_STACK_SIZE,
    NULL,
    PRAYER_TASK_PRIORITY,
    &prayerTaskHandle,
    0  // Core 0
  );
  Serial.printf("Task Shalat (Core 0) - Stack: %d byte\n", PRAYER_TASK_STACK_SIZE);

  if (prayerTaskHandle) {
    esp_task_wdt_add(prayerTaskHandle);
    Serial.println("  Tugas Shalat WDT terdaftar");
  }

  // ================================
  // CLOCK TASK
  // ================================
  xTaskCreatePinnedToCore(
    clockTickTask,
    "Clock",
    CLOCK_TASK_STACK_SIZE,
    NULL,
    CLOCK_TASK_PRIORITY,
    NULL,
    0  // Core 0
  );
  Serial.printf("Task Jam (Core 0) - Stack: %d byte\n", CLOCK_TASK_STACK_SIZE);

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
    Serial.printf("Task Sinkronisasi RTC (Core 0) - Stack: %d byte\n", RTC_TASK_STACK_SIZE);
  }

  // ================================
  // PRAYER WATCHDOG TASK
  // ================================
  xTaskCreate(
    [](void* param) {
      const TickType_t checkInterval = pdMS_TO_TICKS(30000);
      
      Serial.println("Tugas Watchdog Shalat - Memantau setiap 30 detik");
      
      while (true) {
        vTaskDelay(checkInterval);
        
        if (prayerTaskHandle != NULL) {
          eTaskState state = eTaskGetState(prayerTaskHandle);
          
          if (state == eDeleted || state == eInvalid) {
            Serial.println("\n========================================");
            Serial.println("KRITIS: TUGAS SHALAT CRASH");
            Serial.println("========================================");
            Serial.println("Detected state: " + String(state == eDeleted ? "DELETED" : "INVALID"));
            Serial.println("Aksi: Memulai ulang tugas otomatis...");
            Serial.println("========================================");
            
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
              Serial.println("\nTugas Shalat berhasil dimulai ulang");
              Serial.println("Stack: " + String(PRAYER_TASK_STACK_SIZE) + " bytes");
              Serial.println("WDT: Re-registered");
              Serial.println("========================================\n");
            } else {
              Serial.println("\nGAGAL memulai ulang Tugas Shalat!");
              Serial.println("System may be unstable");
              Serial.println("========================================\n");
            }
          }
        } else {
          Serial.println("\nPERINGATAN: Handle Tugas Shalat NULL");
          Serial.println("Mencoba membuat tugas...");
          
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
            Serial.println("Tugas Shalat berhasil dibuat\n");
          }
        }
      }
    },
    "PrayerWatchdog",
    2048,
    NULL,
    1,
    NULL
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  Serial.println("\nRegistering tasks to watchdog:");
  
  if (wifiTaskHandle) {
    esp_task_wdt_add(wifiTaskHandle);
    Serial.println("  Tugas WiFi");
  }
  if (webTaskHandle) {
    esp_task_wdt_add(webTaskHandle);
    Serial.println("  Tugas Web");
  }
  if (ntpTaskHandle) {
    esp_task_wdt_add(ntpTaskHandle);
    Serial.println("  Tugas NTP");
  }

  Serial.println("========================================\n");

  Serial.println("========================================");
  Serial.println("MEMORY REPORT");
  Serial.println("========================================");
  
  size_t freeHeap = ESP.getFreeHeap();
  size_t totalHeap = ESP.getHeapSize();
  size_t usedHeap = totalHeap - freeHeap;
  
  Serial.printf("Total Heap:  %d byte (%.2f KB)\n", totalHeap, totalHeap / 1024.0);
  Serial.printf("Heap Terpakai: %d byte (%.2f KB)\n", usedHeap, usedHeap / 1024.0);
  Serial.printf("Heap Tersisa: %d byte (%.2f KB)\n", freeHeap, freeHeap / 1024.0);
  Serial.printf("Usage:       %.1f%%\n", (usedHeap * 100.0) / totalHeap);
  
  Serial.println("\nAlokasi Stack Tugas:");
  uint32_t totalStack = UI_TASK_STACK_SIZE + WIFI_TASK_STACK_SIZE + 
                        NTP_TASK_STACK_SIZE + WEB_TASK_STACK_SIZE + 
                        PRAYER_TASK_STACK_SIZE + CLOCK_TASK_STACK_SIZE +
                        8192;
  if (rtcAvailable) totalStack += RTC_TASK_STACK_SIZE;
  
  Serial.printf("Total:        %d byte (%.2f KB)\n", totalStack, totalStack / 1024.0);
  Serial.printf("Task UI:      %d byte\n", UI_TASK_STACK_SIZE);
  Serial.printf("Task Web:     %d byte\n", WEB_TASK_STACK_SIZE);
  Serial.printf("Task HTTP:    8192 byte (BARU)\n");
  Serial.printf("Task Shalat:  %d byte (dikurangi dari 16384)\n", PRAYER_TASK_STACK_SIZE);
  Serial.println("========================================\n");

  Serial.println("========================================");
  Serial.println("SYSTEM READY");
  Serial.println("========================================");
  Serial.println("Multi-client concurrent access enabled");
  Serial.println("WiFi sleep dinonaktifkan untuk respons lebih baik");
  Serial.println("Router optimization active (keep-alive)");
  Serial.println("Pemulihan otomatis Tugas Shalat diaktifkan");
  Serial.println("Tugas HTTP dipisahkan (non-blocking)");
  Serial.println("Pemantauan stack aktif");
  Serial.println("Memory optimized (saved ~8KB)");
  Serial.println("========================================\n");

  if (wifiConfig.routerSSID.length() > 0) {
    Serial.println("WiFi dikonfigurasi, akan auto-connect...");
    Serial.println("SSID: " + wifiConfig.routerSSID);
  } else {
    Serial.println("Hubungkan ke AP untuk konfigurasi:");
    Serial.println("1. WiFi: " + String(wifiConfig.apSSID));
    Serial.println("2. Password: " + String(wifiConfig.apPassword));
    Serial.println("3. Browser: http://192.168.100.1");
    Serial.println("4. Atur WiFi & pilih kota");
  }

  if (prayerConfig.selectedCity.length() == 0) {
    Serial.println("\nREMINDER: Select city via web interface");
  }

  Serial.println("\nBoot selesai - Siap menerima koneksi");
  Serial.println("Log pemantauan akan muncul di bawah:");
  Serial.println("  - Laporan penggunaan stack setiap 60 detik");
  Serial.println("  - Pemeriksaan kesehatan Tugas Shalat setiap 30 detik");
  Serial.println("  - Log pemrosesan Tugas HTTP");
  Serial.println("  - Laporan memori setiap 30 detik (dari webTask)");
  Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}