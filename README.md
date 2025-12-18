# 🕌 ESP32 Islamic Prayer Clock

> Jam Waktu Sholat Digital dengan Auto-Update & Web Interface

![Version](https://img.shields.io/badge/version-2.1-blue) ![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green) ![Platform](https://img.shields.io/badge/platform-ESP32-red)

---

📸 Preview
<div align="center">
  <img src="https://github.com/user-attachments/assets/d0e64d2a-2a00-4a62-b92b-9aa36d95f4b8" height="250" alt="Home Screen">
  <img src="https://github.com/user-attachments/assets/55d84d62-d986-460a-b2b2-3ac4d2b4aaf1" height="250" alt="WiFi Settings">
  <img src="https://github.com/user-attachments/assets/b9f1db1c-83f2-4492-aef8-7b62997da9a9" height="250" alt="Time Sync">
  <img src="https://github.com/user-attachments/assets/84408e8e-ef1b-4ee5-aa42-9c0f965bb031" height="250" alt="City Selection">
  <img src="https://github.com/user-attachments/assets/6479dfd9-99b9-4034-a0d2-29453d6c46d9" height="250" alt="Coordinates Edit">
  <img src="https://github.com/user-attachments/assets/57c8726d-adf2-4ce2-bd92-2c5e05f66533" height="250" alt="Upload Cities">
  <img src="https://github.com/user-attachments/assets/1f105d28-80a6-490c-a4a9-d7f8174a3c3e" height="250" alt="Prayer Times">
</div>

## 📋 Daftar Isi

- [Fitur Utama](#-fitur-utama)
- [Hardware](#-hardware)
- [Instalasi](#-instalasi)
- [Setup Awal](#-setup-awal)
- [Web Interface](#-web-interface)
- [System Architecture](#-system-architecture)
- [Troubleshooting](#-troubleshooting)
- [API Endpoint](#-api-endpoint)

---

## ✨ Fitur Utama

| Fitur | Deskripsi |
|-------|-----------|
| 🕌 **Auto Prayer Times** | Update otomatis tengah malam via Aladhan API |
| ⏰ **NTP Sync** | Sinkronisasi jam setiap 1 jam (5 server fallback) |
| 🌐 **Web Interface** | Konfigurasi lengkap via browser |
| 📍 **500+ Cities** | Database kota Indonesia dengan GPS |
| 💾 **RTC DS3231** | Jam tetap akurat saat mati lampu |
| 🖥️ **LVGL 9.2.0** | UI touchscreen smooth & responsive |
| 🔄 **Auto WiFi Reconnect** | Fast reconnect dengan background scan |
| 🕋 **8 Calculation Methods** | Pilih metode: Kemenag, MWL, Egyptian, dll |
| 📍 **Manual GPS Edit** | Edit koordinat dengan reset ke default |
| 🌍 **Timezone Config** | Set UTC offset (-12 hingga +14) |

---

## 🔧 Hardware

### Board Utama
**ESP32-2432S024** (ESP32 + 2.4" TFT Touchscreen)
- MCU: ESP32 Dual-Core @ 240MHz
- Display: ILI9341 (320x240, 65K colors)
- Touch: XPT2046 (Resistive)
- WiFi: 802.11 b/g/n (2.4GHz)
- Power: 5V USB (min 2A)

⚠️ **PENTING**: 
- Power supply HARUS 5V 2A minimum
- Kabel USB berkualitas baik (bukan hanya charging)

### RTC DS3231 (Opsional, Sangat Direkomendasikan)

```
DS3231 Pin    →    ESP32-2432S024
VCC           →    3.3V
GND           →    GND
SDA           →    GPIO 21
SCL           →    GPIO 22
```

**Keuntungan RTC:**
- ✅ Jam tetap akurat saat mati lampu
- ✅ Auto-load time saat boot
- ✅ Auto-save time setiap NTP sync
- ✅ Battery backup: CR2032 (bertahun-tahun)

---

## 📦 Instalasi

### 1️⃣ Requirements

| Komponen | Versi | Keterangan |
|----------|-------|------------|
| **ESP32 Board** | **v3.0.7** | 🔴 WAJIB! (bukan v2.x) |
| **LVGL** | **9.2.0** | 🔴 WAJIB! (bukan v8.x/9.0.x) |
| Arduino IDE | 2.x+ | Atau PlatformIO |

### 2️⃣ Install ESP32 Board

**Arduino IDE:**
```
1. File → Preferences
2. Additional Boards Manager URLs:
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Tools → Board → Boards Manager
4. Install: esp32 v3.0.7
```

### 3️⃣ Install Libraries

Via **Library Manager** (Sketch → Include Library → Manage Libraries):

```
LVGL                    9.2.0   🔴 WAJIB VERSI INI
TFT_eSPI               2.5.0+
XPT2046_Touchscreen    1.4+
ArduinoJson            6.21.0+
ESPAsyncWebServer      1.2.3+
AsyncTCP               1.1.1+
TimeLib                1.6.1+
NTPClient              3.2.1+
RTClib                 2.1.1+
```

### 4️⃣ Clone & Upload

```bash
# Clone repository
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia

# Upload filesystem (data/ folder)
Tools → ESP32 Sketch Data Upload

# Upload code
Sketch → Upload
```

**Settings:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Size: 4MB
```

---

## 🚀 Setup Awal

### 1️⃣ First Boot
```
SSID: "JWS Indonesia"
Password: "12345678"
IP: 192.168.4.1
```

### 2️⃣ Connect & Configure
```
1. Cari WiFi "JWS Indonesia"
2. Password: 12345678
3. Browser: http://192.168.4.1
4. Masukkan SSID & Password WiFi rumah
5. Pilih kota (500+ pilihan)
6. Edit koordinat (opsional)
7. Pilih metode kalkulasi (default: Egyptian)
```

### 3️⃣ Konfigurasi Timezone
```
Default: UTC+7 (WIB)
- WIB (Jawa, Sumatera): +7
- WITA (Kalimantan, Sulawesi): +8
- WIT (Papua, Maluku): +9
```

---

## 🌐 Web Interface

**Access:**
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ESP32>  (cek serial monitor)
```

### Tab HOME
- 📊 Device status (WiFi, IP, NTP, RTC, Uptime)
- 🔄 Manual restart

### Tab WIRELESS
- 📡 WiFi SSID & Password
- 📶 Access Point settings

### Tab TIME SYNC
- 📱 Manual sync dari browser
- 🌐 Auto NTP sync (setiap 1 jam)
- 🕘 Timezone configuration (UTC -12 hingga +14)

### Tab CITY
- 🕌 Pilih lokasi (500+ kota)
- 📍 Edit koordinat GPS manual
- 🔄 Reset ke koordinat default
- 🕋 Pilih metode kalkulasi (8 pilihan)
- 📤 Upload cities.json (max 1MB)

### Tab PRAYER
- 🕌 Jadwal shalat (Imsak → Isya)
- 🔄 Auto-refresh tengah malam
- 📍 Berdasarkan lokasi terpilih

### Tab FACTORY
- ⚠️ Factory reset (hapus semua data)

---

## 📊 System Architecture

### Multi-Core FreeRTOS

| Task | Core | Priority | Function |
|------|------|----------|----------|
| **UI Task** | 1 | 3 | LVGL rendering @ 20 FPS |
| **WiFi Task** | 0 | 2 | Connection management + fast reconnect |
| **NTP Task** | 0 | 2 | Time synchronization |
| **Web Task** | 0 | 1 | Web server (5 concurrent) |
| **Prayer Task** | 0 | 1 | Midnight auto-update |
| **Clock Task** | 0 | 2 | Clock tick + hourly NTP |
| **RTC Sync** | 0 | 1 | RTC ↔ System time |

### Auto-Update System

**Midnight Prayer Update:**
```cpp
// Setiap hari 00:00-00:05
if (currentHour == 0 && currentMinute < 5) {
    getPrayerTimesByCoordinates(lat, lon);
}
```

**Hourly NTP Sync:**
```cpp
// Setiap 1 jam (3600 detik)
if (autoSyncCounter >= 3600) {
    xTaskNotifyGive(ntpTaskHandle);
    autoSyncCounter = 0;
}
```

**Timezone Auto-Apply:**
```cpp
// Saat timezone berubah:
// 1. Save to LittleFS
// 2. Update NTP offset
// 3. Trigger auto NTP re-sync
// 4. Apply ke RTC
```

### Configuration Storage (LittleFS)

```
/wifi_creds.txt       - WiFi credentials
/ap_creds.txt         - Access Point config
/prayer_times.txt     - Cached prayer times
/city_selection.txt   - City + coordinates
/method_selection.txt - Calculation method
/timezone.txt         - UTC offset (default: +7)
/cities.json          - Cities database (uploaded)
```

---

## 🔍 Troubleshooting

### Compile Errors

**`ledcAttach() not declared`**
```
Penyebab: ESP32 Board bukan v3.0.7
Solusi: Uninstall → Install ESP32 v3.0.7
```

**LVGL function errors**
```
Penyebab: LVGL bukan v9.2.0
Solusi: Uninstall → Install LVGL 9.2.0
```

### Upload Errors

**`Timed out waiting for packet header`**
```
Solusi:
1. Tekan BOOT saat upload
2. Upload Speed → 115200
3. Ganti kabel USB
4. Install driver CH340/CP2102
```

### Runtime Issues

**WiFi tidak connect**
```
Solusi:
1. Cek SSID/password (case-sensitive)
2. Router harus 2.4GHz (bukan 5GHz)
3. Restart device
4. Factory reset (hold RESET 3 detik)
```

**Prayer times tidak akurat**
```
Solusi:
1. Edit koordinat manual (Google Maps)
2. Ganti metode kalkulasi
3. Koordinat akurat = prayer times akurat (±1-2 menit)
```

**Jam 01/01/2000 setelah lama**
```
Solusi:
1. Normal jika belum NTP sync
2. Tunggu WiFi connect
3. Install RTC DS3231 (battery backup)
```

**Timezone tidak tersimpan**
```
Solusi:
1. Cek LittleFS mounted
2. Jangan factory reset setelah set
3. Verifikasi serial boot: "Timezone loaded: UTC+X"
```

### Advanced Debugging

**Enable Verbose:**
```cpp
#define DEBUG_WIFI
#define DEBUG_NTP
#define DEBUG_PRAYER
Serial.setDebugOutput(true);
```

**Monitor Memory:**
```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
```

---

## 🌐 API Endpoint

### 📡 GET `/api/data`

Endpoint untuk integrasi IoT - semua data dalam satu request.

**Response:**
```json
{
  "time": "14:35:22",
  "date": "13/12/2025",
  "day": "Saturday",
  "timestamp": 1734075322,
  "prayerTimes": {
    "imsak": "03:57",
    "subuh": "04:07",
    "sunrise": "05:32",
    "dzuhur": "11:47",
    "ashar": "15:14",
    "maghrib": "18:01",
    "isya": "19:17"
  },
  "location": {
    "city": "Jakarta",
    "latitude": "-6.175392",
    "longitude": "106.827153"
  },
  "method": {
    "id": 5,
    "name": "Egyptian General Authority of Survey"
  },
  "device": {
    "wifiConnected": true,
    "ntpSynced": true,
    "ntpServer": "pool.ntp.org",
    "freeHeap": 245632,
    "uptime": 3600
  },
  "timezone": 7
}
```

---

## 📁 File Structure

```
esp32-prayer-clock/
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
├── README.md
├── LICENSE
└── platformio.ini             # PlatformIO config
```

**Runtime Files (Auto-created):**
```
/wifi_creds.txt
/ap_creds.txt
/prayer_times.txt
/city_selection.txt
/method_selection.txt
/timezone.txt
```

---

## 🔐 Security

**Default Credentials:**
```
AP SSID: JWS Indonesia
AP Password: 12345678
```

**Recommendations:**
- ✅ Ganti AP password via web interface
- ✅ Gunakan strong WiFi password
- ✅ Akses hanya dari trusted network
- ❌ Jangan expose ke public internet

---
