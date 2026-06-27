/*
 * LED Matrix Controller dengan Web Interface
 * Format LEDGroup - Fully Synchronized
 * 
 * CARA UPLOAD:
 * 1. Upload data folder ke LittleFS menggunakan ESP8266 LittleFS Data Upload
 * 2. Upload sketch ini ke ESP8266
 * 
 * CARA AKSES:
 * 1. Koneksi ke WiFi: LED_Matrix_Config
 * 2. Password: 12345678
 * 3. Buka browser: http://192.168.4.1
 */

#include <FastLED.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// Konfigurasi Access Point
const char* ap_ssid = "LED_Matrix_Config";
const char* ap_password = "12345678";

// LED Configuration
#define LED_PIN     D4
#define RELAY_PIN   D1
#define NUM_LEDS 300
#define MAX_GROUPS 20
#define MAX_LEDS_PER_COLOR 50
#define BRIGHTNESS 50

CRGB leds[NUM_LEDS];
ESP8266WebServer server(80);

// LED Group Structure (sama seperti contoh)
struct LEDGroup {
  uint8_t count;
  uint16_t indices[MAX_LEDS_PER_COLOR];
  uint8_t r, g, b;
};

// Configuration Structure
struct Config {
  uint8_t rows;
  uint8_t cols;
  uint8_t ledsPerStrip;
  uint8_t brightness;
  bool zigzag;
  uint8_t startCorner;
  uint16_t numLeds;
  uint8_t numGroups;
  LEDGroup groups[MAX_GROUPS];
};

Config config;

// Load configuration from LittleFS
void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    if (file) {
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, file);
      file.close();
      
      if (!error) {
        config.rows = doc["rows"] | 1;
        config.cols = doc["cols"] | 1;
        config.ledsPerStrip = doc["ledsPerStrip"] | 2;
        config.brightness = doc["brightness"] | BRIGHTNESS;
        config.zigzag = doc["zigzag"] | true;
        config.startCorner = doc["startCorner"] | 0;
        config.numLeds = doc["numLeds"] | 2;
        
        // Load LED Groups
        JsonArray groups = doc["groups"];
        config.numGroups = groups.size();
        if (config.numGroups > MAX_GROUPS) config.numGroups = MAX_GROUPS;
        
        for (int g = 0; g < config.numGroups; g++) {
          JsonObject group = groups[g];
          config.groups[g].count = group["count"];
          config.groups[g].r = group["r"];
          config.groups[g].g = group["g"];
          config.groups[g].b = group["b"];
          
          JsonArray indices = group["indices"];
          int idxCount = indices.size();
          if (idxCount > MAX_LEDS_PER_COLOR) idxCount = MAX_LEDS_PER_COLOR;
          
          for (int i = 0; i < idxCount; i++) {
            config.groups[g].indices[i] = indices[i];
          }
        }
        
        Serial.println("Configuration loaded successfully!");
        Serial.printf("Groups loaded: %d\n", config.numGroups);
        return;
      }
    }
  }
  
  // Default configuration
  Serial.println("Using default configuration");
  config.rows = 1;
  config.cols = 1;
  config.ledsPerStrip = 2;
  config.brightness = BRIGHTNESS;
  config.zigzag = true;
  config.startCorner = 0;
  config.numLeds = 2;
  config.numGroups = 0;
  
  saveConfig();
}

// Save configuration to LittleFS
void saveConfig() {
  DynamicJsonDocument doc(8192);
  
  doc["rows"] = config.rows;
  doc["cols"] = config.cols;
  doc["ledsPerStrip"] = config.ledsPerStrip;
  doc["brightness"] = config.brightness;
  doc["zigzag"] = config.zigzag;
  doc["startCorner"] = config.startCorner;
  doc["numLeds"] = config.numLeds;
  
  // Save LED Groups
  JsonArray groups = doc.createNestedArray("groups");
  for (int g = 0; g < config.numGroups; g++) {
    JsonObject group = groups.createNestedObject();
    group["count"] = config.groups[g].count;
    group["r"] = config.groups[g].r;
    group["g"] = config.groups[g].g;
    group["b"] = config.groups[g].b;
    
    JsonArray indices = group.createNestedArray("indices");
    for (int i = 0; i < config.groups[g].count; i++) {
      indices.add(config.groups[g].indices[i]);
    }
  }
  
  File file = LittleFS.open("/config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Configuration saved!");
  } else {
    Serial.println("Failed to save configuration!");
  }
}

// Apply LED configuration
void applyLEDConfig() {
  FastLED.clear();
  FastLED.setBrightness(config.brightness);
  
  // Apply groups (sama seperti contoh loop)
  for (uint8_t g = 0; g < config.numGroups; g++) {
    for (uint8_t i = 0; i < config.groups[g].count; i++) {
      uint16_t ledIndex = config.groups[g].indices[i];
      if (ledIndex < config.numLeds) {
        leds[ledIndex] = CRGB(config.groups[g].r, config.groups[g].g, config.groups[g].b);
      }
    }
  }
  
  FastLED.show();
  Serial.printf("Applied %d LED groups\n", config.numGroups);
}

// Serve files from LittleFS
void handleFileRequest() {
  String path = server.uri();
  if (path.endsWith("/")) path += "index.html";
  
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".json")) contentType = "application/json";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".jpg")) contentType = "image/jpeg";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

// API: Get current configuration
void handleGetConfig() {
  DynamicJsonDocument doc(8192);
  
  doc["rows"] = config.rows;
  doc["cols"] = config.cols;
  doc["ledsPerStrip"] = config.ledsPerStrip;
  doc["brightness"] = config.brightness;
  doc["zigzag"] = config.zigzag;
  doc["startCorner"] = config.startCorner;
  doc["numLeds"] = config.numLeds;
  
  JsonArray groups = doc.createNestedArray("groups");
  for (int g = 0; g < config.numGroups; g++) {
    JsonObject group = groups.createNestedObject();
    group["count"] = config.groups[g].count;
    group["r"] = config.groups[g].r;
    group["g"] = config.groups[g].g;
    group["b"] = config.groups[g].b;
    
    JsonArray indices = group.createNestedArray("indices");
    for (int i = 0; i < config.groups[g].count; i++) {
      indices.add(config.groups[g].indices[i]);
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// API: Save configuration
void handleSave() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      config.rows = doc["rows"];
      config.cols = doc["cols"];
      config.ledsPerStrip = doc["ledsPerStrip"];
      config.brightness = doc["brightness"];
      config.zigzag = doc["zigzag"];
      config.startCorner = doc["startCorner"];
      config.numLeds = doc["numLeds"];
      
      // Load groups
      JsonArray groups = doc["groups"];
      config.numGroups = groups.size();
      if (config.numGroups > MAX_GROUPS) config.numGroups = MAX_GROUPS;
      
      for (int g = 0; g < config.numGroups; g++) {
        JsonObject group = groups[g];
        config.groups[g].count = group["count"];
        config.groups[g].r = group["r"];
        config.groups[g].g = group["g"];
        config.groups[g].b = group["b"];
        
        JsonArray indices = group["indices"];
        int idxCount = indices.size();
        if (idxCount > MAX_LEDS_PER_COLOR) idxCount = MAX_LEDS_PER_COLOR;
        
        for (int i = 0; i < idxCount; i++) {
          config.groups[g].indices[i] = indices[i];
        }
      }
      
      saveConfig();
      applyLEDConfig();
      
      server.send(200, "application/json", "{\"success\":true}");
      Serial.println("Configuration updated via web!");
    } else {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"No data\"}");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================");
  Serial.println("LED Matrix Controller Starting...");
  Serial.println("=================================");
  
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Serial.println("LittleFS Mounted");
  
  // List files
  Serial.println("\nFiles in LittleFS:");
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    Serial.print("  ");
    Serial.print(dir.fileName());
    Serial.print(" - ");
    Serial.print(dir.fileSize());
    Serial.println(" bytes");
  }

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  delay(5000);
  digitalWrite(RELAY_PIN, HIGH);
  
  // Initialize FastLED
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  Serial.println("FastLED Initialized");

  FastLED.clear();
  delay(500);
  FastLED.show();
  delay(500);
  
  // Load configuration
  loadConfig();
  applyLEDConfig();
  
  // Setup Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("\nAP IP address: ");
  Serial.println(IP);
  
  // Setup web server routes
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/save", HTTP_POST, handleSave);
  server.onNotFound(handleFileRequest);
  
  server.begin();
  Serial.println("Web Server Started");
  Serial.println("\n=================================");
  Serial.println("Koneksi ke WiFi: " + String(ap_ssid));
  Serial.println("Password: " + String(ap_password));
  Serial.println("Buka browser: http://192.168.4.1");
  Serial.println("=================================\n");
}

void loop() {
  server.handleClient();
  
  // Keep LED updated (sama seperti contoh)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 50) {
    FastLED.clear();
    for (uint8_t g = 0; g < config.numGroups; g++) {
      for (uint8_t i = 0; i < config.groups[g].count; i++) {
        uint16_t ledIndex = config.groups[g].indices[i];
        if (ledIndex < config.numLeds) {
          leds[ledIndex] = CRGB(config.groups[g].r, config.groups[g].g, config.groups[g].b);
        }
      }
    }
    FastLED.show();
    lastUpdate = millis();
  }
}