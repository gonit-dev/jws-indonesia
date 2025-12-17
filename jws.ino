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
#define UI_TASK_STACK_SIZE 16384
#define WIFI_TASK_STACK_SIZE 8192
#define NTP_TASK_STACK_SIZE 8192
#define WEB_TASK_STACK_SIZE 16384
#define PRAYER_TASK_STACK_SIZE 8192
#define WEB_SERVER_MAX_CLIENTS 5
#define WEB_SERVER_STACK_SIZE 8192


#define UI_TASK_PRIORITY 3
#define WIFI_TASK_PRIORITY 2
#define NTP_TASK_PRIORITY 2
#define WEB_TASK_PRIORITY 1
#define PRAYER_TASK_PRIORITY 1

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
const char *ntpServers[] = {
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

// Global configurations
WiFiConfig wifiConfig;
TimeConfig timeConfig;
PrayerConfig prayerConfig;
MethodConfig methodConfig = { 5, "Egyptian General Authority of Survey" };
int timezoneOffset = 7;

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
volatile bool ntpSyncInProgress = false;
volatile bool ntpSyncCompleted = false;

// ================================
// FORWARD DECLARATIONS
// ================================

// Display Functions
void updateCityDisplay();
void updateTimeDisplay();
void updatePrayerDisplay();
void hideAllUIElements();
void showAllUIElements();

// Prayer Times Functions
void getPrayerTimesByCoordinates(String lat, String lon);
void savePrayerTimes();
void loadPrayerTimes();

// WiFi Functions
void saveWiFiCredentials();
void loadWiFiCredentials();
void saveAPCredentials();
void setupWiFiEvents();

// Settings Functions
void saveTimezoneConfig();
void loadTimezoneConfig();
void saveCitySelection();
void loadCitySelection();
void saveMethodSelection();
void loadMethodSelection();

// Time Functions
void saveTimeToRTC();
bool initRTC();

// Server Functions
void setupServerRoutes();

// Utility Functions
void scheduleRestart(int delaySeconds);
void delayedRestart(void *parameter);

// LittleFS Functions
bool init_littlefs();

// LVGL Callbacks
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data);

// RTOS Tasks
void uiTask(void *parameter);
void wifiTask(void *parameter);
void ntpTask(void *parameter);
void webTask(void *parameter);
void prayerTask(void *parameter);
void rtcSyncTask(void *parameter);
void clockTickTask(void *parameter);

// Display Functions
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

// Prayer Times Functions
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
    Serial.println("\nPRAYER TIMES UPDATE BLOCKED!");
    Serial.println("========================================");
    Serial.println("Reason: Invalid system time detected");
    Serial.printf("Current timestamp: %ld\n", now_t);
    Serial.println("This would send wrong date to API!");
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

    DynamicJsonDocument doc(8192);
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

// WiFi Functions
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
        file.close();
      }
    } else {
      strcpy(wifiConfig.apSSID, "JWS Indonesia");
      strcpy(wifiConfig.apPassword, "12345678");
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

void setupWiFiEvents() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[WiFi-Event] %d\n", event);

    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("STA Connected to AP");
        break;

      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("Got IP: " + WiFi.localIP().toString());
        break;

      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("STA Disconnected!");
        Serial.printf("Reason Code: %d\n", info.wifi_sta_disconnected.reason);

        switch (info.wifi_sta_disconnected.reason) {
          case WIFI_REASON_AUTH_EXPIRE:
            Serial.println("Detail: Authentication expired");
            break;
          case WIFI_REASON_AUTH_LEAVE:
            Serial.println("Detail: Deauthenticated (router disconnect)");
            break;
          case WIFI_REASON_ASSOC_LEAVE:
            Serial.println("Detail: Disassociated");
            break;
          case WIFI_REASON_ASSOC_EXPIRE:
            Serial.println("Detail: Association expired");
            break;
          case WIFI_REASON_NOT_AUTHED:
            Serial.println("Detail: Not authenticated");
            break;
          case WIFI_REASON_NOT_ASSOCED:
            Serial.println("Detail: Not associated");
            break;
          case WIFI_REASON_ASSOC_TOOMANY:
            Serial.println("Detail: Too many stations");
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
            Serial.printf("Detail: Unknown reason (%d)\n", info.wifi_sta_disconnected.reason);
        }

        wifiDisconnectedTime = millis();

        if (wifiConfig.isConnected) {
          Serial.println("Triggering auto-reconnect...");
          wifiConfig.isConnected = false;
          wifiState = WIFI_IDLE;
        }
        break;

      case ARDUINO_EVENT_WIFI_AP_START:
        Serial.println("AP Started: " + String(WiFi.softAPSSID()));
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
        break;

      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Serial.println("Client connected to AP");
        Serial.printf("Stations: %d\n", WiFi.softAPgetStationNum());
        break;

      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.println("Client disconnected from AP");
        Serial.printf("Stations: %d\n", WiFi.softAPgetStationNum());
        break;
    }
  });

  Serial.println("WiFi Event Handler registered");
}

// Settings Functions
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
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = now();
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
        Serial.println("Temperature sensor works: " + String(rtc.getTemperature()) + "°C");
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
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = now();
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

// Server Functions
void setupServerRoutes() {
    // ========================================
    // HTML - NO CACHE (Always Fresh)
    // ========================================
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS, "/index.html", "text/html");

        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    // ========================================
    // milligram.min CSS - CACHE 1 JAM
    // ========================================
    server.on("/assets/css/foundation.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse *response = request->beginResponse(
        LittleFS, "/assets/css/foundation.min.css", "text/css");

      response->addHeader("Cache-Control", "public, max-age=3600");
      request->send(response);
    });

    // ========================================
    // DEVICE STATUS - NO CACHE
    // ========================================
    server.on("/devicestatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        char timeStr[20];
        char dateStr[20];

        sprintf(timeStr, "%02d:%02d:%02d",
                hour(timeConfig.currentTime),
                minute(timeConfig.currentTime),
                second(timeConfig.currentTime));

        sprintf(dateStr, "%02d/%02d/%04d",
                day(timeConfig.currentTime),
                month(timeConfig.currentTime),
                year(timeConfig.currentTime));

        bool isWiFiConnected = (WiFi.status() == WL_CONNECTED && wifiConfig.isConnected && wifiConfig.localIP.toString() != "0.0.0.0");

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
        response += "\"currentDate\":\"" + String(dateStr) + "\",";
        response += "\"uptime\":" + String(millis() / 1000) + ",";
        response += "\"freeHeap\":\"" + String(ESP.getFreeHeap()) + "\"";
        response += "}";

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    // ========================================
    // TIMEZONE - GET & SET
    // ========================================
    server.on("/gettimezone", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"offset\":" + String(timezoneOffset);
            xSemaphoreGive(settingsMutex);
        } else {
            json += "\"offset\":7";
        }

        json += "}";

        Serial.println("GET /gettimezone: " + json);

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    server.on("/settimezone", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("\n========================================");
        Serial.println("POST /settimezone received");
        Serial.println("========================================");

        if (!request->hasParam("offset", true)) {
            Serial.println("ERROR: Missing offset parameter");
            request->send(400, "application/json",
                          "{\"error\":\"Missing offset parameter\"}");
            return;
        }

        String offsetStr = request->getParam("offset", true)->value();
        offsetStr.trim();

        int offset = offsetStr.toInt();

        Serial.println("Received offset: " + String(offset));

        if (offset < -12 || offset > 14) {
            Serial.println("ERROR: Invalid timezone offset");
            request->send(400, "application/json",
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

        bool prayerTimesUpdated = false;

        if (wifiConfig.isConnected && prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
            Serial.println("\n========================================");
            Serial.println("AUTO-UPDATING PRAYER TIMES");
            Serial.println("========================================");
            Serial.println("Reason: Timezone changed to UTC" + String(offset >= 0 ? "+" : "") + String(offset));
            Serial.println("City: " + prayerConfig.selectedCity);
            Serial.println("Coordinates: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
            Serial.println("");

            getPrayerTimesByCoordinates(
                prayerConfig.latitude,
                prayerConfig.longitude);

            Serial.println("Prayer times updated with new timezone");
            Serial.println("========================================\n");

            prayerTimesUpdated = true;
        } else {
            Serial.println("\nPrayer times auto-update skipped:");
            if (!wifiConfig.isConnected) {
                Serial.println("WiFi not connected");
            }
            if (prayerConfig.latitude.length() == 0 || prayerConfig.longitude.length() == 0) {
                Serial.println("No city coordinates available");
            }
            Serial.println("");
        }

        if (wifiConfig.isConnected && ntpTaskHandle != NULL) {
            Serial.println("\n========================================");
            Serial.println("AUTO-TRIGGERING NTP RE-SYNC");
            Serial.println("========================================");
            Serial.println("Reason: Timezone changed");
            Serial.println("New timezone: UTC" + String(offset >= 0 ? "+" : "") + String(offset));
            Serial.println("Will apply to system time immediately...");
            Serial.println("========================================\n");

            if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                timeConfig.ntpSynced = false;
                xSemaphoreGive(timeMutex);
            }

            xTaskNotifyGive(ntpTaskHandle);

            Serial.println("NTP re-sync triggered successfully");
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
        response += "\"ntpTriggered\":" + String((wifiConfig.isConnected && ntpTaskHandle != NULL) ? "true" : "false") + ",";
        response += "\"prayerTimesUpdated\":" + String(prayerTimesUpdated ? "true" : "false");
        response += "}";

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        request->send(resp);
    });

    // ========================================
    // PRAYER TIMES - GET
    // ========================================
    server.on("/getprayertimes", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"imsak\":\"" + prayerConfig.imsakTime + "\",";
        json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
        json += "\"terbit\":\"" + prayerConfig.terbitTime + "\",";
        json += "\"zuhur\":\"" + prayerConfig.zuhurTime + "\",";
        json += "\"ashar\":\"" + prayerConfig.asharTime + "\",";
        json += "\"maghrib\":\"" + prayerConfig.maghribTime + "\",";
        json += "\"isya\":\"" + prayerConfig.isyaTime + "\"";
        json += "}";

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    // ========================================
    // CITIES.JSON - GET & UPLOAD (CACHE 1 JAM)
    // ========================================
    server.on("/getcities", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("GET /getcities");

        if (!LittleFS.exists("/cities.json")) {
            Serial.println("cities.json not found");
            request->send(404, "application/json", "[]");
            return;
        }

        AsyncWebServerResponse *response = request->beginResponse(
            LittleFS,
            "/cities.json",
            "application/json");

        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "public, max-age=3600"); // 1 jam

        response->setContentLength(LittleFS.open("/cities.json", "r").size());

        request->send(response);

        Serial.println("cities.json sent");
    });

    // ========================================
    // CITY SELECTION - GET & SET
    // ========================================
    server.on("/setcity", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("\n========================================");
        Serial.println("POST /setcity received");
        Serial.print("Client IP: ");
        Serial.println(request->client()->remoteIP().toString());
        Serial.println("========================================");

        if (!request->hasParam("city", true)) {
            Serial.println("ERROR: Missing 'city' parameter");

            int params = request->params();
            Serial.printf("Received parameters (%d):\n", params);
            for (int i = 0; i < params; i++) {
                const AsyncWebParameter *p = request->getParam(i);
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
        Serial.println("City API: " + cityApi);
        Serial.println("City Name: " + cityName);
        Serial.println("Latitude: " + lat);
        Serial.println("Longitude: " + lon);

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
            Serial.println("Memory updated");
            Serial.println("selectedCity (API): " + prayerConfig.selectedCity);
            Serial.println("selectedCityName (Display): " + prayerConfig.selectedCityName);
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
                        Serial.printf("File saved (%d bytes)\n", bytesWritten);
                        Serial.println("Line 1 (API): " + prayerConfig.selectedCity);
                        Serial.println("Line 2 (Display): " + prayerConfig.selectedCityName);
                        Serial.println("Line 3 (Lat): " + prayerConfig.latitude);
                        Serial.println("Line 4 (Lon): " + prayerConfig.longitude);
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
                String line1 = verifyFile.readStringUntil('\n');
                line1.trim();
                String line2 = verifyFile.readStringUntil('\n');
                line2.trim();
                String line3 = verifyFile.readStringUntil('\n');
                line3.trim();
                String line4 = verifyFile.readStringUntil('\n');
                line4.trim();

                Serial.println("Line 1: " + line1);
                Serial.println("Line 2: " + line2);
                Serial.println("Line 3: " + line3);
                Serial.println("Line 4: " + line4);

                verifyFile.close();
                Serial.printf("File verified (size: %d bytes)\n", fileSize);
            }
        } else {
            Serial.println("WARNING: File verification failed - file not found");
        }

        Serial.println("Updating display...");
        updateCityDisplay();
        Serial.println("Display updated");

        bool willFetchPrayerTimes = false;

        if (WiFi.status() == WL_CONNECTED) {
            if (lat.length() > 0 && lon.length() > 0) {
                Serial.println("Fetching prayer times with coordinates...");
                Serial.println("City: " + prayerConfig.selectedCity);
                Serial.println("Lat: " + lat);
                Serial.println("Lon: " + lon);

                getPrayerTimesByCoordinates(lat, lon);

                Serial.println("Prayer times update initiated");
                willFetchPrayerTimes = true;
            } else {
                Serial.println("No coordinates provided - cannot fetch prayer times");
            }
        } else {
            Serial.println("WiFi not connected - prayer times will update when online");
        }

        Serial.println("========================================");
        Serial.println("SUCCESS: City saved successfully");
        Serial.println("API Name: " + prayerConfig.selectedCity);
        Serial.println("Display Name: " + prayerConfig.selectedCityName);
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

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        request->send(resp);
    });

    server.on("/getcityinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
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

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    server.on(
        "/uploadcities", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            String jsonSizeStr = "";
            String citiesCountStr = "";

            if (request->hasParam("jsonSize", true)) {
                jsonSizeStr = request->getParam("jsonSize", true)->value();
            }

            if (request->hasParam("citiesCount", true)) {
                citiesCountStr = request->getParam("citiesCount", true)->value();
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

            AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", "{\"success\":true}");
            request->send(resp);
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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

                    Serial.println("\nUpload complete!");
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

    // ========================================
    // METHOD SELECTION - GET & SET
    // ========================================
    server.on("/getmethod", HTTP_GET, [](AsyncWebServerRequest *request) {
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

        Serial.println("GET /getmethod: " + json);

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    server.on("/setmethod", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("\n========================================");
        Serial.println("POST /setmethod received");
        Serial.print("Client IP: ");
        Serial.println(request->client()->remoteIP().toString());
        Serial.println("========================================");

        if (!request->hasParam("methodId", true) || !request->hasParam("methodName", true)) {
            Serial.println("ERROR: Missing parameters");

            int params = request->params();
            Serial.printf("Received parameters (%d):\n", params);
            for (int i = 0; i < params; i++) {
                const AsyncWebParameter *p = request->getParam(i);
                Serial.printf("  %s = %s\n", p->name().c_str(), p->value().c_str());
            }

            request->send(400, "application/json",
                          "{\"error\":\"Missing methodId or methodName parameter\"}");
            return;
        }

        String methodIdStr = request->getParam("methodId", true)->value();
        String methodName = request->getParam("methodName", true)->value();

        methodIdStr.trim();
        methodName.trim();

        int methodId = methodIdStr.toInt();

        Serial.println("Received data:");
        Serial.println("Method ID: " + String(methodId));
        Serial.println("Method Name: " + methodName);

        if (methodId < 0 || methodId > 20) {
            Serial.println("ERROR: Invalid method ID");
            request->send(400, "application/json",
                          "{\"error\":\"Invalid method ID\"}");
            return;
        }

        if (methodName.length() == 0) {
            Serial.println("ERROR: Empty method name");
            request->send(400, "application/json",
                          "{\"error\":\"Method name cannot be empty\"}");
            return;
        }

        if (methodName.length() > 100) {
            Serial.println("ERROR: Method name too long");
            request->send(400, "application/json",
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

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        request->send(resp);
    });

    // ========================================
    // WIFI SETTINGS - GET & SET
    // ========================================
    server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String newSSID = request->getParam("ssid", true)->value();
            String newPassword = request->getParam("password", true)->value();

            Serial.println("\n========================================");
            Serial.println("POST /setwifi");
            Serial.println("========================================");
            Serial.println("New SSID: " + newSSID);

            if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                wifiConfig.routerSSID = newSSID;
                wifiConfig.routerPassword = newPassword;
                xSemaphoreGive(wifiMutex);
            }

            saveWiFiCredentials();

            Serial.println("Disconnecting old WiFi...");
            WiFi.disconnect(false);
            delay(200);

            Serial.println("Forcing AP_STA mode...");
            WiFi.mode(WIFI_AP_STA);
            delay(100);

            IPAddress apIP = WiFi.softAPIP();
            if (apIP == IPAddress(0, 0, 0, 0)) {
                Serial.println("AP died, restarting...");
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                delay(200);
                Serial.println("AP restored: " + WiFi.softAPIP().toString());
            } else {
                Serial.println("AP still alive: " + apIP.toString());
            }

            Serial.println("Triggering WiFi task reconnect...");
            if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                wifiConfig.isConnected = false;
                wifiState = WIFI_IDLE;
                reconnectAttempts = 0;
                xSemaphoreGive(wifiMutex);
            }

            Serial.println("========================================");
            Serial.println("WiFi config saved, AP secured");
            Serial.println("WiFi task will auto-reconnect");
            Serial.println("========================================\n");

            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
            request->send(resp);

        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    server.on("/getwificonfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";

        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"routerSSID\":\"" + wifiConfig.routerSSID + "\",";
            xSemaphoreGive(wifiMutex);
        } else {
            json += "\"routerSSID\":\"\",";
        }

        String currentAPSSID = WiFi.softAPSSID();
        if (currentAPSSID.length() == 0 || currentAPSSID == "null") {
            currentAPSSID = String(wifiConfig.apSSID);
        }

        if (currentAPSSID.length() == 0) {
            currentAPSSID = "JWS Indonesia";
        }

        json += "\"apSSID\":\"" + currentAPSSID + "\"";
        json += "}";

        Serial.println("GET /getwificonfig: " + json);

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    server.on("/setap", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String pass = request->getParam("password", true)->value();

            if (pass.length() > 0 && pass.length() < 8) {
                request->send(400, "text/plain", "Password minimal 8 karakter");
                return;
            }

            Serial.println("\n========================================");
            Serial.println("POST /setap");
            Serial.println("========================================");
            Serial.println("New AP SSID: " + ssid);

            ssid.toCharArray(wifiConfig.apSSID, 33);
            pass.toCharArray(wifiConfig.apPassword, 65);
            saveAPCredentials();

            Serial.println("Stopping old AP...");
            WiFi.softAPdisconnect(false);
            delay(200);

            Serial.println("Forcing AP_STA mode...");
            WiFi.mode(WIFI_AP_STA);
            delay(100);

            Serial.println("Starting new AP...");
            WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
            delay(200);

            IPAddress newAPIP = WiFi.softAPIP();
            Serial.println("========================================");
            Serial.println("AP Settings updated");
            Serial.println("New SSID: " + String(wifiConfig.apSSID));
            Serial.println("New IP: " + newAPIP.toString());
            Serial.println("========================================\n");

            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
            request->send(resp);

        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    // ========================================
    // TIME SYNC FROM BROWSER
    // ========================================
    server.on("/synctime", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("y", true) && 
            request->hasParam("m", true) && 
            request->hasParam("d", true) && 
            request->hasParam("h", true) && 
            request->hasParam("i", true) && 
            request->hasParam("s", true)) {

            int y = request->getParam("y", true)->value().toInt();
            int m = request->getParam("m", true)->value().toInt();
            int d = request->getParam("d", true)->value().toInt();
            int h = request->getParam("h", true)->value().toInt();
            int i = request->getParam("i", true)->value().toInt();
            int s = request->getParam("s", true)->value().toInt();

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
                xQueueSend(displayQueue, &update, 0);

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

            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "Waktu berhasil di-sync!");
            request->send(resp);
            
        } else {
            request->send(400, "text/plain", "Data waktu tidak lengkap");
        }
    });

    // ========================================
    // API DATA ENDPOINT - NO CACHE
    // ========================================
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";

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

            const char *dayNames[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
            int dayOfWeek = weekday(timeConfig.currentTime) - 1;
            strcpy(dayStr, dayNames[dayOfWeek]);

            json += "\"time\":\"" + String(timeStr) + "\",";
            json += "\"date\":\"" + String(dateStr) + "\",";
            json += "\"day\":\"" + String(dayStr) + "\",";
            json += "\"timestamp\":" + String(timeConfig.currentTime) + ",";

            xSemaphoreGive(timeMutex);
        }

        if (xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            json += "\"prayerTimes\":{";
            json += "\"imsak\":\"" + prayerConfig.imsakTime + "\",";
            json += "\"subuh\":\"" + prayerConfig.subuhTime + "\",";
            json += "\"terbit\":\"" + prayerConfig.terbitTime + "\",";
            json += "\"zuhur\":\"" + prayerConfig.zuhurTime + "\",";
            json += "\"ashar\":\"" + prayerConfig.asharTime + "\",";
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

        json += "\"device\":{";
        json += "\"wifiConnected\":" + String((WiFi.status() == WL_CONNECTED && wifiConfig.isConnected) ? "true" : "false") + ",";
        json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\",";
        json += "\"ntpSynced\":" + String(timeConfig.ntpSynced ? "true" : "false") + ",";
        json += "\"ntpServer\":\"" + timeConfig.ntpServer + "\",";
        json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "}";

        json += "}";

        Serial.println("API /api/data requested");

        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", json);
        resp->addHeader("Cache-Control", "no-cache");
        request->send(resp);
    });

    // ========================================
    // 404 NOT FOUND PAGE
    // ========================================
    server.on("/notfound", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        html += "<div class='icon'>🔍</div>";
        html += "<div class='error-code'>404</div>";
        html += "<h2>Page Not Found</h2>";
        html += "<p>The page you're looking for doesn't exist or you don't have permission to access it. Please return to the home page.</p>";
        html += "<a href='/' class='btn'>← Back to Home</a>";
        html += "</div></body></html>";

        AsyncWebServerResponse *resp = request->beginResponse(404, "text/html", html);
        request->send(resp);
    });

    // ========================================
    // FACTORY RESET
    // ========================================
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("\n========================================");
        Serial.println("FACTORY RESET STARTED");
        Serial.println("========================================");
        
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

        if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
            methodConfig.methodId = 5;
            methodConfig.methodName = "Egyptian General Authority of Survey";
            xSemaphoreGive(settingsMutex);
        }

        timezoneOffset = 7;
        Serial.println("\nResetting time to 00:00:00 01/01/2000...");
        
        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            setTime(0, 0, 0, 1, 1, 2000);
            timeConfig.currentTime = now();
            timeConfig.ntpSynced = false;
            timeConfig.ntpServer = "";
            
            DisplayUpdate update;
            update.type = DisplayUpdate::TIME_UPDATE;
            xQueueSend(displayQueue, &update, 0);
            
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
        
        Serial.println("Time reset complete");

        if (xSemaphoreTake(settingsMutex, portMAX_DELAY) == pdTRUE) {
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
            
            strcpy(wifiConfig.apSSID, "JWS Indonesia");
            strcpy(wifiConfig.apPassword, "12345678");
            
            xSemaphoreGive(settingsMutex);
        }
        
        updateCityDisplay();
        WiFi.disconnect(true);
        
        Serial.println("\n========================================");
        Serial.println("FACTORY RESET COMPLETE");
        Serial.println("Device will restart in 5 seconds...");
        Serial.println("========================================\n");
        
        AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
        request->send(resp);
        
        scheduleRestart(5);
    });

    // ========================================
    // MANUAL RESTART
    // ========================================
    server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("\n========================================");
        Serial.println("MANUAL RESTART REQUESTED");
        Serial.println("========================================");
        Serial.println("Device will restart in 5 seconds...");
        Serial.println("========================================\n");

        AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", "OK");
        request->send(resp);

        scheduleRestart(5);
    });

    // ========================================
    // 404 HANDLER - REDIRECT TO /notfound
    // ========================================
    server.onNotFound([](AsyncWebServerRequest *request) {
        String url = request->url();
        IPAddress clientIP = request->client()->remoteIP();

        Serial.printf("\n[404] Client: %s | URL: %s\n",
                      clientIP.toString().c_str(), url.c_str());

        if (url.startsWith("/assets/") || url.endsWith(".css") || url.endsWith(".js") || 
            url.endsWith(".png") || url.endsWith(".jpg") || url.endsWith(".jpeg") || 
            url.endsWith(".gif") || url.endsWith(".ico") || url.endsWith(".svg") || 
            url.endsWith(".woff") || url.endsWith(".woff2") || url.endsWith(".ttf")) {

            Serial.println("Static asset not found (returning 404)");
            request->send(404, "text/plain", "File not found");
            return;
        }

        Serial.println("Invalid URL, redirecting to /notfound");
        request->redirect("/notfound");
    });
}

// Utility Functions
void scheduleRestart(int delaySeconds) {
  static int delay = delaySeconds;
  xTaskCreate(
    delayedRestart,
    "RestartTask",
    2048,
    &delay,
    1,
    NULL);
}

void delayedRestart(void *parameter) {
  int delaySeconds = *((int *)parameter);

  Serial.printf("Restarting in %d seconds...\n", delaySeconds);

  vTaskDelay(pdMS_TO_TICKS(500));

  for (int i = delaySeconds; i > 0; i--) {
    Serial.printf("Restart in %d...\n", i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  Serial.println("Restarting NOW!");

  Serial.flush();
  delay(100);

  ESP.restart();
  vTaskDelete(NULL);
}

bool init_littlefs() {
  Serial.println("Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return false;
  }
  Serial.println("LittleFS Mounted");
  return true;
}

// LVGL Callbacks
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

// RTOS Tasks
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
  esp_task_wdt_add(NULL);
  int connectAttempt = 0;
  const int MAX_CONNECT_ATTEMPTS = 20;
  unsigned long lastConnectAttempt = 0;
  const unsigned long RECONNECT_INTERVAL = 10000;
  bool wasConnected = false;
  bool autoUpdateDone = false;

  lastFastScan = millis();

  Serial.println("\n========================================");
  Serial.println("WiFi Task Started - Fast Reconnect Mode");
  Serial.println("========================================\n");

  while (true) {
    esp_task_wdt_reset();

    // ================================================
    // STOP SCANNING SAAT CONNECTED
    // ================================================
    if (wifiConfig.isConnected && WiFi.status() == WL_CONNECTED) {
      WiFi.scanDelete();
    }

    // ================================================
    // FAST SCAN MODE - Hanya saat disconnected
    // ================================================
    if (!wifiConfig.isConnected && wifiConfig.routerSSID.length() > 0) {
      unsigned long now = millis();

      if (now - lastFastScan >= FAST_SCAN_INTERVAL) {
        lastFastScan = now;

        int n = WiFi.scanComplete();

        if (n == WIFI_SCAN_FAILED) {
          if (wifiState != WIFI_CONNECTING) {
            WiFi.scanNetworks(true, false, false, 300);
            Serial.println("Fast scan started...");
          }

        } else if (n >= 0) {
          bool ssidFound = false;
          int8_t bestRSSI = -100;

          for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == wifiConfig.routerSSID) {
              ssidFound = true;
              bestRSSI = WiFi.RSSI(i);
              break;
            }
          }

          if (ssidFound) {
            Serial.println("\nTARGET SSID DETECTED!");
            Serial.println("SSID: " + wifiConfig.routerSSID);
            Serial.println("RSSI: " + String(bestRSSI) + " dBm");

            if (wifiState != WIFI_CONNECTING) {
              Serial.println("Triggering immediate connection...\n");

              if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                WiFi.scanDelete();
                delay(100);

                wifi_mode_t currentMode;
                esp_wifi_get_mode(&currentMode);

                if (currentMode != WIFI_MODE_APSTA) {
                  Serial.println("Mode bukan AP_STA, forcing restore...");
                  WiFi.mode(WIFI_AP_STA);
                  delay(100);

                  if (WiFi.softAPgetStationNum() == 0 && WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
                    WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                    delay(100);
                    Serial.println("AP restarted: " + String(wifiConfig.apSSID));
                  }
                }

                WiFi.setHostname("JWS-Indonesia");
                delay(100);

                WiFi.setTxPower(WIFI_POWER_19_5dBm);

                Serial.println("========================================");
                Serial.println("FAST RECONNECT: Connecting to router...");
                Serial.println("========================================");
                Serial.println("SSID: " + wifiConfig.routerSSID);
                Serial.println("Signal: " + String(bestRSSI) + " dBm");

                WiFi.begin(wifiConfig.routerSSID.c_str(), wifiConfig.routerPassword.c_str());

                wifiState = WIFI_CONNECTING;
                connectAttempt = 0;
                lastConnectAttempt = millis();
                reconnectAttempts++;

                Serial.printf("Connecting... (reconnect attempt %d/%d)\n",
                              reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
                Serial.println("========================================\n");

                xSemaphoreGive(wifiMutex);

              } else {
                Serial.println("Failed to acquire wifiMutex for fast reconnect");
              }
            }

          } else {
            static int scanCount = 0;
            scanCount++;

            if (scanCount % 5 == 0) {
              Serial.printf("Scan #%d: '%s' not found (scanned %d networks)\n",
                            scanCount, wifiConfig.routerSSID.c_str(), n);
            }

            WiFi.scanDelete();
          }
        }
      }
    }

    switch (wifiState) {
      case WIFI_IDLE:
        {
          if (wifiConfig.routerSSID.length() > 0 && !wifiConfig.isConnected) {
            unsigned long now = millis();

            if (wasConnected) {
              unsigned long timeSinceLastAttempt = now - lastConnectAttempt;

              if (timeSinceLastAttempt < RECONNECT_INTERVAL) {
                unsigned long remainingWait = RECONNECT_INTERVAL - timeSinceLastAttempt;

                bool inCooldown = (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS);

                static unsigned long lastDebug = 0;
                if (now - lastDebug > 5000) {
                  if (inCooldown) {
                    Serial.printf("COOLDOWN: %lu seconds remaining...\n",
                                  remainingWait / 1000);
                  } else {
                    Serial.printf("Waiting %lu seconds before retry...\n",
                                  remainingWait / 1000);
                  }
                  lastDebug = now;
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
              }

              if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
                Serial.println("\n========================================");
                Serial.println("COOLDOWN COMPLETED");
                Serial.println("========================================");
                Serial.println("Resetting reconnect counter...");
                reconnectAttempts = 0;
                Serial.println("Ready for fresh reconnect attempts");
                Serial.println("========================================\n");
              }

              Serial.println("Reconnect interval passed, attempting connection...\n");
            }

            if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
              Serial.println("========================================");
              Serial.println("WiFi: Attempting to connect...");
              Serial.println("========================================");
              Serial.println("SSID: " + wifiConfig.routerSSID);

              WiFi.scanDelete();
              delay(100);

              WiFi.disconnect(false);

              wifi_mode_t currentMode;
              esp_wifi_get_mode(&currentMode);

              if (currentMode != WIFI_MODE_APSTA) {
                Serial.println("Mode bukan AP_STA, forcing restore...");
                WiFi.mode(WIFI_AP_STA);
                delay(100);

                if (WiFi.softAPgetStationNum() == 0 && WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
                  WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                  delay(100);
                  Serial.println("AP restarted: " + String(wifiConfig.apSSID));
                }
              }

              WiFi.setHostname("JWS-Indonesia");
              delay(100);
              Serial.print("Hostname: ");
              Serial.println(WiFi.getHostname());

              WiFi.setTxPower(WIFI_POWER_19_5dBm);

              WiFi.begin(wifiConfig.routerSSID.c_str(), wifiConfig.routerPassword.c_str());

              wifiState = WIFI_CONNECTING;
              connectAttempt = 0;
              lastConnectAttempt = millis();
              reconnectAttempts++;

              Serial.printf("Connecting... (reconnect attempt %d/%d)\n",
                            reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
              Serial.println("========================================\n");

              xSemaphoreGive(wifiMutex);
            }
          }

          vTaskDelay(pdMS_TO_TICKS(500));
          break;
        }

      case WIFI_CONNECTING:
        {
          esp_task_wdt_reset();

          if (WiFi.status() == WL_CONNECTED) {
            if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
              wifiConfig.isConnected = true;
              wifiConfig.localIP = WiFi.localIP();
              wifiState = WIFI_CONNECTED;
              wasConnected = true;

              reconnectAttempts = 0;
              connectAttempt = 0;

              WiFi.scanDelete();

              autoUpdateDone = false;

              unsigned long reconnectTime = (millis() - wifiDisconnectedTime) / 1000;

              Serial.println("\n========================================");
              Serial.println("WiFi Connected Successfully!");
              Serial.println("========================================");
              Serial.println("SSID: " + String(WiFi.SSID()));
              Serial.println("IP: " + wifiConfig.localIP.toString());
              Serial.println("Gateway: " + WiFi.gatewayIP().toString());
              Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");

              if (wifiDisconnectedTime > 0) {
                Serial.printf("Reconnect time: %lu seconds\n", reconnectTime);
              }

              int rssi = WiFi.RSSI();
              String quality = "Unknown";
              if (rssi >= -50) quality = "Excellent";
              else if (rssi >= -60) quality = "Good";
              else if (rssi >= -70) quality = "Fair";
              else quality = "Weak";
              Serial.println("Signal: " + quality);

              Serial.println("========================================\n");

              xSemaphoreGive(wifiMutex);

              if (ntpTaskHandle != NULL) {
                Serial.println("Auto-triggering NTP sync...");
                ntpSyncInProgress = false;
                ntpSyncCompleted = false;
                xTaskNotifyGive(ntpTaskHandle);
                Serial.println("Waiting for NTP sync to complete...\n");
              }
            }
          } else {
            connectAttempt++;

            if (connectAttempt % 5 == 0) {
              wl_status_t status = WiFi.status();
              String statusStr = "Unknown";

              switch (status) {
                case WL_IDLE_STATUS: statusStr = "Idle"; break;
                case WL_NO_SSID_AVAIL: statusStr = "SSID Not Found"; break;
                case WL_SCAN_COMPLETED: statusStr = "Scan Complete"; break;
                case WL_CONNECTED: statusStr = "Connected"; break;
                case WL_CONNECT_FAILED: statusStr = "Failed"; break;
                case WL_CONNECTION_LOST: statusStr = "Connection Lost"; break;
                case WL_DISCONNECTED: statusStr = "Disconnected"; break;
              }

              Serial.printf("Connecting... %d/%d (%s)\n",
                            connectAttempt, MAX_CONNECT_ATTEMPTS, statusStr.c_str());
            }

            if (connectAttempt >= MAX_CONNECT_ATTEMPTS) {
              Serial.println("\n========================================");
              Serial.println("WiFi Connection Timeout");
              Serial.println("========================================");
              Serial.printf("Status: %d\n", WiFi.status());
              Serial.printf("Reconnect attempt: %d/%d\n", reconnectAttempts, MAX_RECONNECT_ATTEMPTS);

              wifiState = WIFI_FAILED;

              Serial.println("Disconnecting WiFi safely...");
              WiFi.disconnect(false);
              delay(200);

              Serial.println("Forcing AP_STA mode...");
              WiFi.mode(WIFI_AP_STA);
              delay(200);

              wifi_mode_t currentMode;
              esp_wifi_get_mode(&currentMode);
              Serial.printf("Current mode: %d (expected %d)\n", currentMode, WIFI_MODE_APSTA);

              IPAddress apIP = WiFi.softAPIP();
              String apSSID = WiFi.softAPSSID();

              Serial.println("Checking AP status...");
              Serial.println("AP IP: " + apIP.toString());
              Serial.println("AP SSID: " + apSSID);

              if (apIP == IPAddress(0, 0, 0, 0) || apSSID.length() == 0) {
                Serial.println("AP died during timeout! Restarting...");
                WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                delay(200);
                Serial.println("AP restarted:");
                Serial.println("SSID: " + WiFi.softAPSSID());
                Serial.println("IP: " + WiFi.softAPIP().toString());
              } else {
                Serial.println("AP still alive");
              }

              Serial.println("========================================\n");
            }
          }

          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }

      case WIFI_CONNECTED:
        {
          if (!autoUpdateDone && wifiConfig.isConnected) {

            if (!ntpSyncInProgress && !ntpSyncCompleted) {
              vTaskDelay(pdMS_TO_TICKS(500));
              break;
            }

            if (ntpSyncInProgress) {
              static int ntpWaitCounter = 0;
              ntpWaitCounter++;

              if (ntpWaitCounter % 10 == 0) {
                Serial.println("Waiting for NTP sync to complete...");
              }

              vTaskDelay(pdMS_TO_TICKS(500));

              if (ntpWaitCounter > 60) {
                Serial.println("NTP sync timeout - proceeding anyway");
                ntpSyncInProgress = false;
                ntpWaitCounter = 0;
              }
              break;
            }

            if (ntpSyncCompleted || timeConfig.ntpSynced) {
              Serial.println("\n========================================");
              Serial.println("NTP SYNC COMPLETED - UPDATING PRAYER TIMES");
              Serial.println("========================================");

              vTaskDelay(pdMS_TO_TICKS(1000));

              if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {

                char timeStr[20], dateStr[20];
                sprintf(timeStr, "%02d:%02d:%02d",
                        hour(timeConfig.currentTime),
                        minute(timeConfig.currentTime),
                        second(timeConfig.currentTime));
                sprintf(dateStr, "%02d/%02d/%04d",
                        day(timeConfig.currentTime),
                        month(timeConfig.currentTime),
                        year(timeConfig.currentTime));

                Serial.println("Current System Time: " + String(timeStr));
                Serial.println("Current Date: " + String(dateStr));
                Serial.println("City: " + prayerConfig.selectedCity);
                Serial.println("Coordinates: " + prayerConfig.latitude + ", " + prayerConfig.longitude);
                Serial.println("");

                getPrayerTimesByCoordinates(
                  prayerConfig.latitude,
                  prayerConfig.longitude);

                Serial.println("Prayer times update completed");
                Serial.println("========================================\n");

              } else {
                Serial.println("\n========================================");
                Serial.println("PRAYER TIMES AUTO-UPDATE SKIPPED");
                Serial.println("========================================");
                Serial.println("Reason: No city coordinates available");
                Serial.println("Action: Please select city via web interface");
                Serial.println("========================================\n");
              }

              autoUpdateDone = true;

            } else {
              Serial.println("\n========================================");
              Serial.println("NTP SYNC FAILED");
              Serial.println("========================================");
              Serial.println("Proceeding with prayer times update anyway");
              Serial.println("Warning: Time may be inaccurate!");
              Serial.println("========================================\n");

              vTaskDelay(pdMS_TO_TICKS(1000));

              if (prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0) {
                getPrayerTimesByCoordinates(
                  prayerConfig.latitude,
                  prayerConfig.longitude);
              }

              autoUpdateDone = true;
            }
          }

          unsigned long now = millis();

          if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
            lastWiFiCheck = now;

            wl_status_t status = WiFi.status();

            static unsigned long lastStatusPrint = 0;
            if (now - lastStatusPrint > 60000) {
              Serial.printf("WiFi: Connected | RSSI: %d dBm | IP: %s | Uptime: %lu min\n",
                            WiFi.RSSI(),
                            wifiConfig.localIP.toString().c_str(),
                            (now - wifiDisconnectedTime) / 60000);
              lastStatusPrint = now;
            }

            if (status != WL_CONNECTED) {
              Serial.println("\n========================================");
              Serial.println("WiFi DISCONNECTED DETECTED!");
              Serial.println("========================================");
              Serial.printf("Status code: %d\n", status);
              Serial.print("Reason: ");

              switch (status) {
                case WL_NO_SSID_AVAIL:
                  Serial.println("SSID not available (router offline?)");
                  break;
                case WL_CONNECT_FAILED:
                  Serial.println("Connection failed");
                  break;
                case WL_CONNECTION_LOST:
                  Serial.println("Connection lost");
                  break;
                case WL_DISCONNECTED:
                  Serial.println("Disconnected");
                  break;
                default:
                  Serial.println("Unknown");
              }

              if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE) {
                wifiConfig.isConnected = false;
                wifiState = WIFI_IDLE;

                autoUpdateDone = false;
                ntpSyncInProgress = false;
                ntpSyncCompleted = false;

                wasConnected = true;
                wifiDisconnectedTime = millis();

                // ============================================
                //  PASTIKAN AP TETAP HIDUP SAAT DISCONNECT
                // ============================================
                Serial.println("Checking AP status...");

                IPAddress apIP = WiFi.softAPIP();
                String apSSID = WiFi.softAPSSID();

                if (apIP == IPAddress(0, 0, 0, 0) || apSSID.length() == 0) {
                  Serial.println("AP died during disconnect! Restarting...");

                  WiFi.mode(WIFI_AP_STA);
                  delay(100);

                  WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
                  delay(200);

                  Serial.println("AP restarted: " + String(wifiConfig.apSSID));
                  Serial.println("AP IP: " + WiFi.softAPIP().toString());
                } else {
                  Serial.println("AP still alive: " + apSSID);
                  Serial.println("AP IP: " + apIP.toString());
                }

                Serial.println("Initiating fast reconnect mode...");
                Serial.println("Background scan every 3 seconds");
                Serial.println("Will connect immediately when router detected");
                Serial.printf("Next attempt in %d seconds\n", RECONNECT_INTERVAL / 1000);
                Serial.println("AP remains active: " + String(wifiConfig.apSSID));
                Serial.println("AP IP: " + WiFi.softAPIP().toString());
                Serial.println("========================================\n");

                WiFi.scanNetworks(true, false, false, 300);
                lastFastScan = millis();

                xSemaphoreGive(wifiMutex);
              }
            }
          }

          vTaskDelay(pdMS_TO_TICKS(500));
          break;
        }

      case WIFI_FAILED:
        {
          esp_task_wdt_reset();

          lastConnectAttempt = millis();

          Serial.println("\n========================================");
          Serial.println("Connection failed - entering retry mode");
          Serial.println("Fast scan continues - instant connect when available");
          Serial.println("========================================\n");

          wifiState = WIFI_IDLE;

          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
    }
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

        if (xSemaphoreTake(timeMutex, portMAX_DELAY) == pdTRUE) {
            bool syncSuccess = false;
            int serverIndex = 0;
            
            while (!syncSuccess && serverIndex < NTP_SERVER_COUNT) {
                esp_task_wdt_reset();
                
                Serial.printf("Trying NTP server: %s\n", ntpServers[serverIndex]);
                
                timeClient.setPoolServerName(ntpServers[serverIndex]);
                int offsetSeconds = timezoneOffset * 3600;
                timeClient.setTimeOffset(offsetSeconds);
                Serial.printf("   Using timezone: UTC%s%d (%d seconds)\n", 
                    timezoneOffset >= 0 ? "+" : "", timezoneOffset, offsetSeconds);
                timeClient.begin();
                
                unsigned long startTime = millis();
                bool updateResult = false;
                
                while (millis() - startTime < 5000) {
                    updateResult = timeClient.forceUpdate();
                    if (updateResult) break;
                    
                    esp_task_wdt_reset();
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                                
                if (updateResult) {
                    time_t ntpTime = timeClient.getEpochTime();
                    
                    timeConfig.currentTime = ntpTime;
                    setTime(timeConfig.currentTime);
                    timeConfig.ntpSynced = true;
                    syncSuccess = true;
                    timeConfig.ntpServer = String(ntpServers[serverIndex]);
                    
                    Serial.println("NTP Sync successful!");
                    Serial.printf("   Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                                hour(ntpTime), minute(ntpTime), second(ntpTime),
                                day(ntpTime), month(ntpTime), year(ntpTime));
                    
                    // ========================================
                    // UPDATE DISPLAY
                    // ========================================
                    DisplayUpdate update;
                    update.type = DisplayUpdate::TIME_UPDATE;
                    xQueueSend(displayQueue, &update, 0);
                    
                    xSemaphoreGive(timeMutex);
                    
                    // ========================================
                    // SAVE TO RTC HARDWARE
                    // ========================================
                    if (rtcAvailable) {
                        Serial.println("\nSaving time to RTC hardware...");
                        
                        saveTimeToRTC();
                        
                        delay(500);
                        
                        DateTime rtcNow = rtc.now();
                        Serial.println("RTC Verification:");
                        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                                     rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                                     rtcNow.day(), rtcNow.month(), rtcNow.year());
                        
                        // Validasi RTC tersimpan dengan benar
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
                    
                    xSemaphoreTake(timeMutex, portMAX_DELAY);
                    
                    // ========================================
                    // UPDATE PRAYER TIMES
                    // ========================================
                    Serial.println("\nPost-NTP: Checking prayer times...");
                    
                    if (prayerConfig.latitude.length() > 0 && 
                        prayerConfig.longitude.length() > 0) {
                        
                        Serial.println("   City configured: " + prayerConfig.selectedCity);
                        Serial.println("   Updating prayer times with correct date...");
                        
                        xSemaphoreGive(timeMutex);
                        
                        esp_task_wdt_reset();
                        
                        getPrayerTimesByCoordinates(
                            prayerConfig.latitude, 
                            prayerConfig.longitude
                        );
                        
                        Serial.println("Prayer times updated post-NTP sync");
                        
                        xSemaphoreTake(timeMutex, portMAX_DELAY);
                        
                    } else {
                        Serial.println("No city coordinates - skipping prayer update");
                        xSemaphoreGive(timeMutex);
                        xSemaphoreTake(timeMutex, portMAX_DELAY);
                    }
                    
                    Serial.println("========================================\n");
                    break;
                }
                
                serverIndex++;
                esp_task_wdt_reset();
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            
            if (!syncSuccess) {
                Serial.println("All NTP servers failed!");
                Serial.println("   Keeping current time");
                Serial.println("========================================\n");
            }
            
            ntpSyncInProgress = false;
            ntpSyncCompleted = syncSuccess;
            
            xSemaphoreGive(timeMutex);
            
        } else {
            Serial.println("Failed to acquire time mutex");
            Serial.println("========================================\n");
            
            ntpSyncInProgress = false;
            ntpSyncCompleted = false;
        }
        
        esp_task_wdt_reset();
    }
}

void printStackReport() {
  Serial.println("\nSTACK USAGE ANALYSIS");
  Serial.println("========================================");

  struct TaskInfo {
    TaskHandle_t handle;
    const char *name;
    uint32_t size;
  };

  TaskInfo tasks[] = {
    { uiTaskHandle, "UI", 16384 },
    { webTaskHandle, "Web", 16384 },
    { wifiTaskHandle, "WiFi", 8192 },
    { ntpTaskHandle, "NTP", 8192 },
    { prayerTaskHandle, "Prayer", 8192 },
    { rtcTaskHandle, "RTC", 4096 }
  };

  uint32_t totalAllocated = 0;
  uint32_t totalUsed = 0;

  for (int i = 0; i < 6; i++) {
    if (tasks[i].handle) {
      UBaseType_t hwm = uxTaskGetStackHighWaterMark(tasks[i].handle);
      uint32_t free = hwm;
      uint32_t used = tasks[i].size - free;
      float percent = (used * 100.0) / tasks[i].size;

      totalAllocated += tasks[i].size;
      totalUsed += used;

      Serial.printf("%-10s: %5d/%5d (%5.1f%%) ",
                    tasks[i].name, used, tasks[i].size, percent);

      if (percent < 50) Serial.println("BOROS - bisa dikurangi");
      else if (percent < 70) Serial.println("OPTIMAL");
      else if (percent < 85) Serial.println("PAS");
      else if (percent < 95) Serial.println("BAHAYA - harus dinaikkan!");
      else Serial.println("KRITIS - segera naikkan!");
    }
  }

  Serial.println("========================================");
  Serial.printf("Total Allocated: %d bytes (%.1f KB)\n",
                totalAllocated, totalAllocated / 1024.0);
  Serial.printf("Total Used:      %d bytes (%.1f KB)\n",
                totalUsed, totalUsed / 1024.0);
  Serial.printf("Efficiency:      %.1f%%\n",
                (totalUsed * 100.0) / totalAllocated);
  Serial.println("========================================\n");
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

  size_t initialHeap = ESP.getFreeHeap();
  size_t lowestHeap = initialHeap;

  while (true) {
    esp_task_wdt_reset();

    vTaskDelay(pdMS_TO_TICKS(5000));

    unsigned long now = millis();

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
        Serial.println("WARNING: Possible memory leak detected!");
        Serial.println("Consider restarting if memory continues dropping");
      } else if (currentHeap < 50000) {
        Serial.println("WARNING: Low memory! Consider restarting soon");
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
        Serial.println("\nCRITICAL: WiFi mode changed!");
        Serial.printf("Current mode: %d (should be %d)\n", mode, WIFI_MODE_APSTA);
        Serial.println("Forcing back to AP_STA...");

        WiFi.mode(WIFI_AP_STA);
        delay(100);

        WiFi.softAP(wifiConfig.apSSID, wifiConfig.apPassword);
        delay(100);

        Serial.println("AP restored: " + String(wifiConfig.apSSID));
      }
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

      bool shouldUpdate = (currentHour == 0 && currentMinute < 5 && !hasUpdatedToday && wifiConfig.isConnected && prayerConfig.latitude.length() > 0 && prayerConfig.longitude.length() > 0);

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
                
                // ================================
                // VALIDASI RTC TIME LEBIH KETAT
                // ================================
                bool rtcValid = (
                    rtcTime.year() >= 2000 && 
                    rtcTime.year() <= 2100 &&
                    rtcTime.month() >= 1 && 
                    rtcTime.month() <= 12 &&
                    rtcTime.day() >= 1 && 
                    rtcTime.day() <= 31 &&
                    rtcUnix >= 946684800
                );
                
                if (!rtcValid) {
                    Serial.println("\nRTC SYNC WARNING:");
                    Serial.printf("   RTC Time: %02d:%02d:%02d %02d/%02d/%04d\n",
                                 rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                 rtcTime.day(), rtcTime.month(), rtcTime.year());
                    Serial.printf("   RTC Unix: %ld\n", rtcUnix);
                    Serial.println("   Status: INVALID - keeping system time");
                    Serial.println("   RTC will be overwritten on next NTP sync\n");
                    
                    xSemaphoreGive(timeMutex);
                    vTaskDelayUntil(&xLastWakeTime, xFrequency);
                    continue;
                }
                
                // ================================
                // SYNC HANYA JIKA RTC LEBIH AKURAT
                // ================================
                if (abs(systemTime - rtcUnix) > 2) {
                    if (timeConfig.ntpSynced && systemTime > rtcUnix) {
                        Serial.println("\nRTC SYNC SKIPPED:");
                        Serial.println("   System time is newer (from NTP)");
                        Serial.printf("   System: %02d:%02d:%02d %02d/%02d/%04d\n",
                                     hour(systemTime), minute(systemTime), second(systemTime),
                                     day(systemTime), month(systemTime), year(systemTime));
                        Serial.printf("   RTC:    %02d:%02d:%02d %02d/%02d/%04d\n",
                                     rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                     rtcTime.day(), rtcTime.month(), rtcTime.year());
                        Serial.println("   RTC will be updated on next NTP sync\n");
                    } else {
                        timeConfig.currentTime = rtcUnix;
                        setTime(rtcUnix);
                        
                        Serial.println("\nSystem time synced from RTC");
                        Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                                     rtcTime.hour(), rtcTime.minute(), rtcTime.second(),
                                     rtcTime.day(), rtcTime.month(), rtcTime.year());
                        Serial.println();
                        
                        DisplayUpdate update;
                        update.type = DisplayUpdate::TIME_UPDATE;
                        xQueueSend(displayQueue, &update, 0);
                    }
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

  displayQueue = xQueueCreate(10, sizeof(DisplayUpdate));

  Serial.println("Semaphores & Queue created");

  // ================================
  // LITTLEFS & LOAD SETTINGS
  // ================================
  init_littlefs();
  loadWiFiCredentials();
  loadPrayerTimes();
  loadCitySelection();
  loadMethodSelection();
  loadTimezoneConfig();

  Wire.begin(/*RTC_SDA, RTC_SCL*/);
  delay(500);

  Wire.beginTransmission(0x68);
  Wire.endTransmission();

  // ================================
  // RTC DS3231 INIT
  // ================================
  rtcAvailable = initRTC();

  if (rtcAvailable) {
      Serial.println("\nSaving time to RTC hardware...");
      
      saveTimeToRTC();
      
      delay(500);
      
      DateTime rtcNow = rtc.now();
      Serial.println("RTC Verification:");
      Serial.printf("   RTC: %02d:%02d:%02d %02d/%02d/%04d\n",
                    rtcNow.hour(), rtcNow.minute(), rtcNow.second(),
                    rtcNow.day(), rtcNow.month(), rtcNow.year());
      
      // Validasi RTC tersimpan dengan benar
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
    xQueueSend(displayQueue, &update, 0);
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

  xTaskCreatePinnedToCore(
    clockTickTask,
    "Clock",
    4096,
    NULL,
    2,
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
      4096,
      NULL,
      1,
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
  Serial.println("SYSTEM READY!");
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

  Serial.println("\nBoot complete - Ready for connections!");
  Serial.println("========================================\n");
}

// ================================
// LOOP
// ================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}