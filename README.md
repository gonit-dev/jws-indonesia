# 🕌 ESP32 Islamic Prayer Clock

> Jam Waktu Sholat Digital Otomatis dengan Web Interface & RTC Backup

![Version](https://img.shields.io/badge/version-2.1-blue) ![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green) ![Platform](https://img.shields.io/badge/platform-ESP32-red) ![License](https://img.shields.io/badge/license-MIT-yellow)

---

## 📸 Preview
<div align="center">
  <img src="https://github.com/user-attachments/assets/d0e64d2a-2a00-4a62-b92b-9aa36d95f4b8" height="250" alt="Home Screen">
  <img src="https://github.com/user-attachments/assets/55d84d62-d986-460a-b2b2-3ac4d2b4aaf1" height="250" alt="WiFi Settings">
  <img src="https://github.com/user-attachments/assets/b9f1db1c-83f2-4492-aef8-7b62997da9a9" height="250" alt="Time Sync">
  <img src="https://github.com/user-attachments/assets/84408e8e-ef1b-4ee5-aa42-9c0f965bb031" height="250" alt="City Selection">
  <img src="https://github.com/user-attachments/assets/6479dfd9-99b9-4034-a0d2-29453d6c46d9" height="250" alt="Coordinates Edit">
  <img src="https://github.com/user-attachments/assets/57c8726d-adf2-4ce2-bd92-2c5e05f66533" height="250" alt="Upload Cities">
  <img src="https://github.com/user-attachments/assets/1f105d28-80a6-490c-a4a9-d7f8174a3c3e" height="250" alt="Prayer Times">
</div>

---

## ✨ Fitur Utama

| Fitur | Deskripsi |
|-------|-----------|
| 🕌 **Auto Prayer Times** | Update otomatis tengah malam via Aladhan API |
| ⏰ **NTP Time Sync** | Sinkronisasi jam otomatis setiap 1 jam |
| 🌐 **Web Interface** | Konfigurasi lengkap via browser (responsive) |
| 📍 **500+ Cities** | Database kota Indonesia dengan koordinat GPS |
| 💾 **RTC DS3231** | Jam tetap akurat saat mati lampu (battery backup) |
| 🖥️ **LVGL 9.2.0** | UI touchscreen smooth 20 FPS |
| 🔄 **Event-Driven WiFi** | Auto-reconnect cepat tanpa polling |
| 🕋 **8 Methods** | Pilih metode: Kemenag, MWL, Egyptian, dll |
| 📍 **Manual GPS Edit** | Edit koordinat manual dengan reset default |
| 🌍 **Timezone Config** | Set UTC offset (-12 hingga +14) |

---

## 🔧 Hardware Requirements

### Board Utama: ESP32-2432S024
- **MCU:** ESP32 Dual-Core @ 240MHz
- **Display:** ILI9341 2.4" TFT (320x240, 65K colors)
- **Touch:** XPT2046 Resistive
- **WiFi:** 802.11 b/g/n (2.4GHz)
- **Power:** 5V USB (minimum 2A)

⚠️ **Penting:** Gunakan power supply 5V 2A minimum & kabel USB berkualitas baik!

### RTC DS3231 (Opsional, Sangat Direkomendasikan)

```
DS3231       ESP32-2432S024
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21
SCL     →    GPIO 22
```

**Keuntungan RTC:**
- ✅ Jam tetap akurat saat mati lampu
- ✅ Auto-load time saat boot
- ✅ Auto-save setiap NTP sync
- ✅ Battery CR2032 (bertahun-tahun)

---

## 📦 Instalasi

### 1️⃣ Requirements

| Komponen | Versi | Wajib? |
|----------|-------|--------|
| **ESP32 Board** | **v3.0.7** | 🔴 Ya |
| **LVGL** | **9.2.0** | 🔴 Ya |
| Arduino IDE | 2.x+ | Atau PlatformIO |

### 2️⃣ Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Install: esp32 v3.0.7
```

### 3️⃣ Install Libraries

Via **Library Manager** (Sketch → Include Library → Manage Libraries):

```
✅ LVGL                   9.2.0
✅ TFT_eSPI               2.5.0+
✅ XPT2046_Touchscreen    1.4+
✅ ArduinoJson            6.21.0+
✅ ESPAsyncWebServer      1.2.3+
✅ AsyncTCP               1.1.1+
✅ TimeLib                1.6.1+
✅ RTClib                 2.1.1+
```

### 4️⃣ Upload

```bash
# Clone repository
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia

# Upload filesystem (data/ folder)
Tools → ESP32 Sketch Data Upload

# Upload code
Sketch → Upload (atau Ctrl+U)
```

**Board Settings:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
```

---

## 🚀 Quick Start

### Step 1: First Boot
```
📶 SSID: "JWS Indonesia"
🔐 Password: "12345678"
🌐 IP: 192.168.4.1
```

### Step 2: Konfigurasi WiFi
1. Hubungkan ke WiFi **"JWS Indonesia"**
2. Buka browser → `http://192.168.4.1`
3. Tab **WIRELESS** → Masukkan SSID & Password WiFi rumah
4. Klik **Simpan** → Tunggu auto-connect (~15 detik)

### Step 3: Set Lokasi
1. Tab **CITY** → Pilih kota dari dropdown (500+ pilihan)
2. *Optional:* Edit koordinat GPS manual
3. Klik **Simpan** → Jadwal shalat auto-update

### Step 4: Timezone (Opsional)
```
Default: UTC+7 (WIB)
- WIB: +7  (Jawa, Sumatera)
- WITA: +8 (Kalimantan, Sulawesi)
- WIT: +9  (Papua, Maluku)
```

Tab **TIME SYNC** → Edit timezone → Klik **💾 Simpan**

---

## 🌐 Web Interface Guide

**Access Points:**
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ESP32>  (cek serial monitor)
```

### 📑 Tab Overview

#### 🏠 HOME
- Device status (WiFi, IP, NTP, Uptime)
- Real-time clock display
- Manual restart button

#### 📡 WIRELESS
- **WiFi Settings:** SSID & Password WiFi rumah
- **AP Settings:** Custom SSID & Password Access Point

#### ⏰ TIME SYNC
- **Manual Sync:** Sync waktu dari browser
- **Auto NTP:** Otomatis sync setiap 1 jam
- **Timezone:** Set UTC offset (-12 hingga +14)

#### 🕌 CITY
- **Pilih Lokasi:** 500+ kota Indonesia
- **Edit GPS:** Manual coordinates dengan reset default
- **Metode Kalkulasi:** 8 pilihan (Kemenag, MWL, Egyptian, dll)
- **Upload JSON:** Upload cities.json baru (max 1MB)

#### 🙏 PRAYER
- Jadwal shalat lengkap (Imsak → Isya)
- Auto-refresh tengah malam
- Display metode kalkulasi aktif

#### ⚠️ FACTORY
- Factory reset (hapus semua data)
- Device auto-restart

---

## 🏗️ System Architecture

### Multi-Core FreeRTOS Tasks

| Task | Core | Priority | Stack | Function |
|------|------|----------|-------|----------|
| **UI Task** | 1 | 3 (High) | 12KB | LVGL rendering @ 20 FPS |
| **WiFi Task** | 0 | 2 (High) | 4KB | Event-driven connection |
| **NTP Task** | 0 | 2 (High) | 6KB | Time sync (5 server fallback) |
| **Web Task** | 0 | 1 (Low) | 6KB | AsyncWebServer (5 clients) |
| **Prayer Task** | 0 | 1 (Low) | 6KB | Midnight auto-update |
| **Clock Task** | 0 | 2 (High) | 2KB | 1-second tick + hourly NTP |
| **RTC Sync** | 0 | 1 (Low) | 2KB | RTC ↔ System time sync |

### Auto-Update System

**Midnight Prayer Update:**
```cpp
// Setiap hari jam 00:00-00:05
if (hour == 0 && minute < 5 && !hasUpdatedToday) {
    triggerNTPSync();        // Step 1: Sync waktu dulu
    waitNTPComplete();       // Step 2: Tunggu selesai
    updatePrayerTimes();     // Step 3: Update jadwal
}
```

**Hourly NTP Sync:**
```cpp
// Setiap 1 jam (3600 detik)
if (++autoSyncCounter >= 3600) {
    triggerNTPSync();
    autoSyncCounter = 0;
}
```

**Timezone Auto-Apply:**
- Save ke LittleFS → Trigger NTP re-sync → Update prayer times

---

## 📁 File Structure

```
jws-indonesia/
├── jws.ino                    # Main program (8000+ lines)
├── src/                       # EEZ Studio UI
│   ├── ui.h/cpp
│   ├── screens.h
│   ├── images.h
│   └── fonts.h
├── data/                      # LittleFS (upload ke ESP32)
│   ├── index.html             # Web interface
│   ├── assets/
│   │   ├── css/foundation.min.css
│   │   └── js/jquery.min.js
│   └── cities.json            # 500+ cities database
└── README.md
```

**Runtime Files (Auto-created di LittleFS):**
```
/wifi_creds.txt       → WiFi credentials
/ap_creds.txt         → Access Point config
/prayer_times.txt     → Cached prayer times
/city_selection.txt   → City + GPS coordinates
/method_selection.txt → Calculation method
/timezone.txt         → UTC offset
```

---

## 🔍 Troubleshooting

### ❌ Compile Errors

**Error:** `ledcAttach() not declared`
```
Penyebab: ESP32 Board bukan v3.0.7
Solusi: Uninstall ESP32 → Install v3.0.7
```

**Error:** LVGL function errors
```
Penyebab: LVGL bukan v9.2.0
Solusi: Uninstall LVGL → Install v9.2.0
```

### 📤 Upload Errors

**Error:** `Timed out waiting for packet header`
```
Solusi:
1. Tekan & tahan tombol BOOT saat upload
2. Kurangi Upload Speed → 115200
3. Ganti kabel USB (data + power)
4. Install driver CH340/CP2102
```

### 🌐 Runtime Issues

**WiFi tidak connect**
```
✅ Cek SSID/password (case-sensitive)
✅ Router harus 2.4GHz (bukan 5GHz)
✅ Restart device
✅ Factory reset via web interface
```

**Prayer times tidak akurat (selisih >5 menit)**
```
✅ Edit koordinat GPS manual (Google Maps)
✅ Ganti metode kalkulasi (8 pilihan)
✅ Koordinat akurat = waktu akurat
```

**Jam 01/01/2000 setelah mati lampu lama**
```
✅ Normal jika belum NTP sync
✅ Tunggu WiFi connect
✅ Install RTC DS3231 (battery backup)
```

**Timezone tidak tersimpan**
```
✅ Cek LittleFS mounted
✅ Jangan factory reset setelah set
✅ Verifikasi serial: "Timezone loaded: UTC+X"
```

---

## 🔐 Security

**Default Credentials:**
```
AP SSID:    JWS Indonesia
AP Password: 12345678
```

**Recommendations:**
- ✅ Ganti AP password via web interface
- ✅ Gunakan strong WiFi password
- ✅ Akses hanya dari trusted network
- ❌ Jangan expose ke public internet

---

## 🌐 API Endpoint

### `/api/data` - IoT Integration

**GET Request:**
```bash
curl http://192.168.4.1/api/data
```

**Response:**
```json
{
  "time": "14:35:22",
  "date": "19/12/2024",
  "day": "Thursday",
  "timestamp": 1734598522,
  "prayerTimes": {
    "imsak": "03:57",
    "subuh": "04:07",
    "terbit": "05:32",
    "zuhur": "11:47",
    "ashar": "15:14",
    "maghrib": "18:01",
    "isya": "19:17"
  },
  "location": {
    "city": "Jakarta",
    "latitude": "-6.175392",
    "longitude": "106.827153"
  },
  "device": {
    "wifiConnected": true,
    "ntpSynced": true,
    "ntpServer": "pool.ntp.org",
    "freeHeap": 245632,
    "uptime": 3600
  }
}
```

---

## 🚀 Performance Optimizations

### Why So Fast?

1. **WiFi Sleep DOUBLE-DISABLED**
   ```cpp
   WiFi.setSleep(WIFI_PS_NONE);       // Arduino layer
   esp_wifi_set_ps(WIFI_PS_NONE);     // ESP-IDF layer
   ```
   - Response: <10ms (vs 100-500ms dulu)

2. **Event-Driven WiFi**
   ```cpp
   WiFi.onEvent([](WiFiEvent_t event) { ... });
   ```
   - No polling → CPU idle → more resources

3. **Pre-allocated Buffers**
   ```cpp
   char jsonBuffer[512];
   snprintf(jsonBuffer, sizeof(jsonBuffer), ...);
   ```
   - No malloc/free → super fast

4. **Browser Caching**
   ```cpp
   response->addHeader("Cache-Control", "public, max-age=3600");
   ```
   - Load CSS/JS once → instant reload

**Result:** Page load **200-500ms** (was 2-5 seconds!)

---

## 📊 Memory Usage

```
Task Stacks:      ~40KB
LVGL Buffer:      3.2KB
JSON Document:    Variable (cities.json)
Free Heap:        ~240KB (typical)
```

**Stack Analysis:**
```
UI Task:      OPTIMAL (60-70% usage)
Web Task:     OPTIMAL (50-60% usage)
WiFi Task:    OPTIMAL (40-50% usage)
NTP Task:     OPTIMAL (50-60% usage)
Prayer Task:  PAS (70-80% usage)
```
---

<div align="center">

**Made with ❤️ in Indonesia**

⭐ Star this repo if helpful!

</div>
