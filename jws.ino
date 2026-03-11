/*
 * ESP32-2432S024 + LVGL 9.2.0 + EEZ Studio - Islamic Prayer Clock
 * ARCHITECTURE: FreeRTOS Multi-Task Design - FULLY AUTOMATED
 * OPTIMIZED VERSION - Event-Driven + Built-in NTP
 */

#define PRODUCTION 1  // 1 = NONAKTIFKAN SERIAL PRINT, 0 = AKTIFKAN SERIAL PRINT

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

#define DFPLAYER_TX 25  // ESP32 TX → DFPLAYER RX
#define DFPLAYER_RX 32  // ESP32 RX → DFPLAYER TX

#define BUZZER_PIN 26
#define BUZZER_CHANNEL 1
#define BUZZER_FREQ 2000
#define BUZZER_RESOLUTION 8

// PIN RGB LED (COMMON ANODE / AKTIF RENDAH)
#define RGB_R_PIN      4
#define RGB_G_PIN      16
#define RGB_B_PIN      17

#define TFT_BL_CHANNEL 0
#define TFT_BL_FREQ 5000
#define TFT_BL_RESOLUTION 8
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)

#define TS_MIN_X 370
#define TS_MAX_X 3700
#define TS_MIN_Y 470
#define TS_MAX_Y 3600

// ================================
// RTOS CONFIGURATION
// ================================
#define UI_TASK_STACK_SIZE 12288       // LVGL + EEZ RENDERING
#define WIFI_TASK_STACK_SIZE 3072      // EVENT-DRIVEN + RECONNECT
#define NTP_TASK_STACK_SIZE 4096       // HANYA SINKRONISASI NTP
#define WEB_TASK_STACK_SIZE 4096       // ASYNC WEB SERVER
#define PRAYER_TASK_STACK_SIZE 4096    // HTTP + PARSING JSON
#define RTC_TASK_STACK_SIZE 2048       // KOMUNIKASI I2C
#define CLOCK_TASK_STACK_SIZE 2048     // INCREMENT WAKTU
#define AUDIO_TASK_STACK_SIZE 4096     // AUDIO ADZAN

#define UI_TASK_PRIORITY 3             // TERTINGGI - RESPONSIVITAS LAYAR
#define WIFI_TASK_PRIORITY 2           // TINGGI - STABILITAS JARINGAN
#define NTP_TASK_PRIORITY 2            // TINGGI - SINKRONISASI WAKTU
#define WEB_TASK_PRIORITY 1            // RENDAH - WEB SERVER LATAR
#define PRAYER_TASK_PRIORITY 1         // RENDAH - PEMBARUAN HARIAN
#define RTC_TASK_PRIORITY 1            // RENDAH - SINKRONISASI CADANGAN
#define CLOCK_TASK_PRIORITY 2          // TINGGI - AKURASI WAKTU
#define AUDIO_TASK_PRIORITY 0          // RENDAH - AUDIO ADZAN

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
// GRUP EVENT WIFI - EVENT-DRIVEN
// ================================
EventGroupHandle_t wifiEventGroup;

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
const int NTP_SERVER_COUNT = 3;

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
  char alarmTime[6]; // FORMAT "HH:MM"
  bool alarmEnabled;
};

// ================================
// ALARM STATE
// ================================
struct AlarmState {
  bool isRinging;
  unsigned long lastToggle;
  bool clockVisible;
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
static int lastAlarmMinute = -1;

struct CountdownState {
  bool isActive;
  unsigned long startTime;
  int totalSeconds;
  String message;
  String reason; // "AP_RESTART", "DEVICE_RESTART", "FACTORY_RESET"
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
  "00:00",
  false
};

volatile bool needPrayerUpdate = false;
String pendingPrayerLat = "";
String pendingPrayerLon = "";

unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
unsigned long wifiFailedTime = 0;
int wifiRetryCount = 0;
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
static int lastBlinkMinute = -1;

// ================================
// WIFI RESTART PROTECTION
// ================================
static SemaphoreHandle_t wifiRestartMutex = NULL;
static bool wifiRestartInProgress = false;
static bool apRestartInProgress = false;

static unsigned long lastWiFiRestartRequest = 0;
static unsigned long lastAPRestartRequest = 0;
const unsigned long RESTART_DEBOUNCE_MS = 3000;

// ================================
// RGB LED STATE
// ================================
static bool rgbBootBlinking = true;
static bool internetAvailable = false;
static unsigned long rgbLastToggle = 0;
static bool rgbLedState = false;

// ============================================
// FUNGSI RGB LED (COMMON ANODE / AKTIF RENDAH)
// ============================================
void rgbSetColor(bool r, bool g, bool b) {
  digitalWrite(RGB_R_PIN, r ? LOW : HIGH);
  digitalWrite(RGB_G_PIN, g ? LOW : HIGH);
  digitalWrite(RGB_B_PIN, b ? LOW : HIGH);
}

void rgbOff() {
  digitalWrite(RGB_R_PIN, HIGH);
  digitalWrite(RGB_G_PIN, HIGH);
  digitalWrite(RGB_B_PIN, HIGH);
}

void rgbBootBlinkTask(void *parameter) {
  while (rgbBootBlinking) {
    rgbSetColor(true, false, false);
    vTaskDelay(pdMS_TO_TICKS(300));
    rgbOff();
    vTaskDelay(pdMS_TO_TICKS(300));
  }
  rgbOff();
  vTaskDelete(NULL);
}

void handleRGBLed() {
  unsigned long now = millis();

  if (countdownState.isActive &&
      (countdownState.reason == "device_restart" || countdownState.reason == "factory_reset")) {
    if (now - rgbLastToggle >= 500) {
      rgbLastToggle = now;
      rgbLedState = !rgbLedState;
      rgbSetColor(rgbLedState, false, false);
    }
    return;
  }

  if (wifiConfig.routerSSID.length() > 0) {
    switch (wifiState) {
      case WIFI_CONNECTED:
        if (internetAvailable) {
          rgbSetColor(false, true, false);
        } else {
          if (now - rgbLastToggle >= 500) {
            rgbLastToggle = now;
            rgbLedState = !rgbLedState;
            rgbSetColor(false, rgbLedState, false);
          }
        }
        break;

      case WIFI_CONNECTING:
        if (now - rgbLastToggle >= 300) {
          rgbLastToggle = now;
          rgbLedState = !rgbLedState;
          rgbSetColor(false, rgbLedState, false);
        }
        break;

      case WIFI_FAILED:
        rgbSetColor(true, false, false);
        break;

      case WIFI_IDLE:
      default:
        rgbOff();
        break;
    }
    return;
  }

  rgbOff();
}

void rgbBootDone() {
  rgbBootBlinking = false;
  vTaskDelay(pdMS_TO_TICKS(100));
  rgbOff();
  Serial.println("RGB LED: BOOT SELESAI - LED MATI");
}

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
void createDefaultConfigFiles();
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
void internetCheckTask(void *parameter);

bool initDFPlayer();
void setDFPlayerVolume(int vol);
void playDFPlayerAdzan(String prayerName);
bool isDFPlayerPlaying();

// ============================================
// FUNGSI LAYAR DAN ANTARMUKA
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
// COUNTDOWN HELPER FUNCTIONS
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
    Serial.println("HITUNG MUNDUR DIMULAI");
    Serial.println("========================================");
    Serial.println("ALASAN: " + reason);
    Serial.println("PESAN: " + message);
    Serial.println("DURASI: " + String(seconds) + " DETIK");
    Serial.println("========================================\n");
  }
}

void stopCountdown() {
  if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    countdownState.isActive = false;
    countdownState.reason = "";
    xSemaphoreGive(countdownMutex);

    Serial.println("HITUNG MUNDUR DIHENTIKAN");
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
// PRAYER TIME BLINK FUNCTIONS
// ============================================
void checkPrayerTime() {
  if (alarmState.isRinging) return;

  if (prayerConfig.imsakTime   == "00:00" &&
      prayerConfig.subuhTime   == "00:00" &&
      prayerConfig.terbitTime  == "00:00" &&
      prayerConfig.zuhurTime   == "00:00" &&
      prayerConfig.asharTime   == "00:00" &&
      prayerConfig.maghribTime == "00:00" &&
      prayerConfig.isyaTime    == "00:00") {
    return;
  }

  time_t now_t = timeConfig.currentTime;
  struct tm timeinfo;
  localtime_r(&now_t, &timeinfo);

  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  int currentMinuteKey = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  if (timeinfo.tm_sec < 5
      && currentMinuteKey != lastBlinkMinute
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
      lastBlinkMinute = currentMinuteKey;
      startBlinking(prayerName);

      bool isAdzanPrayer = (prayerName != "imsak" && prayerName != "terbit");

      if (isAdzanPrayer && dfPlayerAvailable) {
        adzanState.canTouch = true;
        adzanState.currentPrayer = prayerName;
        adzanState.startTime = now_t;
        adzanState.deadlineTime = now_t + 600;
        saveAdzanState();
        Serial.println("ADZAN AKTIF: " + prayerName + " - SENTUH LAYAR UNTUK PUTAR (10 MENIT)");
      } else {
        adzanState.canTouch = false;
        adzanState.currentPrayer = "";
        Serial.println("NOTIFIKASI AKTIF: " + prayerName + " - BUZZER+KEDIP SAJA (TIDAK PERLU SENTUH)");
      }
    }
  }

  if (adzanState.canTouch && getAdzanRemainingSeconds() <= 0) {
    Serial.println("ADZAN KEDALUWARSA: " + adzanState.currentPrayer);
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
  Serial.println("MULAI BERKEDIP SELAMA 1 MENIT...");
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
      Serial.println("[STOPBLINKING] PERINGATAN: TIMEOUT DISPLAYMUTEX - ELEMEN MUNGKIN MASIH TERSEMBUNYI");
    }

    Serial.println("KEDIP SELESAI - SEMUA WAKTU SHALAT TAMPIL NORMAL");
  }
}

void handleBlinking() {
  if (!blinkState.isBlinking) return;

  unsigned long currentMillis = millis();

  if (currentMillis - blinkState.blinkStartTime >= BLINK_DURATION) {
    stopBlinking();
    return;
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
          ledcWrite(BUZZER_CHANNEL, pwmValue);
        } else {
          lv_obj_add_flag(targetLabel, LV_OBJ_FLAG_HIDDEN);
          ledcWrite(BUZZER_CHANNEL, 0);
        }
      }

      xSemaphoreGive(displayMutex);
    } else {
      blinkState.currentVisible = !blinkState.currentVisible;
    }
  }
}

// ============================================
// PRAYER TIMES API FUNCTIONS
// ============================================
void getPrayerTimesByCoordinates(String lat, String lon) {
  Serial.println("\n[TUGAS SHALAT] MENGIRIM PERMINTAAN HTTP KE ANTRIAN...");

  HTTPRequest request;
  request.latitude = lat;
  request.longitude = lon;

  if (xQueueSend(httpQueue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println("[TUGAS SHALAT] PERMINTAAN HTTP BERHASIL DIANTREKAN");
  } else {
    Serial.println("[TUGAS SHALAT] GAGAL MENGANTREKAN PERMINTAAN HTTP (ANTRIAN PENUH)");
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
      Serial.println("WAKTU SHALAT TERSIMPAN");

      vTaskDelay(pdMS_TO_TICKS(100));
      if (LittleFS.exists("/prayer_times.txt")) {
        Serial.println("FILE WAKTU SHALAT TERVERIFIKASI");
      }
    } else {
      Serial.println("GAGAL MEMBUKA PRAYER_TIMES.TXT UNTUK DITULIS");
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
        Serial.println("WAKTU SHALAT DIMUAT");
        Serial.println("KOTA: " + prayerConfig.selectedCity);
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// WIFI FUNCTIONS
// ============================================
void saveWiFiCredentials() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/wifi_creds.txt", "w");
    if (file) {
      file.println(wifiConfig.routerSSID);
      file.println(wifiConfig.routerPassword);
      file.flush();
      file.close();
      Serial.println("KREDENSIAL WIFI TERSIMPAN");

      vTaskDelay(pdMS_TO_TICKS(100));
      if (LittleFS.exists("/wifi_creds.txt")) {
        Serial.println("FILE WIFI TERVERIFIKASI");
      }
    } else {
      Serial.println("GAGAL MEMBUKA WIFI_CREDS.TXT UNTUK DITULIS");
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
        Serial.println("KREDENSIAL WIFI DIMUAT");
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

        Serial.println("KONFIGURASI AP DIMUAT:");
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
      Serial.println("KREDENSIAL AP TERSIMPAN");
      Serial.println("  SSID: " + String(wifiConfig.apSSID));
      Serial.println("  IP: " + wifiConfig.apIP.toString());
      Serial.println("  GATEWAY: " + wifiConfig.apGateway.toString());
      Serial.println("  SUBNET: " + wifiConfig.apSubnet.toString());

      vTaskDelay(pdMS_TO_TICKS(100));
      if (LittleFS.exists("/ap_creds.txt")) {
        Serial.println("FILE AP TERVERIFIKASI");
      }
    } else {
      Serial.println("GAGAL MEMBUKA AP_CREDS.TXT UNTUK DITULIS");
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

        Serial.print(String("[WIFI-EVENT] ") + String(event));

        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.print("STA TERHUBUNG KE AP");
                xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
                break;

            case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
                Serial.println("\n========================================");
                Serial.println("WIFI BERHASIL TERHUBUNG");
                Serial.println("========================================");
                Serial.println("IP: " + WiFi.localIP().toString());
                Serial.println("GATEWAY: " + WiFi.gatewayIP().toString());
                Serial.println("RSSI: " + String(WiFi.RSSI()) + " DBM");

                int rssi = WiFi.RSSI();
                String quality = rssi >= -50 ? "Sangat Baik" :
                                rssi >= -60 ? "Baik" :
                                rssi >= -70 ? "Cukup" : "Lemah";
                Serial.println("SINYAL: " + quality);
                Serial.println("========================================\n");

                xEventGroupSetBits(wifiEventGroup, WIFI_GOT_IP_BIT);

                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    wifiConfig.isConnected = true;
                    wifiConfig.localIP = WiFi.localIP();
                    wifiState = WIFI_CONNECTED;
                    reconnectAttempts = 0;
                    wifiRetryCount = 0;
                    xSemaphoreGive(wifiMutex);
                }

                if (ntpTaskHandle != NULL) {
                    Serial.print("MEMICU SINKRONISASI NTP...");
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;
                    xTaskNotifyGive(ntpTaskHandle);
                }
                break;
            }

            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
                Serial.println("\n========================================");
                Serial.println("WIFI TERPUTUS");
                Serial.println("========================================");
                Serial.printf("KODE ALASAN: %d", info.wifi_sta_disconnected.reason);

                xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
                xEventGroupSetBits(wifiEventGroup, WIFI_DISCONNECTED_BIT);

                switch (info.wifi_sta_disconnected.reason) {
                    case WIFI_REASON_AUTH_EXPIRE:
                        Serial.println("DETAIL: AUTENTIKASI KEDALUWARSA");
                        break;
                    case WIFI_REASON_AUTH_LEAVE:
                        Serial.println("DETAIL: TERPUTUS DARI AUTENTIKASI");
                        break;
                    case WIFI_REASON_ASSOC_LEAVE:
                        Serial.println("DETAIL: KONEKSI DIPUTUS");
                        break;
                    case WIFI_REASON_BEACON_TIMEOUT:
                        Serial.println("DETAIL: TIMEOUT BEACON (ROUTER TIDAK TERJANGKAU)");
                        break;
                    case WIFI_REASON_NO_AP_FOUND:
                        Serial.println("DETAIL: AP TIDAK DITEMUKAN");
                        break;
                    case WIFI_REASON_HANDSHAKE_TIMEOUT:
                        Serial.println("DETAIL: TIMEOUT HANDSHAKE");
                        break;
                    default:
                        Serial.printf("DETAIL: ALASAN TIDAK DIKETAHUI (%d)",
                                     info.wifi_sta_disconnected.reason);
                }

                wifiDisconnectedTime = millis();

                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    wifiConfig.isConnected = false;
                    wifiState = WIFI_IDLE;
                    xSemaphoreGive(wifiMutex);
                }

                Serial.println("AKAN MENCOBA MENGHUBUNGKAN KEMBALI...");
                Serial.println("========================================\n");
                break;
            }

            case ARDUINO_EVENT_WIFI_AP_START:
                Serial.print("AP DIMULAI: " + String(WiFi.softAPSSID()));
                Serial.print("   IP AP: " + WiFi.softAPIP().toString());
                break;

            case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                Serial.print("KLIEN TERHUBUNG KE AP");
                Serial.print(String("   TOTAL STASIUN: ") + String(WiFi.softAPgetStationNum()));
                break;

            case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                Serial.print("KLIEN TERPUTUS DARI AP");
                Serial.print(String("   TOTAL STASIUN: ") + String(WiFi.softAPgetStationNum()));
                break;
        }
    });

    Serial.print("WIFI EVENT HANDLER TERDAFTAR (DENGAN PROTEKSI RESTART)");
}

// ============================================
// SETTINGS & CONFIGURATION FUNCTIONS
// ============================================
void saveTimezoneConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/timezone.txt", "w");
    if (file) {
      file.println(timezoneOffset);
      file.flush();
      file.close();
      Serial.println("TIMEZONE TERSIMPAN: UTC" + String(timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset));
    } else {
      Serial.println("GAGAL MENYIMPAN TIMEZONE");
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
          Serial.println("OFFSET TIMEZONE TIDAK VALID DI FILE, MENGGUNAKAN DEFAULT +7");
          timezoneOffset = 7;
        }

        file.close();
        Serial.println("TIMEZONE DIMUAT: UTC" + String(timezoneOffset >= 0 ? "+" : "") + String(timezoneOffset));
      }
    } else {
      timezoneOffset = 7;
      Serial.println("KONFIGURASI TIMEZONE TIDAK DITEMUKAN - MENGGUNAKAN DEFAULT (UTC+7)");
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
        Serial.println("KONFIGURASI BUZZER DIMUAT");
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// ALARM CONFIG - SAVE / LOAD
// ============================================
void saveAlarmConfig() {
  if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
    fs::File file = LittleFS.open("/alarm_config.txt", "w");
    if (file) {
      file.println(alarmConfig.alarmTime);
      file.println(alarmConfig.alarmEnabled ? "1" : "0");
      file.flush();
      file.close();
      Serial.println("KONFIGURASI ALARM TERSIMPAN: " + String(alarmConfig.alarmTime) +
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
        Serial.println("KONFIGURASI ALARM DIMUAT: " + String(alarmConfig.alarmTime) +
                       " | " + (alarmConfig.alarmEnabled ? "ON" : "OFF"));
      }
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// ALARM - BERHENTI (DIPANGGIL SAAT LAYAR DISENTUH)
// ============================================
void stopAlarm() {
  if (!alarmState.isRinging) return;

  Serial.println("\n========================================");
  Serial.println("ALARM DIHENTIKAN (LAYAR DISENTUH)");
  Serial.println("========================================");

  alarmState.isRinging = false;
  ledcWrite(BUZZER_CHANNEL, 0);

  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    if (objects.time_now) lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
    xSemaphoreGive(displayMutex);
  }
  alarmState.clockVisible = true;

  Serial.println("NOTIFIKASI SHALAT DIKEMBALIKAN");
  Serial.println("========================================\n");
}

// ============================================
// ALARM - CEK APAKAH WAKTUNYA BERBUNYI
// ============================================
void checkAlarmTime() {
  if (!alarmConfig.alarmEnabled) return;
  if (alarmState.isRinging) return;

  time_t now_t = timeConfig.currentTime;
  struct tm timeinfo;
  localtime_r(&now_t, &timeinfo);

  int currentMinuteKey = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  if (currentMinuteKey == lastAlarmMinute) return;
  if (timeinfo.tm_sec >= 5) return;

  char currentTime[6];
  sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  if (strcmp(currentTime, alarmConfig.alarmTime) == 0) {
    lastAlarmMinute = currentMinuteKey;

    Serial.println("\n========================================");
    Serial.println("ALARM AKTIF: " + String(alarmConfig.alarmTime));
    Serial.println("========================================");
    Serial.println("KEDIP JAM + BUZZER DIMULAI");
    Serial.println("NOTIFIKASI SHALAT DITANGGUHKAN SAMPAI ALARM MATI");
    Serial.println("========================================\n");

    alarmState.savedBlinkState  = blinkState.isBlinking;
    alarmState.savedAdzanCanTouch = adzanState.canTouch;

    if (blinkState.isBlinking) {
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
    if (adzanState.canTouch) {
      adzanState.canTouch = false;
    }

    alarmState.isRinging = true;
    alarmState.lastToggle = millis();
    alarmState.clockVisible = true;
  }
}

// ============================================
// ALARM - TANGANI KEDIP JAM DAN BUZZER
// ============================================
void handleAlarmBlink() {
  if (!alarmState.isRinging) return;

  unsigned long currentMillis = millis();
  if (currentMillis - alarmState.lastToggle >= 500) {
    alarmState.lastToggle = currentMillis;
    alarmState.clockVisible = !alarmState.clockVisible;

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
      if (objects.time_now) {
        if (alarmState.clockVisible) {
          lv_obj_clear_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
          int pwmValue = map(buzzerConfig.volume, 0, 100, 0, 255);
          ledcWrite(BUZZER_CHANNEL, pwmValue);
        } else {
          lv_obj_add_flag(objects.time_now, LV_OBJ_FLAG_HIDDEN);
          ledcWrite(BUZZER_CHANNEL, 0);
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
      Serial.println("KONFIGURASI BUZZER TERSIMPAN");
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
      Serial.println("ADZAN DIPULIHKAN: " + adzanState.currentPrayer);
      Serial.printf("SISA: %d DETIK (%d MENIT)\n", remaining, remaining/60);
    } else {
      adzanState.canTouch = false;
      adzanState.currentPrayer = "";
      Serial.println("ADZAN KEDALUWARSA");
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
        Serial.println("PEMILIHAN KOTA DIMUAT: " + prayerConfig.selectedCity);
        Serial.println("LAT: " + prayerConfig.latitude + ", LON: " + prayerConfig.longitude);
        Serial.printf("TUNE: IMSAK=%d, SUBUH=%d, TERBIT=%d, ZUHUR=%d, ASHAR=%d, MAGHRIB=%d, ISYA=%d\n",
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

      Serial.println("PEMILIHAN KOTA TIDAK DITEMUKAN");
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
      Serial.println("PEMILIHAN METODE TERSIMPAN:");
      Serial.println("ID: " + String(methodConfig.methodId));
      Serial.println("NAMA: " + methodConfig.methodName);
    } else {
      Serial.println("GAGAL MENYIMPAN PEMILIHAN METODE");
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
        Serial.println("PEMILIHAN METODE DIMUAT:");
        Serial.println("ID: " + String(methodConfig.methodId));
        Serial.println("NAMA: " + methodConfig.methodName);
      }
    } else {
      methodConfig.methodId = 5;
      methodConfig.methodName = "Egyptian General Authority of Survey";
      Serial.println("PEMILIHAN METODE TIDAK DITEMUKAN - MENGGUNAKAN DEFAULT (MESIR)");
    }
    xSemaphoreGive(settingsMutex);
  }
}

// ============================================
// RTC FUNCTIONS
// ============================================
bool initRTC() {
    Serial.println("\n========================================");
    Serial.println("INISIALISASI DS3231 RTC");
    Serial.println("========================================");

    if (!rtc.begin()) {
        Serial.println("DS3231 TIDAK DITEMUKAN!");
        Serial.println("KONEKSI KABEL:");
        Serial.println("  SDA -> GPIO21");
        Serial.println("  SCL -> GPIO22");
        Serial.println("  VCC -> 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("\nBERJALAN TANPA RTC");
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

    Serial.println("DS3231 TERDETEKSI DI I2C");

    DateTime test = rtc.now();
    Serial.printf("HASIL UJI: %02d:%02d:%02d %02d/%02d/%04d",
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
        Serial.println("\n*** KERUSAKAN HARDWARE RTC ***");
        Serial.println("CHIP DS3231 RUSAK!");
        Serial.println("REGISTER WAKTU MENGEMBALIKAN DATA RUSAK");
        Serial.println("SENSOR SUHU BERFUNGSI: " + String(rtc.getTemperature()) + " °C");
        Serial.println("\nKEMUNGKINAN PENYEBAB:");
        Serial.println("  1. CHIP DS3231 PALSU/TIRUAN");
        Serial.println("  2. KERUSAKAN OSILATOR KRISTAL");
        Serial.println("  3. KERUSAKAN SRAM INTERNAL");
        Serial.println("\n>>> SOLUSI: BELI MODUL DS3231 BARU <<<");
        Serial.println("\nSISTEM AKAN BERJALAN TANPA RTC");
        Serial.println("WAKTU AKAN DIRESET KE 01/01/2000 SETIAP RESTART");
        Serial.println("SINKRONISASI NTP AKAN MEMPERBAIKI WAKTU SAAT WIFI TERHUBUNG");
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

    Serial.println("UJI HARDWARE RTC BERHASIL");
    Serial.println("RTC BERFUNGSI DENGAN BENAR");

    if (rtc.lostPower()) {
        Serial.println("\nPERINGATAN: RTC KEHILANGAN DAYA!");
        Serial.println("BATERAI MUNGKIN HABIS ATAU TERPUTUS");
        Serial.println("WAKTU AKAN DIATUR DARI SINKRONISASI NTP");

        rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
    } else {
        Serial.println("\nBATERAI CADANGAN RTC BAIK");
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
            Serial.println("CEK RTC: BATERAI HABIS/KEHILANGAN DAYA");
            return false;
        }

        DateTime rtcNow = rtc.now();
        xSemaphoreGive(i2cMutex);

        if (!isRTCTimeValid(rtcNow)) {
            Serial.println("CEK RTC: WAKTU TIDAK VALID");
            Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d",
                         rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                         rtcNow.day(), rtcNow.month(), rtcNow.year());
            return false;
        }

        return true;
    }

    Serial.println("CEK RTC: TIDAK DAPAT MENDAPATKAN MUTEX I2C");
    return false;
}

void saveTimeToRTC() {
    if (!rtcAvailable) {
        Serial.println("[SIMPAN RTC] DILEWATI - RTC TIDAK TERSEDIA");
        return;
    }

    time_t currentTime;
    if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentTime = timeConfig.currentTime;
        xSemaphoreGive(timeMutex);
    } else {
        Serial.println("[SIMPAN RTC] GAGAL - TIDAK DAPAT MENDAPATKAN TIMEMUTEX");
        return;
    }

    if (currentTime < 946684800) {
        Serial.println("[SIMPAN RTC] DILEWATI - TIMESTAMP TIDAK VALID (SEBELUM 2000)");
        Serial.printf("   TIMESTAMP: %ld\n", currentTime);
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
    Serial.printf("WAKTU YANG DISIMPAN: %02d:%02d:%02d %02d/%02d/%04d",
                 h, min, sec, d, m, y);

    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31 ||
        h > 23 || min > 59 || sec > 59) {
        Serial.println("ERROR: KOMPONEN WAKTU TIDAK VALID!");
        Serial.println("   TIDAK DAPAT MENYIMPAN KE RTC");
        Serial.println("========================================\n");
        return;
    }

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        DateTime dt(y, m, d, h, min, sec);

        rtc.adjust(dt);

        delay(100);

        xSemaphoreGive(i2cMutex);

        Serial.println("DATA DITULIS KE RTC");
    } else {
        Serial.println("ERROR: TIDAK DAPAT MENDAPATKAN MUTEX I2C");
        Serial.println("========================================\n");
        return;
    }

    delay(500);

    Serial.println("\nMEMVERIFIKASI PENULISAN RTC...");

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        DateTime verify = rtc.now();
        xSemaphoreGive(i2cMutex);

        Serial.printf("RTC SEKARANG MENUNJUKKAN: %02d:%02d:%02d %02d/%02d/%04d",
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
            Serial.println("KOMPONEN WAKTU COCOK");
            Serial.println("WAKTU AKAN BERTAHAN SAAT RESTART");
            Serial.println("========================================\n");
        } else {
            Serial.println("\nSIMPAN RTC GAGAL");
            Serial.println("VERIFIKASI TIDAK COCOK:");
            if (!yearOk) Serial.printf("   TAHUN: DITULIS %d, DIBACA %d", y, verify.year());
            if (!monthOk) Serial.printf("   BULAN: DITULIS %d, DIBACA %d", m, verify.month());
            if (!dayOk) Serial.printf("   HARI: DITULIS %d, DIBACA %d", d, verify.day());
            if (!hourOk) Serial.printf("   JAM: DITULIS %d, DIBACA %d", h, verify.hour());
            if (!minOk) Serial.printf("   MENIT: DITULIS %d, DIBACA %d", min, verify.minute());

            Serial.println("\nKEMUNGKINAN PENYEBAB:");
            Serial.println("  1. BATERAI RTC LEMAH/HABIS");
            Serial.println("  2. HARDWARE RTC RUSAK");
            Serial.println("  3. MASALAH KOMUNIKASI I2C");
            Serial.println("  4. CHIP DS3231 PALSU");
            Serial.println("\nREKOMENDASI: GANTI MODUL RTC");
            Serial.println("========================================\n");

            static int failCount = 0;
            failCount++;
            if (failCount >= 3) {
                Serial.println("KRITIS: RTC GAGAL 3 KALI - DINONAKTIFKAN");
                rtcAvailable = false;
                failCount = 0;
            }
        }
    } else {
        Serial.println("ERROR: TIDAK DAPAT MENDAPATKAN MUTEX I2C UNTUK VERIFIKASI");
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
// WEB SERVER FUNCTIONS
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
    int rssi = isWiFiConnected ? WiFi.RSSI() : 0;

    String wifiStateStr;
    switch (wifiState) {
      case WIFI_IDLE:       wifiStateStr = "idle"; break;
      case WIFI_CONNECTING: wifiStateStr = "connecting"; break;
      case WIFI_CONNECTED:  wifiStateStr = "connected"; break;
      case WIFI_FAILED:     wifiStateStr = "failed"; break;
      default:              wifiStateStr = "unknown"; break;
    }

    char jsonBuffer[768];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{"
      "\"connected\":%s,"
      "\"wifiState\":\"%s\","
      "\"ssid\":\"%s\","
      "\"ip\":\"%s\","
      "\"rssi\":%d,"
      "\"ntpSynced\":%s,"
      "\"ntpServer\":\"%s\","
      "\"currentTime\":\"%s\","
      "\"currentDate\":\"%s\","
      "\"uptime\":%lu,"
      "\"freeHeap\":\"%d\""
      "}",
      isWiFiConnected ? "true" : "false",
      wifiStateStr.c_str(),
      ssid.c_str(),
      ip.c_str(),
      rssi,
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
      Serial.println("PERMINTAAN RESTART WIFI DITOLAK");
      Serial.println("========================================");
      Serial.println("ALASAN: TERLALU CEPAT (DEBOUNCING AKTIF)");
      Serial.printf("WAKTU TUNGGU: %lu MS", waitTime);
      Serial.println("========================================\n");

      request -> send(429, "text/plain", msg);
      return;
    }

    if (request -> hasParam("ssid", true) && request -> hasParam("password", true)) {
      String newSSID = request -> getParam("ssid", true) -> value();
      String newPassword = request -> getParam("password", true) -> value();

      Serial.println("\n========================================");
      Serial.println("SIMPAN KREDENSIAL WIFI");
      Serial.println("========================================");
      Serial.println("SSID BARU: " + newSSID);

      if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        wifiConfig.routerSSID = newSSID;
        wifiConfig.routerPassword = newPassword;
        xSemaphoreGive(wifiMutex);
      }

      saveWiFiCredentials();

      Serial.println("KREDENSIAL TERSIMPAN KE LITTLEFS");
      Serial.println("WIFI AKAN MENGHUBUNGKAN ULANG DALAM 3 DETIK...");
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
          request->send(429, "text/plain", "Please wait " + String(waitTime / 1000) + " DETIK");
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
          Serial.println("MODE: KONFIGURASI JARINGAN SAJA");

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
                      Serial.println("IP AP BARU: " + newAPIP.toString());
                  } else {
                      Serial.println("FORMAT IP TIDAK VALID, MEMPERTAHANKAN: " + newAPIP.toString());
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
                      Serial.println("GATEWAY BARU: " + newGateway.toString());
                  } else {
                      Serial.println("GATEWAY TIDAK VALID, MEMPERTAHANKAN: " + newGateway.toString());
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
                      Serial.println("SUBNET BARU: " + newSubnet.toString());
                  } else {
                      Serial.println("SUBNET TIDAK VALID, MEMPERTAHANKAN: " + newSubnet.toString());
                  }
              }
          }

          wifiConfig.apIP = newAPIP;
          wifiConfig.apGateway = newGateway;
          wifiConfig.apSubnet = newSubnet;

          saveAPCredentials();

          Serial.println("\nKONFIGURASI TERSIMPAN:");
          Serial.println("  IP: " + newAPIP.toString() + " (DIPERBARUI)");
          Serial.println("  GATEWAY: " + newGateway.toString() + " (DIPERBARUI)");
          Serial.println("  SUBNET: " + newSubnet.toString() + " (DIPERBARUI)");

      } else {
          Serial.println("MODE: SSID/PASSWORD SAJA");
          Serial.println("KONFIGURASI JARINGAN TIDAK AKAN BERUBAH");

          if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
              Serial.println("ERROR: SSID ATAU PASSWORD TIDAK ADA");
              request->send(400, "text/plain", "Missing ssid or password");
              return;
          }

          String newSSID = request->getParam("ssid", true)->value();
          String newPass = request->getParam("password", true)->value();

          newSSID.trim();
          newPass.trim();

          if (newSSID.length() == 0) {
              Serial.println("ERROR: SSID TIDAK BOLEH KOSONG");
              request->send(400, "text/plain", "SSID cannot be empty");
              return;
          }

          if (newPass.length() > 0 && newPass.length() < 8) {
              Serial.println("ERROR: PASSWORD MINIMAL 8 KARAKTER");
              request->send(400, "text/plain", "Password minimal 8 karakter");
              return;
          }

          newSSID.toCharArray(wifiConfig.apSSID, 33);
          newPass.toCharArray(wifiConfig.apPassword, 65);

          saveAPCredentials();

          Serial.println("\nKONFIGURASI TERSIMPAN:");
          Serial.println("  SSID: " + String(wifiConfig.apSSID) + " (DIPERBARUI)");
          Serial.println("  PASSWORD: ******** (DIPERBARUI)");
          Serial.println("  IP: " + wifiConfig.apIP.toString() + " (TIDAK BERUBAH)");
          Serial.println("  GATEWAY: " + wifiConfig.apGateway.toString() + " (TIDAK BERUBAH)");
          Serial.println("  SUBNET: " + wifiConfig.apSubnet.toString() + " (TIDAK BERUBAH)");
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

      Serial.println("INFO KLIEN:");
      Serial.println("  IP: " + clientIP.toString());
      Serial.println("  AKSES: " + String(isLocalAP ? "AP LOKAL" : "WIFI JARAK JAUH"));

      if (isLocalAP) {
          startCountdown("ap_restart", "Memulai Ulang Access Point", 60);
          Serial.println("HITUNG MUNDUR DIMULAI (KLIEN DI AP LOKAL)");
      } else {
          Serial.println("KLIEN DI JARINGAN JARAK JAUH - TIDAK PERLU HITUNG MUNDUR");
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
      Serial.printf("DITERIMA: %02d:%02d:%02d %02d/%02d/%04d\n", h, i, s, d, m, y);

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
        Serial.println("\nMENYIMPAN WAKTU KE HARDWARE RTC...");

        saveTimeToRTC();

        delay(500);

        DateTime rtcNow = rtc.now();
        Serial.println("VERIFIKASI RTC:");
        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d",
          rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
          rtcNow.day(), rtcNow.month(), rtcNow.year());

        bool rtcValid = (
          rtcNow.year() >= 2000 && rtcNow.year() <= 2100 &&
          rtcNow.month() >= 1 && rtcNow.month() <= 12 &&
          rtcNow.day() >= 1 && rtcNow.day() <= 31
        );

        if (rtcValid) {
          Serial.println("RTC BERHASIL DISIMPAN");
          Serial.println("WAKTU AKAN BERTAHAN SAAT RESTART");
        } else {
          Serial.println("SIMPAN RTC GAGAL - WAKTU TIDAK VALID");
          Serial.println("PERIKSA BATERAI RTC ATAU KONEKSI I2C");
        }
      } else {
        Serial.println("\nRTC TIDAK TERSEDIA - WAKTU AKAN DIRESET SAAT RESTART");
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
      Serial.println("SIMPAN TIMEZONE");
      Serial.println("========================================");

      if (!request -> hasParam("offset", true)) {
        Serial.println("ERROR: PARAMETER OFFSET TIDAK ADA");
        request -> send(400, "application/json",
          "{\"error\":\"Missing offset parameter\"}");
        return;
      }

      String offsetStr = request -> getParam("offset", true) -> value();
      offsetStr.trim();

      int offset = offsetStr.toInt();

      Serial.println("OFFSET DITERIMA: " + String(offset));

      if (offset < -12 || offset > 14) {
        Serial.println("ERROR: OFFSET TIMEZONE TIDAK VALID");
        request -> send(400, "application/json",
          "{\"error\":\"Invalid timezone offset (must be -12 to +14)\"}");
        return;
      }

      Serial.println("MENYIMPAN KE MEMORI DAN FILE...");

      if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        timezoneOffset = offset;
        xSemaphoreGive(settingsMutex);
        Serial.println("MEMORI DIPERBARUI");
      }

      bool ntpTriggered = false;
      bool prayerWillUpdate = false;

      if (wifiConfig.isConnected && ntpTaskHandle != NULL) {
        Serial.println("\n========================================");
        Serial.println("MEMICU ULANG SINKRONISASI NTP OTOMATIS");
        Serial.println("========================================");
        Serial.println("ALASAN: TIMEZONE DIUBAH KE UTC" + String(offset >= 0 ? "+" : "") + String(offset));

        if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
          Serial.println("KOTA: " + prayerConfig.selectedCity);
          Serial.println("KOORDINAT: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
          Serial.println("");
          Serial.println("TUGAS NTP AKAN OTOMATIS:");
          Serial.println("  1. SINKRONISASI WAKTU DENGAN TIMEZONE BARU");
          Serial.println("  2. PERBARUI WAKTU SHALAT DENGAN TANGGAL YANG BENAR");
          prayerWillUpdate = true;
        } else {
          Serial.println("CATATAN: TIDAK ADA KOORDINAT KOTA");
          Serial.println("HANYA WAKTU YANG AKAN DISINKRONKAN (TANPA PEMBARUAN WAKTU SHALAT)");
        }

        if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          timeConfig.ntpSynced = false;
          xSemaphoreGive(timeMutex);
        }

        xTaskNotifyGive(ntpTaskHandle);
        ntpTriggered = true;

        Serial.println("SINKRONISASI ULANG NTP BERHASIL DIPICU");
        Serial.println("========================================\n");

      } else {
        Serial.println("\nTIDAK DAPAT MEMICU SINKRONISASI NTP:");
        if (!wifiConfig.isConnected) {
          Serial.println("ALASAN: WIFI TIDAK TERHUBUNG");
        }
        if (ntpTaskHandle == NULL) {
          Serial.println("ALASAN: TUGAS NTP TIDAK BERJALAN");
        }
        Serial.println("TIMEZONE AKAN DITERAPKAN SAAT KONEKSI BERIKUTNYA\n");
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
      Serial.println("BERHASIL: TIMEZONE TERSIMPAN");
      Serial.println("OFFSET: UTC" + String(offset >= 0 ? "+" : "") + String(offset));
      Serial.println("========================================\n");
  });

  server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!LittleFS.exists("/cities.json")) {
      Serial.println("CITIES.JSON TIDAK DITEMUKAN");
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

          Serial.println("METADATA KOTA TERSIMPAN:");
          Serial.println("UKURAN JSON: " + jsonSizeStr + " BYTE");
          Serial.println("JUMLAH KOTA: " + citiesCountStr);
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
        Serial.println("UPLOAD CITIES.JSON DIMULAI");
        Serial.println("========================================");
        Serial.printf("NAMA FILE: %s", filename.c_str());

        if (filename != "cities.json") {
          Serial.printf("NAMA FILE TIDAK VALID: %s (HARUS CITIES.JSON)", filename.c_str());
          return;
        }

        if (LittleFS.exists("/cities.json")) {
          LittleFS.remove("/cities.json");
          Serial.println("CITIES.JSON LAMA DIHAPUS");
        }

        uploadFile = LittleFS.open("/cities.json", "w");
        if (!uploadFile) {
          Serial.println("GAGAL MEMBUKA FILE UNTUK DITULIS");
          return;
        }

        totalSize = 0;
        uploadStartTime = millis();
        Serial.println("MENULIS KE LITTLEFS...");
      }

      if (uploadFile) {
        size_t written = uploadFile.write(data, len);
        if (written != len) {
          Serial.printf("KETIDAKSESUAIAN PENULISAN: %d/%d BYTE", written, len);
        }
        totalSize += written;

        if (totalSize % 5120 == 0 || final) {
          Serial.printf("PROGRESS: %d BYTE (%.1F KB)",
            totalSize, totalSize / 1024.0);
        }
      }

      if (final) {
        if (uploadFile) {
          uploadFile.flush();
          uploadFile.close();

          unsigned long uploadDuration = millis() - uploadStartTime;

          Serial.println("\nUPLOAD SELESAI");
          Serial.printf("UKURAN TOTAL: %d BYTE (%.2F KB)",
            totalSize, totalSize / 1024.0);
          Serial.printf("DURASI: %lu MS", uploadDuration);

          vTaskDelay(pdMS_TO_TICKS(100));

          if (LittleFS.exists("/cities.json")) {
            fs::File verifyFile = LittleFS.open("/cities.json", "r");
            if (verifyFile) {
              size_t fileSize = verifyFile.size();

              char buffer[101];
              size_t bytesRead = verifyFile.readBytes(buffer, 100);
              buffer[bytesRead] = '\0';

              verifyFile.close();

              Serial.printf("FILE TERVERIFIKASI: %d BYTE", fileSize);
              Serial.println("100 KARAKTER PERTAMA:");
              Serial.println(buffer);

              String preview(buffer);
              if (preview.indexOf('[') >= 0 && preview.indexOf('{') >= 0) {
                Serial.println("FORMAT JSON TERLIHAT VALID");
              } else {
                Serial.println("PERINGATAN: FILE MUNGKIN BUKAN JSON YANG VALID");
              }

              Serial.println("========================================\n");
            }
          } else {
            Serial.println("VERIFIKASI FILE GAGAL - FILE TIDAK DITEMUKAN");
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
      Serial.println("SIMPAN METODE PERHITUNGAN");
      Serial.print("IP KLIEN: ");
      Serial.println(request -> client() -> remoteIP().toString());
      Serial.println("========================================");

      if (!request -> hasParam("methodId", true) || !request -> hasParam("methodName", true)) {
        Serial.println("ERROR: PARAMETER TIDAK ADA");

        int params = request -> params();
        Serial.printf("PARAMETER DITERIMA (%d):", params);
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

      Serial.println("DATA DITERIMA:");
      Serial.println("ID METODE: " + String(methodId));
      Serial.println("NAMA METODE: " + methodName);

      if (methodId < 0 || methodId > 20) {
        Serial.println("ERROR: ID METODE TIDAK VALID");
        request -> send(400, "application/json",
          "{\"error\":\"Invalid method ID\"}");
        return;
      }

      if (methodName.length() == 0) {
        Serial.println("ERROR: NAMA METODE KOSONG");
        request -> send(400, "application/json",
          "{\"error\":\"Method name cannot be empty\"}");
        return;
      }

      if (methodName.length() > 100) {
        Serial.println("ERROR: NAMA METODE TERLALU PANJANG");
        request -> send(400, "application/json",
          "{\"error\":\"Method name too long (max 100 chars)\"}");
        return;
      }

      Serial.println("MENYIMPAN KE MEMORI...");

      if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
        methodConfig.methodId = methodId;
        methodConfig.methodName = methodName;

        xSemaphoreGive(settingsMutex);
        Serial.println("MEMORI DIPERBARUI");
        Serial.println("ID METODE: " + String(methodConfig.methodId));
        Serial.println("NAMA METODE: " + methodConfig.methodName);
      }

      bool willFetchPrayerTimes = false;

      if (WiFi.status() == WL_CONNECTED) {
          if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
              Serial.println("MEMICU PEMBARUAN WAKTU SHALAT DENGAN METODE BARU...");
              Serial.println("KOTA: " + prayerConfig.selectedCity);
              Serial.println("METODE: " + methodName);

              willFetchPrayerTimes = true;
          } else {
              Serial.println("TIDAK ADA KOORDINAT TERSEDIA");
          }
      } else {
          Serial.println("WIFI TIDAK TERHUBUNG");
      }

      String response = "{";
      response += "\"success\":true,";
      response += "\"methodId\":" + String(methodId) + ",";
      response += "\"methodName\":\"" + methodName + "\",";
      response += "\"prayerTimesUpdating\":" + String(willFetchPrayerTimes ? "true" : "false");
      response += "}";

      request -> send(200, "application/json", response);

      vTaskDelay(pdMS_TO_TICKS(50));

      Serial.println("MENULIS KE LITTLEFS...");
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
                  Serial.println("TUGAS SHALAT DIPICU UNTUK PERUBAHAN METODE");
              }
          }

          Serial.println("PEMBARUAN WAKTU SHALAT DIMULAI");
      }

      Serial.println("========================================");
      Serial.println("BERHASIL: METODE TERSIMPAN");
      Serial.println("METODE: " + methodName);
      if (willFetchPrayerTimes) {
        Serial.println("WAKTU SHALAT AKAN DIPERBARUI SEBENTAR LAGI...");
      }
      Serial.println("========================================\n");
  });

  // ========================================
  // TAB JADWAL - WAKTU SHALAT DAN BUZZER
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
      lastAlarmMinute = -1;
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
      Serial.println("UJI BUZZER DIMULAI");
      Serial.println("========================================");
      Serial.printf("VOLUME: %d%%", volume);
      Serial.println("DURASI: BERHENTI MANUAL ATAU TIMEOUT 30 DETIK");
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

              Serial.printf("LOOP UJI BUZZER DIMULAI (PWM: %d)", pwmValue);

              while ((millis() - startTime) < maxDuration) {
                  if (buzzerTestTaskHandle == NULL) {
                      Serial.println("UJI BUZZER DIHENTIKAN OLEH PENGGUNA");
                      break;
                  }

                  ledcWrite(BUZZER_CHANNEL, pwmValue);
                  vTaskDelay(pdMS_TO_TICKS(500));

                  ledcWrite(BUZZER_CHANNEL, 0);
                  vTaskDelay(pdMS_TO_TICKS(500));
              }

              ledcWrite(BUZZER_CHANNEL, 0);
              Serial.println("UJI BUZZER SELESAI");

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
          Serial.println("TUGAS UJI BUZZER DIHAPUS");
      }

      request->send(200, "text/plain", "OK");
      Serial.println("BUZZER BERHASIL DIHENTIKAN\n");
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
        Serial.println("WAKTU ALARM DIATUR: " + t);
      }
    }

    if (request->hasParam("alarmEnabled", true)) {
      String en = request->getParam("alarmEnabled", true)->value();
      alarmConfig.alarmEnabled = (en == "true" || en == "1");
      changed = true;
      Serial.println("ALARM DIAKTIFKAN: " + String(alarmConfig.alarmEnabled ? "ON" : "OFF"));
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

      Serial.println("RESET PABRIK DIMULAI");

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
    int apiRssi = isWiFiConnected ? WiFi.RSSI() : 0;

    String apiWifiStateStr;
    switch (wifiState) {
      case WIFI_IDLE:       apiWifiStateStr = "idle"; break;
      case WIFI_CONNECTING: apiWifiStateStr = "connecting"; break;
      case WIFI_CONNECTED:  apiWifiStateStr = "connected"; break;
      case WIFI_FAILED:     apiWifiStateStr = "failed"; break;
      default:              apiWifiStateStr = "unknown"; break;
    }

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
      "\"wifiState\":\"%s\","
      "\"rssi\":%d,"
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
      apiWifiStateStr.c_str(),
      apiRssi,
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

      Serial.printf("[404] KLIEN: %s | URL: %s",
        clientIP.toString().c_str(), url.c_str());

      if (url.startsWith("/css/") || url.endsWith(".css") || url.endsWith(".js") ||
        url.endsWith(".png") || url.endsWith(".jpg") || url.endsWith(".jpeg") ||
        url.endsWith(".gif") || url.endsWith(".ico") || url.endsWith(".svg") ||
        url.endsWith(".woff") || url.endsWith(".woff2") || url.endsWith(".ttf")) {

        request -> send(404, "text/plain", "File not found");
        return;
      }

      Serial.println("URL TIDAK VALID, MENGALIHKAN KE /NOTFOUND");
      request -> redirect("/notfound");
    });
  }

// ============================================
// UTILITY FUNCTIONS
// ============================================
bool init_littlefs() {
  Serial.println("MENGINISIALISASI LITTLEFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("MOUNT LITTLEFS GAGAL");
    return false;
  }
  Serial.println("LITTLEFS TERPASANG");
  return true;
}

// ============================================
// FILE KONFIGURASI DEFAULT - DIBUAT SAAT BOOT JIKA BELUM ADA
// ============================================
void createDefaultConfigFiles() {
  Serial.println("\n========================================");
  Serial.println("CEK FILE KONFIGURASI DEFAULT");
  Serial.println("========================================");

  if (!LittleFS.exists("/ap_creds.txt")) {
    fs::File f = LittleFS.open("/ap_creds.txt", "w");
    if (f) {
      f.println(DEFAULT_AP_SSID);
      f.println(DEFAULT_AP_PASSWORD);
      f.println("192.168.100.1");
      f.println("192.168.100.1");
      f.println("255.255.255.0");
      f.flush();
      f.close();
      Serial.println("[DIBUAT] /AP_CREDS.TXT");
      Serial.println("  SSID    : " + DEFAULT_AP_SSID);
      Serial.println("  PASSWORD: " + String(DEFAULT_AP_PASSWORD));
      Serial.println("  IP      : 192.168.100.1");
    } else {
      Serial.println("[ERROR] GAGAL MEMBUAT /AP_CREDS.TXT");
    }
  } else {
    Serial.println("[ADA]   /AP_CREDS.TXT - DILEWATI");
  }

  if (!LittleFS.exists("/timezone.txt")) {
    fs::File f = LittleFS.open("/timezone.txt", "w");
    if (f) {
      f.println("7");
      f.flush();
      f.close();
      Serial.println("[DIBUAT] /TIMEZONE.TXT");
      Serial.println("  TIMEZONE: UTC+7 (WIB)");
    } else {
      Serial.println("[ERROR] GAGAL MEMBUAT /TIMEZONE.TXT");
    }
  } else {
    Serial.println("[ADA]   /TIMEZONE.TXT - DILEWATI");
  }

  if (!LittleFS.exists("/buzzer_config.txt")) {
    fs::File f = LittleFS.open("/buzzer_config.txt", "w");
    if (f) {
      f.println("0");
      f.println("0");
      f.println("0");
      f.println("0");
      f.println("0");
      f.println("0");
      f.println("0");
      f.println("50");
      f.flush();
      f.close();
      Serial.println("[DIBUAT] /BUZZER_CONFIG.TXT");
      Serial.println("  SEMUA NOTIFIKASI: NONAKTIF | VOLUME: 50");
    } else {
      Serial.println("[ERROR] GAGAL MEMBUAT /BUZZER_CONFIG.TXT");
    }
  } else {
    Serial.println("[ADA]   /BUZZER_CONFIG.TXT - DILEWATI");
  }

  if (!LittleFS.exists("/alarm_config.txt")) {
    fs::File f = LittleFS.open("/alarm_config.txt", "w");
    if (f) {
      f.println("00:00");
      f.println("0");
      f.flush();
      f.close();
      Serial.println("[DIBUAT] /ALARM_CONFIG.TXT");
      Serial.println("  ALARM: 00:00 | STATUS: NONAKTIF");
    } else {
      Serial.println("[ERROR] GAGAL MEMBUAT /ALARM_CONFIG.TXT");
    }
  } else {
    Serial.println("[ADA]   /ALARM_CONFIG.TXT - DILEWATI");
  }

  if (!LittleFS.exists("/method_selection.txt")) {
    fs::File f = LittleFS.open("/method_selection.txt", "w");
    if (f) {
      f.println("5");
      f.println("Egyptian General Authority of Survey");
      f.flush();
      f.close();
      Serial.println("[DIBUAT] /METHOD_SELECTION.TXT");
      Serial.println("  METODE: 5 - LEMBAGA SURVEI UMUM MESIR");
    } else {
      Serial.println("[ERROR] GAGAL MEMBUAT /METHOD_SELECTION.TXT");
    }
  } else {
    Serial.println("[ADA]   /METHOD_SELECTION.TXT - DILEWATI");
  }

  Serial.println("========================================\n");
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

      Serial.printf("%-10s: %5d/%5d (%5.1f%%) [SISA: %5d] ",
                    tasks[i].name, used, tasks[i].size, percent, free);

      if (percent < 40) Serial.println("BOROS - DAPAT DIKURANGI");
      else if (percent < 60) Serial.println("OPTIMAL");
      else if (percent < 75) Serial.println("SESUAI");
      else if (percent < 90) Serial.println("TINGGI - PANTAU TERUS");
      else if (percent < 95) Serial.println("BERBAHAYA - HARUS DITAMBAH");
      else Serial.println("KRITIS - TAMBAH SEGERA");
    } else {
      Serial.printf("%-10s: TUGAS TIDAK BERJALAN", tasks[i].name);
    }
  }

  Serial.println("========================================");
  Serial.printf("TOTAL DIALOKASIKAN: %d BYTE (%.1F KB)",
                totalAllocated, totalAllocated / 1024.0);
  Serial.printf("TOTAL DIGUNAKAN:    %d BYTE (%.1F KB)",
                totalUsed, totalUsed / 1024.0);
  Serial.printf("TOTAL TERSISA:      %d BYTE (%.1F KB)",
                totalFree, totalFree / 1024.0);
  Serial.printf("EFISIENSI:      %.1f%%\n",
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
          Serial.println("TUGAS KRITIS:");
          hasCritical = true;
        }
        Serial.printf("   %s: %.1f%% (TERSISA: %d BYTE)",
                      tasks[i].name, percent, free);
      }
    }
  }

  if (hasCritical) {
    Serial.println("   TINDAKAN: TAMBAH UKURAN STACK SEGERA");
  }

  Serial.println("========================================\n");
}

// ============================================
// FUNGSI CALLBACK LVGL
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
      // PRIORITAS 1: MATIKAN ALARM DENGAN SENTUHAN LAYAR
      // ======================================
      if (alarmState.isRinging) {
        stopAlarm();
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

          Serial.println("SENTUH ADZAN: " + areas[i].name);

          if (audioTaskHandle != NULL) {
            Serial.println("SISTEM AUDIO TERSEDIA - MEMULAI PEMUTARAN");

            adzanState.isPlaying = true;
            adzanState.canTouch = false;

            xTaskNotifyGive(audioTaskHandle);

          } else {
            Serial.println("========================================");
            Serial.println("PERINGATAN: SISTEM AUDIO TIDAK TERSEDIA");
            Serial.println("========================================");
            Serial.println("ALASAN: SD CARD TIDAK TERDETEKSI ATAU AUDIO DINONAKTIFKAN");
            Serial.println("AKSI: MENGHAPUS STATUS ADZAN SEGERA");
            Serial.println("========================================");

            adzanState.isPlaying = false;
            adzanState.canTouch = false;
            adzanState.currentPrayer = "";

            saveAdzanState();

            Serial.println("STATUS ADZAN DIKOSONGKAN - SIAP UNTUK WAKTU SHALAT BERIKUTNYA");
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
// FUNGSI TASK FREERTOS
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
          Serial.println("WAKTU SHALAT AWAL DITAMPILKAN");
        }

        if (!uiShown) {
          showAllUIElements();
          uiShown = true;
          Serial.println("ELEMEN UI DITAMPILKAN");
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
    handleRGBLed();

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void wifiTask(void *parameter) {
    esp_task_wdt_add(NULL);

    Serial.println("\n========================================");
    Serial.println("TUGAS WIFI DIMULAI - MODE EVENT-DRIVEN");
    Serial.println("========================================\n");

    bool autoUpdateDone = false;
    unsigned long lastMonitor = 0;

    while (true) {
        esp_task_wdt_reset();

        // ========================================
        // TUNGGU EVENT WIFI (EVENT-DRIVEN)
        // ========================================
        EventBits_t bits = xEventGroupWaitBits(
            wifiEventGroup,
            WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | WIFI_GOT_IP_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(5000)
        );

        // ========================================
        // EVENT: WIFI TERPUTUS
        // ========================================
        if (bits & WIFI_DISCONNECTED_BIT) {
            xEventGroupClearBits(wifiEventGroup, WIFI_DISCONNECTED_BIT);

            Serial.println("\n========================================");
            Serial.println("EVENT WIFI TERPUTUS");
            Serial.println("========================================");

            // ============================================
            // SAFETY: SKIP EVENTS DURING RESTART
            // ============================================
            if (wifiRestartInProgress || apRestartInProgress) {
                Serial.println("RESTART WIFI/AP SEDANG BERJALAN - MELEWATI AUTO-RECONNECT");
                Serial.println("ALASAN: HINDARI KONFLIK DENGAN OPERASI RESTART MANUAL");
                Serial.println("========================================\n");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            if (autoUpdateDone) {
                Serial.println("\n>>> RESET FLAG: AUTOUPDATEDONE = FALSE <<<");
                Serial.println("ALASAN: WIFI TERPUTUS");
                Serial.println("EFEK: NTP & SHALAT AKAN DIPICU ULANG SAAT KONEKSI BERIKUTNYA");
                autoUpdateDone = false;
            }

            IPAddress apIP = WiFi.softAPIP();
            if (apIP == IPAddress(0, 0, 0, 0)) {
                Serial.println("PERINGATAN: AP MATI SAAT PEMUTUSAN");
                Serial.println("MEMULIHKAN AP...");

                WiFi.softAPdisconnect(false);
                vTaskDelay(pdMS_TO_TICKS(500));

                WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                vTaskDelay(pdMS_TO_TICKS(500));

                Serial.println("AP DIPULIHKAN: " + WiFi.softAPIP().toString());
            } else {
                Serial.println("AP MASIH AKTIF: " + apIP.toString());
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
                reconnectAttempts++;
                Serial.printf("PERCOBAAN KONEKSI ULANG %d/%d KE: %s\n",
                    reconnectAttempts, MAX_RECONNECT_ATTEMPTS,
                    wifiConfig.routerSSID.c_str());

                if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
                    wifiRetryCount = 0;
                    unsigned long firstRetry = 10000;
                    Serial.println("MAKSIMUM PERCOBAAN KONEKSI TERCAPAI → WIFI_GAGAL");
                    Serial.printf("PERCOBAAN PERTAMA DALAM %lu DETIK\n", firstRetry / 1000);
                    wifiState = WIFI_FAILED;
                    wifiFailedTime = millis();
                } else {
                    esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), hostname.c_str());
                    WiFi.begin(wifiConfig.routerSSID.c_str(),
                              wifiConfig.routerPassword.c_str());
                    wifiState = WIFI_CONNECTING;
                }
            }

            Serial.println("AKAN MENCOBA MENGHUBUNGKAN KEMBALI...");
            Serial.println("========================================\n");
        }

        // ========================================
        // EVENT: WIFI MENDAPAT IP (TERHUBUNG)
        // ========================================
        if (bits & WIFI_GOT_IP_BIT) {
            if (!autoUpdateDone && wifiConfig.isConnected) {
                Serial.println("\n========================================");
                Serial.println("URUTAN AUTO-UPDATE DIMULAI");
                Serial.println("========================================");
                Serial.println("PEMICU: WIFI BARU TERHUBUNG");
                Serial.println("AUTOUPDATEDONE: FALSE (SIAP SINKRONISASI)");

                // ============================================
                // WAIT FOR NTP SYNC START
                // ============================================
                if (!ntpSyncInProgress && !ntpSyncCompleted) {
                    Serial.println("MENUNGGU TUGAS NTP DIMULAI...");
                    vTaskDelay(pdMS_TO_TICKS(1000));

                    if (!ntpSyncInProgress && !ntpSyncCompleted) {
                        Serial.println("PERINGATAN: SINKRONISASI NTP BELUM DIMULAI");
                        Serial.println("MENUNGGU TAMBAHAN 2 DETIK...");
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

                    Serial.println("MENUNGGU SINKRONISASI NTP SELESAI...");

                    while (ntpSyncInProgress && (millis() - waitStartTime < maxWaitTime)) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        ntpWaitCounter++;

                        if (ntpWaitCounter % 10 == 0) {
                            Serial.printf("SINKRONISASI NTP BERJALAN... (%lu MS BERLALU)",
                                        millis() - waitStartTime);
                            esp_task_wdt_reset();
                        }

                        esp_task_wdt_reset();
                    }

                    if (ntpSyncInProgress) {
                        Serial.println("TIMEOUT SINKRONISASI NTP - MELANJUTKAN");
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
                    Serial.println("BERIKUTNYA: PICU PEMBARUAN WAKTU SHALAT");

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

                        Serial.println("WAKTU SAAT INI: " + String(timeStr));
                        Serial.println("TANGGAL SAAT INI: " + String(dateStr));

                        if (timeinfo.tm_year + 1900 >= 2000) {
                            Serial.println("WAKTU VALID - MEMICU PEMBARUAN SHALAT");
                            Serial.println("KOTA: " + prayerConfig.selectedCity);
                            Serial.println("KOORDINAT: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                            Serial.println("");

                            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                                needPrayerUpdate = true;
                                pendingPrayerLat = prayerConfig.latitude;
                                pendingPrayerLon = prayerConfig.longitude;
                                xSemaphoreGive(settingsMutex);

                                if (prayerTaskHandle != NULL) {
                                    xTaskNotifyGive(prayerTaskHandle);
                                    Serial.println("TUGAS SHALAT DIPICU");
                                    Serial.println("STATUS: PEMBARUAN DI LATAR BELAKANG");
                                } else {
                                    Serial.println("ERROR: HANDLE TUGAS SHALAT NULL");
                                }
                            }
                        } else {
                            Serial.println("ERROR: WAKTU MASIH TIDAK VALID (TAHUN < 2000)");
                            Serial.println("MELEWATI PEMBARUAN WAKTU SHALAT");
                        }

                        Serial.println("========================================\n");

                    } else {
                        Serial.println("\n========================================");
                        Serial.println("PEMBARUAN WAKTU SHALAT DILEWATI");
                        Serial.println("========================================");
                        Serial.println("ALASAN: TIDAK ADA KOORDINAT KOTA");
                        Serial.println("AKSI: KONFIGURASI KOTA MELALUI ANTARMUKA WEB");
                        Serial.println("========================================\n");
                    }

                    autoUpdateDone = true;
                    Serial.println(">>> AUTOUPDATEDONE = TRUE <<<");
                    Serial.println("STATUS: SIKLUS AUTO-UPDATE SELESAI");
                    Serial.println("CATATAN: AKAN DIRESET SAAT PEMUTUSAN BERIKUTNYA\n");
                }
            }

            if (millis() - lastMonitor > 60000) {
                lastMonitor = millis();
                Serial.printf("[MONITOR WIFI] TERHUBUNG | RSSI: %d DBM | IP: %s",
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
                Serial.println("\n[TUGAS WIFI] PERCOBAAN KONEKSI AWAL");
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
            Serial.println("\n[TUGAS NTP] RESTART SISTEM TERDETEKSI - MENANGGUHKAN");
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

        Serial.println("MENYINKRONKAN DENGAN SERVER NTP (UTC+0)...");
        Serial.println("ALASAN: DAPATKAN WAKTU UTC MENTAH DULU, TERAPKAN TIMEZONE SETELAH BERHASIL");
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
                Serial.println("\n[TUGAS NTP] SHUTDOWN TERDETEKSI DI TENGAH SINKRONISASI - MEMBATALKAN");
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
                Serial.printf("MENUNGGU SINKRONISASI NTP... (%d/%d) [%.1F DETIK]",
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
            Serial.printf("WAKTU UTC: %02d:%02d:%02d %02d/%02d/%04d",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("DURASI SINKRONISASI: %.1F DETIK", retry * 0.25);
            Serial.printf("SERVER NTP: %s", usedServer.c_str());
            Serial.println("========================================");

            Serial.println("\n========================================");
            Serial.println("MENERAPKAN OFFSET TIMEZONE");
            Serial.println("========================================");

            int currentOffset;
            if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                currentOffset = timezoneOffset;
                xSemaphoreGive(settingsMutex);
            } else {
                currentOffset = 7;
                Serial.println("PERINGATAN: TIDAK DAPAT MENDAPATKAN TIMEZONE DARI PENGATURAN, MENGGUNAKAN DEFAULT UTC+7");
            }

            Serial.printf("PENGATURAN TIMEZONE: UTC%+d", currentOffset);
            Serial.printf("OFFSET: %+d JAM (%+ld DETIK)",
                         currentOffset, (long)(currentOffset * 3600));

            time_t localTime = ntpTime + (currentOffset * 3600);

            struct tm localTimeinfo;
            localtime_r(&localTime, &localTimeinfo);

            Serial.println("");
            Serial.println("KONVERSI WAKTU:");
            Serial.printf("   SEBELUM: %02d:%02d:%02d %02d/%02d/%04d (UTC+0)",
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                        timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
            Serial.printf("   SESUDAH: %02d:%02d:%02d %02d/%02d/%04d (UTC%+d)",
                        localTimeinfo.tm_hour, localTimeinfo.tm_min, localTimeinfo.tm_sec,
                        localTimeinfo.tm_mday, localTimeinfo.tm_mon + 1, localTimeinfo.tm_year + 1900,
                        currentOffset);
            Serial.println("========================================");

            ntpTime = localTime;

        } else {
            Serial.println("\n========================================");
            Serial.println("SINKRONISASI NTP GAGAL");
            Serial.println("========================================");
            Serial.printf("TIMEOUT SETELAH %.1f DETIK\n", retry * 0.25);
            Serial.println("SEMUA SERVER NTP GAGAL MERESPONS");
            Serial.println("KEMUNGKINAN PENYEBAB:");
            Serial.println("  1. TIDAK ADA KONEKSI INTERNET");
            Serial.println("  2. FIREWALL MEMBLOKIR NTP (PORT 123)");
            Serial.println("  3. MASALAH DNS ROUTER");
            Serial.println("  4. ISP MEMBLOKIR NTP");
            Serial.println("");
            Serial.println("SISTEM AKAN MELANJUTKAN DENGAN WAKTU SAAT INI");
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

                Serial.println("WAKTU TERSIMPAN KE MEMORI SISTEM");

                DisplayUpdate update;
                update.type = DisplayUpdate::TIME_UPDATE;
                xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));

                Serial.println("PEMBARUAN LAYAR DIANTREKAN");
            } else {
                Serial.println("ERROR: GAGAL MENDAPATKAN TIMEMUTEX");
                Serial.println("WAKTU TIDAK TERSIMPAN KE SISTEM");
            }

            Serial.println("========================================");
        }

        if (rtcAvailable && syncSuccess) {
            Serial.println("\n========================================");
            Serial.println("MENYIMPAN WAKTU KE HARDWARE RTC");
            Serial.println("========================================");

            if (isRTCValid()) {
                Serial.println("STATUS RTC: VALID - SIAP DISIMPAN");
                Serial.println("MENYIMPAN WAKTU NTP KE RTC...");

                saveTimeToRTC();

                vTaskDelay(pdMS_TO_TICKS(500));

                if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    DateTime rtcNow = rtc.now();
                    xSemaphoreGive(i2cMutex);

                    Serial.println("");
                    Serial.println("VERIFIKASI RTC:");
                    Serial.printf("   WAKTU RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                                rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                                rtcNow.day(), rtcNow.month(), rtcNow.year());

                    if (isRTCTimeValid(rtcNow)) {
                        Serial.println("   STATUS: RTC BERHASIL DISIMPAN");
                        Serial.println("   WAKTU AKAN BERTAHAN SAAT RESTART");
                    } else {
                        Serial.println("   STATUS: SIMPAN RTC GAGAL");
                        Serial.println("   HARDWARE RTC MUNGKIN RUSAK");
                    }
                } else {
                    Serial.println("   STATUS: TIDAK DAPAT DIVERIFIKASI (I2C SIBUK)");
                }
            } else {
                Serial.println("STATUS RTC: TIDAK VALID - TIDAK DAPAT DISIMPAN");
                Serial.println("KEMUNGKINAN MASALAH:");
                Serial.println("  - BATERAI HABIS/TERPUTUS");
                Serial.println("  - KERUSAKAN HARDWARE");
                Serial.println("  - KERUSAKAN DATA WAKTU");
                Serial.println("");
                Serial.println("WAKTU NTP TIDAK AKAN DISIMPAN KE RTC");
                Serial.println("WAKTU AKAN DIRESET SAAT RESTART");
            }

            Serial.println("========================================");
        } else if (syncSuccess && !rtcAvailable) {
            Serial.println("\n========================================");
            Serial.println("RTC TIDAK TERSEDIA");
            Serial.println("========================================");
            Serial.println("HARDWARE RTC TIDAK TERDETEKSI SAAT BOOT");
            Serial.println("WAKTU AKAN DIRESET KE 01/01/2000 SAAT RESTART");
            Serial.println("PERTIMBANGKAN MEMASANG MODUL RTC DS3231");
            Serial.println("========================================");
        }

        if (syncSuccess && wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("PEMICU PEMBARUAN WAKTU SHALAT OTOMATIS");
            Serial.println("========================================");
            Serial.println("ALASAN: SINKRONISASI NTP BERHASIL");
            Serial.println("");

            if (prayerConfig.latitude.length() > 0 &&
                prayerConfig.longitude.length() > 0) {

                Serial.println("KONFIGURASI KOTA:");
                Serial.println("   KOTA: " + prayerConfig.selectedCity);
                Serial.println("   LINTANG: " + prayerConfig.latitude);
                Serial.println("   BUJUR: " + prayerConfig.longitude);
                Serial.println("");

                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = true;
                    pendingPrayerLat = prayerConfig.latitude;
                    pendingPrayerLon = prayerConfig.longitude;
                    xSemaphoreGive(settingsMutex);

                    Serial.println("MEMICU TUGAS SHALAT UNTUK PEMBARUAN...");

                    if (prayerTaskHandle != NULL) {
                        xTaskNotifyGive(prayerTaskHandle);
                        Serial.println("TUGAS SHALAT DIBERI TAHU - AKAN DIPERBARUI DI LATAR BELAKANG");
                    } else {
                        Serial.println("ERROR: HANDLE TUGAS SHALAT NULL");
                    }
                }

                Serial.println("========================================");

            } else {
                Serial.println("DILEWATI: TIDAK ADA KOORDINAT KOTA");
                Serial.println("========================================");
            }
        } else if (syncSuccess && !wifiConfig.isConnected) {
            Serial.println("\n========================================");
            Serial.println("PEMBARUAN WAKTU SHALAT DILEWATI");
            Serial.println("========================================");
            Serial.println("ALASAN: WIFI TIDAK TERHUBUNG");
            Serial.println("WAKTU SHALAT AKAN DIPERBARUI SAAT WIFI TERHUBUNG");
            Serial.println("========================================");
        }

        Serial.println("\n========================================");
        Serial.println("SIKLUS TUGAS NTP SELESAI");
        Serial.println("========================================");
        Serial.printf("HASIL: %s\n", syncSuccess ? "BERHASIL" : "GAGAL");
        if (syncSuccess) {
            Serial.printf("WAKTU AKHIR: %02d:%02d:%02d %02d/%02d/%04d\n",
                        hour(ntpTime), minute(ntpTime), second(ntpTime),
                        day(ntpTime), month(ntpTime), year(ntpTime));
            Serial.printf("TIMEZONE: UTC%+d\n", timezoneOffset);
            Serial.printf("SERVER NTP: %s\n", usedServer.c_str());
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
      Serial.printf("AWAL:     %6d BYTE (%.2F KB)\n",
                    initialHeap, initialHeap / 1024.0);
      Serial.printf("SEKARANG: %6d BYTE (%.2F KB)\n",
                    currentHeap, currentHeap / 1024.0);
      Serial.printf("TERENDAH: %6d BYTE (%.2F KB)\n",
                    lowestHeap, lowestHeap / 1024.0);
      Serial.printf("TERTINGGI:%6d BYTE (%.2F KB)\n",
                    highestHeap, highestHeap / 1024.0);
      Serial.printf("PUNCAK:   %6d BYTE (%.2F KB)\n",
                    usedFromLowest, usedFromLowest / 1024.0);

      if (usedFromLowest > 35000) {
        Serial.println("PERINGATAN: PENGGUNAAN MEMORI PUNCAK TINGGI");
      } else if (usedFromLowest > 25000) {
        Serial.println("PERHATIAN: PENGGUNAAN MEMORI PUNCAK SEDANG");
      } else {
        Serial.println("STATUS MEMORI: NORMAL");
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
          Serial.printf("KEBOCORAN: %d BYTE HILANG SEJAK PENGECEKAN TERAKHIR\n", leaked);
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

    if (wifiState == WIFI_FAILED && wifiConfig.routerSSID.length() > 0) {
      unsigned long backoff = 10000UL * (1UL << min(wifiRetryCount, 4));
      if (backoff > 120000UL) backoff = 120000UL;

      if (now - wifiFailedTime >= backoff) {
        wifiRetryCount++;
        Serial.println("\n========================================");
        Serial.printf("WIFI_GAGAL: PERCOBAAN #%d (BACKOFF %lu DETIK)\n",
            wifiRetryCount, backoff / 1000);

        unsigned long nextBackoff = 10000UL * (1UL << min(wifiRetryCount, 4));
        if (nextBackoff > 120000UL) nextBackoff = 120000UL;
        Serial.printf("JIKA GAGAL LAGI, PERCOBAAN BERIKUTNYA DALAM %lu DETIK\n", nextBackoff / 1000);
        Serial.println("========================================");

        reconnectAttempts = 0;
        esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), hostname.c_str());
        WiFi.begin(wifiConfig.routerSSID.c_str(), wifiConfig.routerPassword.c_str());
        wifiState = WIFI_CONNECTING;
        wifiFailedTime = now;
      }
    }
  }
}

void prayerTask(void *parameter) {
    esp_task_wdt_add(NULL);

    Serial.println("\n========================================");
    Serial.println("TUGAS SHALAT DIMULAI");
    Serial.println("========================================");
    Serial.println("UKURAN STACK: 12288 BYTE");
    Serial.println("MENUNGGU PEMICU...");
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
            Serial.printf("[TUGAS SHALAT] STACK TERSISA: %d BYTE\n", stackRemaining * 4);

            if (stackRemaining < 1000) {
                Serial.println("PERINGATAN: STACK TUGAS SHALAT SANGAT RENDAH!");
            }
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (needPrayerUpdate && pendingPrayerLat.length() > 0 && pendingPrayerLon.length() > 0) {
            esp_task_wdt_reset();

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("TUGAS SHALAT: DILEWATI - WIFI TIDAK TERHUBUNG");

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
                Serial.println("TUGAS SHALAT: DILEWATI - WAKTU SISTEM TIDAK VALID");
                Serial.printf("TIMESTAMP SAAT INI: %ld (SEBELUM 01/01/2000)\n", now_t);

                if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    needPrayerUpdate = false;
                    xSemaphoreGive(settingsMutex);
                }
                continue;
            }

            Serial.println("\n========================================");
            Serial.println("TUGAS SHALAT: MEMPROSES PEMBARUAN");
            Serial.println("========================================");
            Serial.printf("STACK SEBELUM HTTP: %d BYTE TERSISA\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("KOORDINAT: " + pendingPrayerLat + ", " + pendingPrayerLon);

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

            Serial.printf("STACK SETELAH HTTP: %d BYTE TERSISA\n", uxTaskGetStackHighWaterMark(NULL) * 4);
            Serial.println("TUGAS SHALAT: PEMBARUAN SELESAI");
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
                Serial.println("TENGAH MALAM TERDETEKSI - MEMULAI URUTAN");
                Serial.println("========================================");
                Serial.printf("WAKTU: %02d:%02d:%02d\n", currentHour, currentMinute, second(timeConfig.currentTime));
                Serial.printf("TANGGAL: %02d/%02d/%04d\n", currentDay, month(timeConfig.currentTime), currentYear);
                Serial.println("");

                Serial.println("MEMICU SINKRONISASI NTP...");
                Serial.println("ALASAN: MEMASTIKAN WAKTU AKURAT SEBELUM MEMPERBARUI");

                if (ntpTaskHandle != NULL) {
                    ntpSyncInProgress = false;
                    ntpSyncCompleted = false;

                    xTaskNotifyGive(ntpTaskHandle);

                    waitingForMidnightNTP = true;
                    midnightNTPStartTime = millis();

                    Serial.println("SINKRONISASI NTP BERHASIL DIPICU");
                    Serial.println("MENUNGGU SINKRONISASI NTP SELESAI...");
                    Serial.println("========================================\n");
                } else {
                    Serial.println("ERROR: HANDLE TUGAS NTP NULL");
                    Serial.println("MELEWATI PEMBARUAN TENGAH MALAM");
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

                        Serial.printf("WAKTU BARU: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
                        Serial.printf("TANGGAL BARU: %02d/%02d/%04d\n", currentDay, currentMonth, currentYear);
                        Serial.printf("TIMESTAMP: %ld\n", currentTimestamp);
                        Serial.println("");
                    }

                    if (prayerConfig.latitude.length() > 0 &&
                        prayerConfig.longitude.length() > 0) {

                        if (currentYear >= 2000 && currentTimestamp >= 946684800) {
                            Serial.println("MEMPERBARUI WAKTU SHALAT...");
                            Serial.println("STATUS WAKTU: VALID");
                            Serial.println("KOTA: " + prayerConfig.selectedCity);
                            Serial.println("KOORDINAT: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                            Serial.println("");

                            esp_task_wdt_reset();
                            getPrayerTimesByCoordinates(
                                prayerConfig.latitude,
                                prayerConfig.longitude
                            );
                            esp_task_wdt_reset();

                            Serial.println("\nURUTAN PEMBARUAN TENGAH MALAM SELESAI");
                        } else {
                            Serial.println("PERINGATAN: WAKTU MASIH TIDAK VALID SETELAH NTP");
                            Serial.printf("   TAHUN: %d (MIN: 2000)\n", currentYear);
                            Serial.printf("   TIMESTAMP: %ld (MIN: 946684800)\n", currentTimestamp);
                            Serial.println("   MENGGUNAKAN WAKTU SHALAT YANG ADA");
                        }
                    } else {
                        Serial.println("PERINGATAN: TIDAK ADA KOORDINAT KOTA");
                        Serial.println("   MENGGUNAKAN WAKTU SHALAT YANG ADA");
                    }

                    Serial.println("========================================\n");

                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;

                } else if (waitTime > MAX_WAIT_TIME) {
                    Serial.println("\n========================================");
                    Serial.println("TIMEOUT SINKRONISASI NTP");
                    Serial.println("========================================");
                    Serial.printf("WAKTU TUNGGU: %lu MS (MAKS: %lu MS)\n", waitTime, MAX_WAIT_TIME);
                    Serial.println("STATUS NTP:");
                    Serial.printf("   NTPSYNCINPROGRESS: %s\n", ntpSyncInProgress ? "true" : "false");
                    Serial.printf("   NTPSYNCCOMPLETED: %s\n", ntpSyncCompleted ? "false" : "false");
                    Serial.println("");
                    Serial.println("KEPUTUSAN: GUNAKAN WAKTU SHALAT YANG ADA");
                    Serial.println("JANGAN PERBARUI (WAKTU MUNGKIN TIDAK AKURAT)");
                    Serial.println("========================================\n");

                    waitingForMidnightNTP = false;
                    hasUpdatedToday = true;

                } else {
                    if (waitTime % 5000 < 1000) {
                        Serial.printf("MENUNGGU SINKRONISASI NTP... (%lu/%lu MS)\n",
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
    const TickType_t xFrequency = pdMS_TO_TICKS(60000);

    Serial.println("\n========================================");
    Serial.println("TUGAS SINKRONISASI RTC DIMULAI");
    Serial.println("========================================");
    Serial.println("FUNGSI: SINKRONISASI WAKTU SISTEM DARI RTC");
    Serial.println("INTERVAL: SETIAP 1 MENIT");
    Serial.println("TUJUAN: SUMBER WAKTU CADANGAN SAAT WIFI TIDAK TERSEDIA");
    Serial.println("========================================\n");

    while (true) {
        if (rtcAvailable) {
            if (!isRTCValid()) {
                Serial.println("\n[SINKRONISASI RTC] DILEWATI - RTC TIDAK VALID");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }

            DateTime rtcTime;
            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                rtcTime = rtc.now();
                xSemaphoreGive(i2cMutex);
            } else {
                Serial.println("\n[SINKRONISASI RTC] DILEWATI - TIDAK DAPAT MENDAPATKAN MUTEX I2C");
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                continue;
            }

            if (!isRTCTimeValid(rtcTime)) {
                Serial.println("\n[SINKRONISASI RTC] DILEWATI - WAKTU RTC TIDAK VALID");
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

                Serial.println("\n[SINKRONISASI RTC] DILEWATI");
                Serial.println("ALASAN: " + reason);
                Serial.printf("   SISTEM: %02d:%02d:%02d %02d/%02d/%04d (DARI NTP)\n",
                             hour(systemTime), minute(systemTime), second(systemTime),
                             day(systemTime), month(systemTime), year(systemTime));
                Serial.printf("   RTC:    %02d:%02d:%02d %02d/%02d/%04d (LEBIH LAMA)\n",
                             rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                             rtcTime.day(), rtcTime.month(), rtcTime.year());
                Serial.println("   AKSI: RTC AKAN DIPERBARUI PADA SINKRONISASI NTP BERIKUTNYA\n");

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
                    Serial.println("ALASAN: " + reason);
                    Serial.printf("SISTEM LAMA: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 hour(systemTime), minute(systemTime), second(systemTime),
                                 day(systemTime), month(systemTime), year(systemTime));
                    Serial.printf("SISTEM BARU: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                 rtcTime.day(), rtcTime.month(), rtcTime.year());
                    Serial.printf("SELISIH WAKTU: %d DETIK\n", timeDiff);
                    Serial.println("========================================\n");
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================
// INTERNET CHECK TASK - CHECK INTERNET CONNECTION EVERY 30 SECONDS
// ============================================
void internetCheckTask(void *parameter) {
  const TickType_t checkInterval = pdMS_TO_TICKS(30000);

  Serial.println("TUGAS CEK INTERNET DIMULAI (INTERVAL 30 DETIK)");

  while (true) {
    vTaskDelay(checkInterval);

    if (wifiState != WIFI_CONNECTED) {
      internetAvailable = false;
      continue;
    }

    WiFiClient client;
    bool result = client.connect(IPAddress(8, 8, 8, 8), 53, 3000);
    if (result) {
      client.stop();
    }

    if (result != internetAvailable) {
      internetAvailable = result;
      Serial.println(internetAvailable
        ? "[Internet] Koneksi internet tersedia"
        : "[Internet] Koneksi internet terputus (WiFi masih konek)");
    }
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
                Serial.printf("  TIMESTAMP TIDAK VALID: %ld\n", timeConfig.currentTime);
                Serial.println("  INI SEBELUM 01/01/2000 00:00:00");
                Serial.println("  MEMAKSA RESET KE: 01/01/2000 00:00:00");

                setTime(0, 0, 0, 1, 1, 2000);
                timeConfig.currentTime = now();

                if (timeConfig.currentTime < EPOCH_2000) {
                    Serial.println("MASALAH TIMELIB.H - MENGGUNAKAN TIMESTAMP HARDCODED");
                    timeConfig.currentTime = EPOCH_2000;
                }

                Serial.printf("WAKTU DIKOREKSI KE: %ld (01/01/2000 00:00:00)\n",
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
                    Serial.println("\nSINKRONISASI NTP OTOMATIS (PER JAM)");
                    xTaskNotifyGive(ntpTaskHandle);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================
// TASK RESTART WIFI DAN ACCESS POINT
// ============================================
void restartWiFiTask(void *parameter) {
    unsigned long now = millis();
    if (now - lastWiFiRestartRequest < RESTART_DEBOUNCE_MS) {
        unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastWiFiRestartRequest);

        Serial.println("\n========================================");
        Serial.println("RESTART WIFI DITOLAK - TERLALU CEPAT");
        Serial.println("========================================");
        Serial.printf("ALASAN: RESTART TERAKHIR %lu MS YANG LALU\n", now - lastWiFiRestartRequest);
        Serial.printf("INTERVAL MINIMUM: %lu MS\n", RESTART_DEBOUNCE_MS);
        Serial.printf("HARAP TUNGGU: %lu MS (%.1F DETIK)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");

        vTaskDelete(NULL);
        return;
    }

    lastWiFiRestartRequest = now;

    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
        if (wifiRestartMutex == NULL) {
            Serial.println("ERROR: GAGAL MEMBUAT WIFIRESTARTMUTEX");
            vTaskDelete(NULL);
            return;
        }
    }

    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("\n========================================");
        Serial.println("RESTART WIFI DIBLOKIR");
        Serial.println("========================================");
        Serial.println("ALASAN: RESTART WIFI/AP LAIN SEDANG BERJALAN");
        Serial.println("AKSI: PERMINTAAN DIABAIKAN UNTUK KEAMANAN");
        Serial.println("========================================\n");

        vTaskDelete(NULL);
        return;
    }

    if (wifiRestartInProgress || apRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("RESTART WIFI DIBATALKAN");
        Serial.println("========================================");
        Serial.printf("RESTART WIFI SEDANG BERJALAN: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("RESTART AP SEDANG BERJALAN: %s\n", apRestartInProgress ? "YES" : "NO");
        Serial.println("========================================\n");

        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }

    wifiRestartInProgress = true;

    Serial.println("\n========================================");
    Serial.println("URUTAN RESTART WIFI AMAN DIMULAI");
    Serial.println("========================================");
    Serial.println("PERLINDUNGAN: DEBOUNCING + MUTEX LOCK AKTIF");
    Serial.println("MODE: KONEKSI ULANG AMAN (TANPA PERGANTIAN MODE)");
    Serial.println("========================================\n");

    vTaskDelay(pdMS_TO_TICKS(3000));

    Serial.println("MEMPERSIAPKAN KONEKSI ULANG...");

    String ssid, password;
    if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ssid = wifiConfig.routerSSID;
        password = wifiConfig.routerPassword;
        wifiConfig.isConnected = false;
        wifiState = WIFI_IDLE;
        reconnectAttempts = 0;
        xSemaphoreGive(wifiMutex);

        Serial.println("   KREDENSIAL DIMUAT DARI MEMORI");
        Serial.println("   SSID: " + ssid);
        Serial.println("   STATUS KONEKSI DIRESET");
    } else {
        Serial.println("   ERROR: GAGAL MENDAPATKAN WIFIMUTEX");
        wifiRestartInProgress = false;
        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }

    Serial.println("\nMEMUTUSKAN WIFI LAMA...");
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("   TERPUTUS (KONFIGURASI DIPERTAHANKAN)");

    Serial.println("\nMEMVERIFIKASI STATUS AP...");

    IPAddress apIP = WiFi.softAPIP();
    if (apIP == IPAddress(0, 0, 0, 0)) {
        Serial.println("   PERINGATAN: AP MATI SAAT PEMUTUSAN");
        Serial.println("   MEMULIHKAN AP...");

        WiFi.softAPdisconnect(false);
        vTaskDelay(pdMS_TO_TICKS(500));

        WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
        bool apStarted = WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (apStarted) {
            Serial.println("   AP BERHASIL DIPULIHKAN");
            Serial.println("   IP AP: " + WiFi.softAPIP().toString());
        } else {
            Serial.println("   ERROR: GAGAL MEMULIHKAN AP");
        }
    } else {
        Serial.println("   AP MASIH AKTIF: " + apIP.toString());
    }

    if (ssid.length() > 0) {
        Serial.println("\nMENGHUBUNGKAN ULANG KE WIFI...");
        Serial.println("   TARGET SSID: " + ssid);
        Serial.println("   MEMULAI KONEKSI...");

        WiFi.begin(ssid.c_str(), password.c_str());

        Serial.println("\n========================================");
        Serial.println("KONEKSI ULANG WIFI DIMULAI");
        Serial.println("========================================");
        Serial.println("STATUS: PERMINTAAN KONEKSI DIKIRIM");
        Serial.println("MONITOR: TUGAS WIFI AKAN MENANGANI KONEKSI");
        Serial.println("DIHARAPKAN: LIHAT EVENT WIFI DI LOG SERIAL");
        Serial.println("========================================\n");
    } else {
        Serial.println("\n========================================");
        Serial.println("KONEKSI ULANG WIFI GAGAL");
        Serial.println("========================================");
        Serial.println("ALASAN: TIDAK ADA SSID YANG DIKONFIGURASI DI MEMORI");
        Serial.println("AKSI: KONFIGURASI WIFI MELALUI ANTARMUKA WEB");
        Serial.println("========================================\n");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    wifiRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);

    Serial.println("URUTAN RESTART WIFI SELESAI");
    Serial.println("KUNCI DILEPAS - SISTEM SIAP UNTUK PERMINTAAN BERIKUTNYA\n");

    vTaskDelete(NULL);
}

void restartAPTask(void *parameter) {
    unsigned long now = millis();
    if (now - lastAPRestartRequest < RESTART_DEBOUNCE_MS) {
        unsigned long waitTime = RESTART_DEBOUNCE_MS - (now - lastAPRestartRequest);

        Serial.println("\n========================================");
        Serial.println("RESTART AP DITOLAK - TERLALU CEPAT");
        Serial.println("========================================");
        Serial.printf("ALASAN: RESTART TERAKHIR %lu MS YANG LALU\n", now - lastAPRestartRequest);
        Serial.printf("INTERVAL MINIMUM: %lu MS\n", RESTART_DEBOUNCE_MS);
        Serial.printf("HARAP TUNGGU: %lu MS (%.1F DETIK)\n", waitTime, waitTime / 1000.0);
        Serial.println("========================================\n");

        vTaskDelete(NULL);
        return;
    }

    lastAPRestartRequest = now;

    if (wifiRestartMutex == NULL) {
        wifiRestartMutex = xSemaphoreCreateMutex();
        if (wifiRestartMutex == NULL) {
            Serial.println("ERROR: GAGAL MEMBUAT WIFIRESTARTMUTEX");
            vTaskDelete(NULL);
            return;
        }
    }

    if (xSemaphoreTake(wifiRestartMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("\n========================================");
        Serial.println("RESTART AP DIBLOKIR");
        Serial.println("========================================");
        Serial.println("ALASAN: RESTART WIFI/AP LAIN SEDANG BERJALAN");
        Serial.println("AKSI: PERMINTAAN DIABAIKAN UNTUK KEAMANAN");
        Serial.println("========================================\n");

        vTaskDelete(NULL);
        return;
    }

    if (apRestartInProgress || wifiRestartInProgress) {
        Serial.println("\n========================================");
        Serial.println("RESTART AP DIBATALKAN");
        Serial.println("========================================");
        Serial.printf("RESTART WIFI SEDANG BERJALAN: %s\n", wifiRestartInProgress ? "YES" : "NO");
        Serial.printf("RESTART AP SEDANG BERJALAN: %s\n", apRestartInProgress ? "YES" : "NO");
        Serial.println("========================================\n");

        xSemaphoreGive(wifiRestartMutex);
        vTaskDelete(NULL);
        return;
    }

    apRestartInProgress = true;

    Serial.println("\n========================================");
    Serial.println("TUGAS RESTART AP DIMULAI");
    Serial.println("========================================");
    Serial.println("HITUNG MUNDUR SEBELUM AP DIMATIKAN");
    Serial.println("========================================\n");

    for (int i = 60; i > 0; i--) {
        if (i == 35) {
            Serial.println("\n========================================");
            Serial.println("MEMATIKAN AP");
            Serial.println("========================================");

            int clientsBefore = WiFi.softAPgetStationNum();
            Serial.printf("KLIEN TERHUBUNG: %d\n", clientsBefore);

            if (clientsBefore > 0) {
                Serial.println("MEMUTUSKAN KLIEN...");
                esp_wifi_deauth_sta(0);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            WiFi.mode(WIFI_MODE_STA);
            WiFi.softAPdisconnect(true);

            Serial.println("AP BERHASIL DIMATIKAN");
            Serial.println("SEMUA KLIEN TERPUTUS");
            Serial.println("========================================\n");
        }

        if (i % 10 == 0 || i <= 5) {
            Serial.printf("AP AKAN RESTART DALAM %d DETIK...\n", i);
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

        Serial.println("KONFIGURASI BARU DIMUAT:");
        Serial.println("  SSID: " + String(savedSSID));
        Serial.println("  IP: " + savedAPIP.toString());
    } else {
        Serial.println("ERROR: TIDAK DAPAT MEMUAT KONFIGURASI - MENGGUNAKAN DEFAULT");
        DEFAULT_AP_SSID.toCharArray(savedSSID, sizeof(savedSSID));
        strncpy(savedPassword, DEFAULT_AP_PASSWORD, sizeof(savedPassword));
        savedAPIP = IPAddress(192, 168, 100, 1);
        savedGateway = IPAddress(192, 168, 100, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);
    }

    WiFi.mode(WIFI_MODE_APSTA);

    Serial.println("\nMENGKONFIGURASI JARINGAN AP BARU...");
    WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
    vTaskDelay(pdMS_TO_TICKS(500));

    Serial.println("MEMULAI SIARAN AP BARU...");
    bool apStarted = WiFi.softAP(savedSSID, savedPassword);
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (apStarted) {
        IPAddress newAPIP = WiFi.softAPIP();
        String currentSSID = WiFi.softAPSSID();

        Serial.println("\n========================================");
        Serial.println("RESTART AP BERHASIL");
        Serial.println("========================================");
        Serial.println("SSID: \"" + currentSSID + "\" (AKTIF)");
        Serial.println("IP: " + newAPIP.toString());
        Serial.println("MAC: " + WiFi.softAPmacAddress());

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("");
            Serial.println("KONEKSI WIFI: DIPERTAHANKAN");
            Serial.println("SSID ROUTER: " + WiFi.SSID());
            Serial.println("IP ROUTER: " + WiFi.localIP().toString());
        }

        Serial.println("");
        Serial.println("TINDAKAN KLIEN DIPERLUKAN:");
        Serial.println("  1. CARI: \"" + currentSSID + "\"");
        Serial.println("  2. HUBUNGKAN DENGAN PASSWORD");
        Serial.println("  3. BUKA: http://" + newAPIP.toString());
        Serial.println("========================================\n");

    } else {
        Serial.println("\n========================================");
        Serial.println("RESTART AP GAGAL");
        Serial.println("========================================");
        Serial.println("MENGEMBALIKAN KE AP DEFAULT...");

        DEFAULT_AP_SSID.toCharArray(savedSSID, sizeof(savedSSID));
        strcpy(savedPassword, DEFAULT_AP_PASSWORD);
        savedAPIP = IPAddress(192, 168, 100, 1);
        savedGateway = IPAddress(192, 168, 100, 1);
        savedSubnet = IPAddress(255, 255, 255, 0);

        WiFi.softAPConfig(savedAPIP, savedGateway, savedSubnet);
        vTaskDelay(pdMS_TO_TICKS(500));

        WiFi.softAP(savedSSID, savedPassword);
        vTaskDelay(pdMS_TO_TICKS(2000));

        Serial.println("KEMBALI KE: " + String(savedSSID));
        Serial.println("========================================\n");
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    apRestartInProgress = false;
    xSemaphoreGive(wifiRestartMutex);

    Serial.println("\n========================================");
    Serial.println("PERLINDUNGAN RESTART AP DILEPAS");
    Serial.println("========================================");
    Serial.println("APRESTARTINPROGRESS: FALSE");
    Serial.println("PEMANTAUAN WEBTASK: AKTIF KEMBALI");
    Serial.println("========================================\n");

    Serial.println("TUGAS RESTART AP SELESAI\n");
    vTaskDelete(NULL);
}

bool initDFPlayer() {
  Serial.println("\n========================================");
  Serial.println("INISIALISASI DFPLAYER MINI");
  Serial.println("========================================");
  Serial.println("UART2: TX=GPIO25, RX=GPIO32");

  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(500);

  if (!dfPlayer.begin(dfSerial, true, true)) {
    Serial.println("KONEKSI DFPLAYER GAGAL!");
    Serial.println("PERIKSA KONEKSI KABEL:");
    Serial.println("  ESP32 TX (GPIO25) → DFPLAYER RX");
    Serial.println("  ESP32 RX (GPIO32) → DFPLAYER TX");
    Serial.println("  VCC → 5V");
    Serial.println("  GND → GND");
    Serial.println("  SPEAKER → SPK_1 & SPK_2 ATAU AMPLIFIER DAC_R & DAC_L");
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

  Serial.println("DFPLAYER BERHASIL DIINISIALISASI!");
  Serial.println("UART2: TX=GPIO25, RX=GPIO32");
  Serial.println("VOLUME: 15/30");
  Serial.println("FILE DI SD: " + String(fileCount));
  Serial.println("========================================\n");

  return true;
}

void setDFPlayerVolume(int vol) {
  if (!dfPlayerAvailable) return;

  int dfVol = map(vol, 0, 100, 0, 30);
  dfPlayer.volume(dfVol);
  Serial.println("VOLUME DFPLAYER: " + String(dfVol) + "/30");
}

void playDFPlayerAdzan(String prayerName) {
  if (!dfPlayerAvailable) {
    Serial.println("DFPLAYER TIDAK TERSEDIA");
    return;
  }

  int trackNumber = 0;

  if (prayerName == "subuh") trackNumber = 1;
  else if (prayerName == "zuhur") trackNumber = 2;
  else if (prayerName == "ashar") trackNumber = 3;
  else if (prayerName == "maghrib") trackNumber = 4;
  else if (prayerName == "isya") trackNumber = 5;

  if (trackNumber == 0) {
    Serial.println("NAMA SHALAT TIDAK VALID: " + prayerName);
    return;
  }

  Serial.println("\n========================================");
  Serial.println("MEMUTAR ADZAN: " + prayerName);
  Serial.println("========================================");
  Serial.println("TRACK: " + String(trackNumber));
  Serial.println("FILE: /000" + String(trackNumber) + ".mp3");
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
      Serial.println("TUGAS AUDIO DIPICU UNTUK: " + adzanState.currentPrayer);

      playDFPlayerAdzan(adzanState.currentPrayer);

      unsigned long startTime = millis();
      const unsigned long maxDuration = 600000;

      while (isDFPlayerPlaying() && (millis() - startTime < maxDuration)) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_task_wdt_reset();
      }

      Serial.println("PEMUTARAN ADZAN SELESAI");

      adzanState.isPlaying = false;
      adzanState.canTouch = false;
      adzanState.currentPrayer = "";
      saveAdzanState();

      Serial.println("STATUS ADZAN DIBERSIHKAN");
    }
  }
}

// ============================================
// TASK HTTP - KHUSUS PERMINTAAN API
// ============================================
void httpTask(void *parameter) {
  esp_task_wdt_add(NULL);

  Serial.println("\n========================================");
  Serial.println("TUGAS HTTP DIMULAI");
  Serial.println("========================================");
  Serial.println("UKURAN STACK: 8192 BYTE");
  Serial.println("TUJUAN: MENANGANI PERMINTAAN API WAKTU SHALAT");
  Serial.println("========================================\n");

  HTTPRequest request;

  while (true) {
    esp_task_wdt_reset();

    if (xQueueReceive(httpQueue, &request, pdMS_TO_TICKS(10000)) == pdTRUE) {
      esp_task_wdt_reset();

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("TUGAS HTTP: WIFI TIDAK TERHUBUNG - MELEWATI PERMINTAAN");
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }

      Serial.println("\n========================================");
      Serial.println("TUGAS HTTP: MEMPROSES WAKTU SHALAT");
      Serial.println("========================================");
      Serial.println("KOORDINAT: " + request.latitude + ", " + request.longitude);

      time_t now_t;
      if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        now_t = timeConfig.currentTime;
        xSemaphoreGive(timeMutex);
      } else {
        now_t = time(nullptr);
      }

      if (now_t < 946684800) {
        Serial.println("WAKTU SISTEM TIDAK VALID - DILEWATI");
        continue;
      }

      char dateStr[12];
      sprintf(dateStr, "%02d-%02d-%04d", day(now_t), month(now_t), year(now_t));

      int currentMethod = methodConfig.methodId;
      String tuneParam = String(prayerConfig.tuneImsak) + "," +
                        String(prayerConfig.tuneSubuh) + "," +
                        String(prayerConfig.tuneTerbit) + "," +
                        String(prayerConfig.tuneZuhur) + "," +
                        String(prayerConfig.tuneAshar) + "," +
                        String(prayerConfig.tuneMaghrib) + "," +
                        "0," +
                        String(prayerConfig.tuneIsya) + "," +
                        "0";

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

      Serial.println("KODE RESPONS: " + String(httpResponseCode));

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

            Serial.println("WAKTU SHALAT BERHASIL DIPERBARUI");
            savePrayerTimes();

            DisplayUpdate update;
            update.type = DisplayUpdate::PRAYER_UPDATE;
            xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
          } else {
            Serial.println("DATA WAKTU SHALAT TIDAK VALID");
          }
        } else {
          Serial.println("ERROR PARSE JSON: " + String(error.c_str()));
        }
      } else {
        Serial.println("PERMINTAAN HTTP GAGAL: " + String(httpResponseCode));
      }

      http.end();
      client.stop();

      Serial.println("TUGAS HTTP: PERMINTAAN SELESAI");
      Serial.println("========================================\n");

      esp_task_wdt_reset();
    }

    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ================================
// SETUP - ESP32 CORE 3.X
// ================================
void setup() {
#if !PRODUCTION
  Serial.begin(115200);
  delay(1000);
#endif

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("ESP32 JAM SHALAT ISLAM");
  Serial.println("LVGL 9.2.0 + FREERTOS");
  Serial.println("AKSES BERSAMAAN DIOPTIMALKAN");
  Serial.println("VERSI 2.3 - TUGAS HTTP DIPISAHKAN");
  Serial.println("========================================\n");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  Serial.println("LAMPU LATAR: MATI");

  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);
  rgbOff();
  rgbBootBlinking = true;
  xTaskCreate(rgbBootBlinkTask, "RGBBoot", 1024, NULL, 1, NULL);
  Serial.println("RGB LED: KEDIP BOOT AKTIF (MERAH)");

  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  Serial.println("TFT DIINISIALISASI");

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

  Serial.println("SEMAPHORE & ANTRIAN DIBUAT");

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
    Serial.println("TUGAS AUDIO OK");
  } else {
    Serial.println("DFPLAYER DINONAKTIFKAN - TIDAK ADA PEMUTARAN AUDIO");
    Serial.println("MENGHAPUS STATUS ADZAN YANG TERTUNDA...");

    adzanState.isPlaying = false;
    adzanState.canTouch = false;
    adzanState.currentPrayer = "";

    if (LittleFS.exists("/adzan_state.txt")) {
      LittleFS.remove("/adzan_state.txt");
      Serial.println("FILE STATUS ADZAN DIHAPUS (TIDAK ADA SISTEM AUDIO)");
    }

    Serial.println("STATUS ADZAN DIBERSIHKAN - MODE HANYA BUZZER AKTIF");
  }

  if (wifiConfig.apIP == IPAddress(0, 0, 0, 0)) {
    wifiConfig.apIP = IPAddress(192, 168, 4, 1);
    wifiConfig.apGateway = IPAddress(192, 168, 4, 1);
    wifiConfig.apSubnet = IPAddress(255, 255, 255, 0);
  }

  init_littlefs();
  createDefaultConfigFiles();
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
      Serial.println("\nRTC TERSEDIA");
      Serial.println("WAKTU BERHASIL DIMUAT DARI RTC");
      Serial.println("WAKTU AKAN BERTAHAN SAAT RESTART");
  } else {
      Serial.println("\nRTC TIDAK TERSEDIA - WAKTU AKAN DIRESET SAAT RESTART");

      if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
          const time_t EPOCH_2000 = 946684800;
          setTime(0, 0, 0, 1, 1, 2000);
          timeConfig.currentTime = EPOCH_2000;

          if (timeConfig.currentTime < EPOCH_2000) {
              timeConfig.currentTime = EPOCH_2000;
          }

          xSemaphoreGive(timeMutex);
          Serial.printf("WAKTU AWAL DIATUR: %ld (01/01/2000 00:00:00 UTC)\n", EPOCH_2000);
      }
  }

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  Serial.println("LAYAR SENTUH DIINISIALISASI");

  ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcWrite(BUZZER_CHANNEL, 0);
  Serial.println("BUZZER DIINISIALISASI (GPIO26)");

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

  Serial.println("LVGL DIINISIALISASI");

  ui_init();
  Serial.println("EEZ UI DIINISIALISASI");

  hideAllUIElements();
  Serial.println("ELEMEN UI DISEMBUNYIKAN");

  delay(100);
  lv_timer_handler();
  delay(100);
  lv_timer_handler();
  delay(100);

  tft.fillScreen(TFT_BLACK);

  Serial.println("UI DIRENDER (LAYAR HITAM)");

  Serial.println("MEMULAI LAMPU LATAR...");

  ledcAttach(TFT_BL, TFT_BL_FREQ, TFT_BL_RESOLUTION);
  ledcWrite(TFT_BL, TFT_BL_BRIGHTNESS);
  Serial.printf("LAMPU LATAR MENYALA: %d/255\n", TFT_BL_BRIGHTNESS);

  Serial.println("\n========================================");
  Serial.println("KONFIGURASI WIFI");
  Serial.println("========================================");

  setupWiFiEvents();

  WiFi.mode(WIFI_OFF);
  delay(500);

  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif != NULL) {
    esp_netif_set_hostname(sta_netif, hostname.c_str());
    Serial.print("HOSTNAME DIATUR VIA ESP-IDF: ");
    Serial.println(hostname.c_str());
  } else {
    Serial.println("PERINGATAN: TIDAK DAPAT MENDAPATKAN HANDLE STA NETIF");
  }

  WiFi.mode(WIFI_AP_STA);
  delay(100);

  Serial.println("MENERAPKAN OPTIMASI WIFI UNTUK AKSES ROUTER...");

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.println("  PROTOKOL: 802.11 B/G/N");
  Serial.println("  BANDWIDTH: 40MHZ (HT40)");
  Serial.println("  DAYA TX: 19.5DBM (MAKSIMUM)");

  WiFi.setSleep(WIFI_PS_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_wifi_set_max_tx_power(78);

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  Serial.println("MODE WIFI: AP + STA");
  Serial.println("SLEEP WIFI: GANDA DINONAKTIFKAN");
  Serial.println("  ARDUINO: WIFI_PS_NONE");
  Serial.println("  ESP-IDF: WIFI_PS_NONE");
  Serial.println("DAYA WIFI: MAKSIMUM (19.5DBM)");
  Serial.println("AUTO RECONNECT: DIAKTIFKAN");
  Serial.println("PERSISTENT: DINONAKTIFKAN");
  Serial.println("========================================\n");

  WiFi.softAPConfig(wifiConfig.apIP, wifiConfig.apGateway, wifiConfig.apSubnet);
  WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
  delay(100);

  Serial.printf("AP DIMULAI: %s\n", wifiConfig.apSSID);
  Serial.printf("PASSWORD: %s\n", wifiConfig.apPassword);
  Serial.print("IP AP: ");
  Serial.println(WiFi.softAPIP());
  Serial.printf("MAC AP: %s\n", WiFi.softAPmacAddress().c_str());

  timeConfig.ntpServer = "pool.ntp.org";
  timeConfig.ntpSynced = false;

  if (prayerConfig.selectedCity.length() > 0) {
    Serial.println("\nKOTA DIPILIH: " + prayerConfig.selectedCity);
    Serial.println("\nWAKTU SHALAT DIMUAT:");
    Serial.println("KOTA: " + prayerConfig.selectedCity);
    Serial.println("IMSAK: " + prayerConfig.imsakTime);
    Serial.println("SUBUH: " + prayerConfig.subuhTime);
    Serial.println("TERBIT: " + prayerConfig.terbitTime);
    Serial.println("ZUHUR: " + prayerConfig.zuhurTime);
    Serial.println("ASHAR: " + prayerConfig.asharTime);
    Serial.println("MAGHRIB: " + prayerConfig.maghribTime);
    Serial.println("ISYA: " + prayerConfig.isyaTime);

    DisplayUpdate update;
    update.type = DisplayUpdate::PRAYER_UPDATE;
    xQueueSend(displayQueue, &update, pdMS_TO_TICKS(100));
  } else {
    Serial.println("\nTIDAK ADA KOTA DIPILIH");
    Serial.println("SILAKAN PILIH KOTA MELALUI ANTARMUKA WEB");
  }

  Serial.println("\nMENGKONFIGURASI WATCHDOG...");

  esp_task_wdt_deinit();

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 100000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
  if (wdt_err == ESP_OK) {
    Serial.println("WATCHDOG DIKONFIGURASI (TIMEOUT 100 DETIK)");
  } else {
    Serial.printf("ERROR INISIALISASI WATCHDOG: %s\n", esp_err_to_name(wdt_err));
  }

  Serial.println("\n========================================");
  Serial.println("MEMULAI TUGAS FREERTOS");
  Serial.println("========================================");

  xTaskCreatePinnedToCore(
    uiTask,
    "UI",
    UI_TASK_STACK_SIZE,
    NULL,
    UI_TASK_PRIORITY,
    &uiTaskHandle,
    1
  );
  Serial.printf("TUGAS UI (CORE 1) - STACK: %d BYTE\n", UI_TASK_STACK_SIZE);

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
    0
  );
  Serial.printf("TUGAS WIFI (CORE 0) - STACK: %d BYTE\n", WIFI_TASK_STACK_SIZE);

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
    0
  );
  Serial.printf("TUGAS NTP (CORE 0) - STACK: %d BYTE\n", NTP_TASK_STACK_SIZE);

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
    0
  );
  Serial.printf("TUGAS WEB (CORE 0) - STACK: %d BYTE\n", WEB_TASK_STACK_SIZE);

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
    0
  );
  Serial.printf("TUGAS HTTP (CORE 0) - STACK: 8192 BYTE\n");

  if (httpTaskHandle) {
    esp_task_wdt_add(httpTaskHandle);
    Serial.println("  TUGAS HTTP WDT TERDAFTAR");
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
    0
  );
  Serial.printf("TUGAS SHALAT (CORE 0) - STACK: %d BYTE\n", PRAYER_TASK_STACK_SIZE);

  if (prayerTaskHandle) {
    esp_task_wdt_add(prayerTaskHandle);
    Serial.println("  TUGAS SHALAT WDT TERDAFTAR");
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
    0
  );
  Serial.printf("TUGAS JAM (CORE 0) - STACK: %d BYTE\n", CLOCK_TASK_STACK_SIZE);

  // ================================
  // INTERNET CHECK TASK
  // ================================
  xTaskCreate(
    internetCheckTask,
    "InternetCheck",
    3072,
    NULL,
    1,
    NULL
  );
  Serial.println("TUGAS CEK INTERNET (CORE BEBAS) - STACK: 3072 BYTE");

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
      0
    );
    Serial.printf("TUGAS SINKRONISASI RTC (CORE 0) - STACK: %d BYTE\n", RTC_TASK_STACK_SIZE);
  }

  // ================================
  // PRAYER WATCHDOG TASK
  // ================================
  xTaskCreate(
    [](void* param) {
      const TickType_t checkInterval = pdMS_TO_TICKS(30000);

      Serial.println("TUGAS WATCHDOG SHALAT - MEMANTAU SETIAP 30 DETIK");

      while (true) {
        vTaskDelay(checkInterval);

        if (prayerTaskHandle != NULL) {
          eTaskState state = eTaskGetState(prayerTaskHandle);

          if (state == eDeleted || state == eInvalid) {
            Serial.println("\n========================================");
            Serial.println("KRITIS: TUGAS SHALAT CRASH");
            Serial.println("========================================");
            Serial.println("STATUS TERDETEKSI: " + String(state == eDeleted ? "DELETED" : "INVALID"));
            Serial.println("AKSI: MEMULAI ULANG TUGAS OTOMATIS...");
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
              Serial.println("\nTUGAS SHALAT BERHASIL DIMULAI ULANG");
              Serial.println("STACK: " + String(PRAYER_TASK_STACK_SIZE) + " BYTE");
              Serial.println("WDT: TERDAFTAR ULANG");
              Serial.println("========================================\n");
            } else {
              Serial.println("\nGAGAL MEMULAI ULANG TUGAS SHALAT!");
              Serial.println("SISTEM MUNGKIN TIDAK STABIL");
              Serial.println("========================================\n");
            }
          }
        } else {
          Serial.println("\nPERINGATAN: HANDLE TUGAS SHALAT NULL");
          Serial.println("MENCOBA MEMBUAT TUGAS...");

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
            Serial.println("TUGAS SHALAT BERHASIL DIBUAT\n");
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

  Serial.println("\nMENDAFTARKAN TUGAS KE WATCHDOG:");

  if (wifiTaskHandle) {
    esp_task_wdt_add(wifiTaskHandle);
    Serial.println("  TUGAS WIFI");
  }
  if (webTaskHandle) {
    esp_task_wdt_add(webTaskHandle);
    Serial.println("  TUGAS WEB");
  }
  if (ntpTaskHandle) {
    esp_task_wdt_add(ntpTaskHandle);
    Serial.println("  TUGAS NTP");
  }

  Serial.println("========================================\n");

  Serial.println("========================================");
  Serial.println("LAPORAN MEMORI");
  Serial.println("========================================");

  size_t freeHeap = ESP.getFreeHeap();
  size_t totalHeap = ESP.getHeapSize();
  size_t usedHeap = totalHeap - freeHeap;

  Serial.printf("TOTAL HEAP:  %d BYTE (%.2F KB)\n", totalHeap, totalHeap / 1024.0);
  Serial.printf("HEAP TERPAKAI: %d BYTE (%.2F KB)\n", usedHeap, usedHeap / 1024.0);
  Serial.printf("HEAP TERSISA: %d BYTE (%.2F KB)\n", freeHeap, freeHeap / 1024.0);
  Serial.printf("PENGGUNAAN:       %.1f%%\n", (usedHeap * 100.0) / totalHeap);

  Serial.println("\nALOKASI STACK TUGAS:");
  uint32_t totalStack = UI_TASK_STACK_SIZE + WIFI_TASK_STACK_SIZE +
                        NTP_TASK_STACK_SIZE + WEB_TASK_STACK_SIZE +
                        PRAYER_TASK_STACK_SIZE + CLOCK_TASK_STACK_SIZE +
                        8192;
  if (rtcAvailable) totalStack += RTC_TASK_STACK_SIZE;

  Serial.printf("TOTAL:        %d BYTE (%.2F KB)\n", totalStack, totalStack / 1024.0);
  Serial.printf("TUGAS UI:      %d BYTE\n", UI_TASK_STACK_SIZE);
  Serial.printf("TUGAS WEB:     %d BYTE\n", WEB_TASK_STACK_SIZE);
  Serial.printf("TUGAS HTTP:    8192 BYTE (BARU)\n");
  Serial.printf("TUGAS SHALAT:  %d BYTE (DIKURANGI DARI 16384)\n", PRAYER_TASK_STACK_SIZE);
  Serial.println("========================================\n");

  Serial.println("========================================");
  Serial.println("SISTEM SIAP");
  Serial.println("========================================");
  Serial.println("AKSES BERSAMAAN MULTI-KLIEN DIAKTIFKAN");
  Serial.println("SLEEP WIFI DINONAKTIFKAN UNTUK RESPONS LEBIH BAIK");
  Serial.println("OPTIMASI ROUTER AKTIF (KEEP-ALIVE)");
  Serial.println("PEMULIHAN OTOMATIS TUGAS SHALAT DIAKTIFKAN");
  Serial.println("TUGAS HTTP DIPISAHKAN (NON-BLOCKING)");
  Serial.println("PEMANTAUAN STACK AKTIF");
  Serial.println("MEMORI DIOPTIMALKAN (HEMAT ~8KB)");
  Serial.println("========================================\n");

  if (wifiConfig.routerSSID.length() > 0) {
    Serial.println("WIFI DIKONFIGURASI, AKAN AUTO-CONNECT...");
    Serial.println("SSID: " + wifiConfig.routerSSID);
  } else {
    Serial.println("HUBUNGKAN KE AP UNTUK KONFIGURASI:");
    Serial.println("1. WIFI: " + String(wifiConfig.apSSID));
    Serial.println("2. PASSWORD: " + String(wifiConfig.apPassword));
    Serial.println("3. BROWSER: http://192.168.100.1");
    Serial.println("4. ATUR WIFI & PILIH KOTA");
  }

  if (prayerConfig.selectedCity.length() == 0) {
    Serial.println("\nPENGINGAT: PILIH KOTA MELALUI ANTARMUKA WEB");
  }

  Serial.println("\nBOOT SELESAI - SIAP MENERIMA KONEKSI");
  rgbBootDone();
  Serial.println("LOG PEMANTAUAN AKAN MUNCUL DI BAWAH:");
  Serial.println("  - LAPORAN PENGGUNAAN STACK SETIAP 60 DETIK");
  Serial.println("  - PEMERIKSAAN KESEHATAN TUGAS SHALAT SETIAP 30 DETIK");
  Serial.println("  - LOG PEMROSESAN TUGAS HTTP");
  Serial.println("  - LAPORAN MEMORI SETIAP 30 DETIK (DARI WEBTASK)");
  Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}