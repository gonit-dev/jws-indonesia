# 🕌 ESP32 Islamic Prayer Clock

> Jam Waktu Sholat Digital dengan Touchscreen, Auto-Update, dan Web Interface

![Version](https://img.shields.io/badge/version-2.1-blue)
![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)
![Platform](https://img.shields.io/badge/platform-ESP32-red)
![License](https://img.shields.io/badge/license-MIT-yellow)

## 📸 Preview

<img width="1366" height="1686" alt="FireShot Capture 094 - Islamic Prayer Clock Settings - 192 168 4 1" src="https://github.com/user-attachments/assets/3ae0b778-6e82-49fc-9834-0ee9a1704c75" />
<img alt="DISPLAY" src="https://github.com/user-attachments/assets/4a1ee20d-c013-42f2-b564-745606634ea7" />

---

## ✨ Fitur Utama

| Fitur | Deskripsi |
|-------|-----------|
| 🕌 **Auto Prayer Times** | Update otomatis setiap tengah malam (00:00-00:05) |
| ⏰ **NTP Sync** | Sinkronisasi jam otomatis setiap 1 jam dengan 5 server fallback |
| 🌐 **Web Interface** | Konfigurasi via browser - WiFi, City Selection, Manual Sync |
| 📍 **500+ Cities** | Pilih kota dari seluruh Indonesia dengan koordinat GPS akurat |
| 💾 **RTC DS3231** | Jam tetap akurat meski mati lampu (dengan baterai CR2032) |
| 🖥️ **LVGL 9.2.0** | UI touchscreen yang smooth & responsive |
| 📱 **AP Mode** | Setup awal tanpa perlu WiFi eksternal |
| 🔐 **Session Security** | Token-based session dengan auto-expire 15 menit |
| 📤 **Upload Cities** | Update data kota/kabupaten via web interface (max 1MB) |
| 🌐 **REST API** | Akses data via HTTP endpoint untuk integrasi IoT |
| 🔄 **Auto WiFi Reconnect** | Otomatis reconnect jika koneksi terputus |
| 📊 **Multi-Core FreeRTOS** | Task scheduling optimal di dual-core ESP32 |
| 🔧 **Custom Hostname** | Hostname mengikuti nama WiFi Access Point |

---

## 🔧 Hardware Requirements

### Board Utama
**ESP32-2432S024** (ESP32 + 2.4" TFT Touchscreen)
- **MCU**: ESP32 Dual-Core @ 240MHz
- **RAM**: 520KB SRAM
- **Flash**: 4MB
- **Display**: ILI9341 (320x240 pixels, 65K colors)
- **Touch**: XPT2046 (Resistive touch)
- **WiFi**: 802.11 b/g/n (2.4GHz)
- **Power**: 5V USB (min 2A)

### Modul Tambahan (Optional)

#### RTC DS3231 (Sangat Direkomendasikan!)
```
DS3231 ─────── ESP32-2432S024
VCC    ─────── 3.3V
GND    ─────── GND
SDA    ─────── GPIO 21
SCL    ─────── GPIO 22
```

**Keuntungan RTC:**
- ✅ Jam tetap akurat saat mati lampu
- ✅ Akurasi tinggi (±2 ppm drift)
- ✅ Kompensasi suhu otomatis
- ✅ Baterai backup CR2032 (opsional)
- ❌ Tanpa RTC: Jam reset ke 01/01/2000 00:00:00 setiap restart

---

## 📦 Instalasi

### ⚠️ Requirements (PENTING!)

| Komponen | Versi | Keterangan |
|----------|-------|------------|
| **ESP32 Board** | **v3.0.7** | WAJIB! Bukan v2.x (API berbeda) |
| **LVGL** | **9.2.0** | WAJIB! Bukan v8.x/9.0.x (breaking changes) |
| **Arduino IDE** | 2.x+ | Atau PlatformIO |

### 1️⃣ Install ESP32 Board v3.0.7

**Arduino IDE:**
```
1. File → Preferences
2. Additional Boards Manager URLs:
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Tools → Board → Boards Manager
4. Cari "esp32" → Install versi 3.0.7 (PENTING: Pilih 3.0.7!)
```

**PlatformIO (platform.ini):**
```ini
[env:esp32-2432s024]
platform = espressif32@6.9.0  ; ESP32 Core 3.0.7
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
```

---

### 2️⃣ Install Library Dependencies

| Library | Versi | Install via | Keterangan |
|---------|-------|-------------|------------|
| **LVGL** | **9.2.0** ⚠️ | Library Manager | HARUS versi 9.2.0! |
| TFT_eSPI | 2.5.0+ | Library Manager | Display driver |
| XPT2046_Touchscreen | 1.4+ | Library Manager | Touch driver |
| ArduinoJson | 6.21.0+ | Library Manager | JSON parser |
| ESPAsyncWebServer | 1.2.3+ | Library Manager | Web server |
| AsyncTCP | 1.1.1+ | Library Manager | Async networking |
| TimeLib | 1.6.1+ | Library Manager | Time functions |
| NTPClient | 3.2.1+ | Library Manager | NTP sync |
| RTClib | 2.1.1+ | Library Manager | RTC driver |

> **🚨 CRITICAL**: LVGL harus versi **9.2.0** (bukan latest/8.x/9.0.x!)

**Cara Install di Arduino IDE:**
```
1. Sketch → Include Library → Manage Libraries
2. Cari nama library
3. Pilih versi yang sesuai
4. Klik Install
```

---

### 3️⃣ Clone Repository

```bash
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia
```

---

### 4️⃣ Upload File System (LittleFS)

**Arduino IDE:**
```
1. Install ESP32 LittleFS Uploader:
   https://github.com/lorol/arduino-esp32littlefs-plugin
2. Tools → ESP32 Sketch Data Upload
3. Tunggu sampai selesai (upload data/ ke ESP32)
```

**PlatformIO:**
```bash
pio run --target uploadfs
```

**File yang diupload:**
- `/data/index.html` - Web interface
- `/data/assets/css/foundation.css` - Stylesheet
- `/data/cities.json` - Database 500+ kota

---

### 5️⃣ Upload Code

**Arduino IDE:**
```
1. Tools → Board → ESP32 Dev Module
2. Tools → Upload Speed → 921600
3. Tools → Flash Size → 4MB
4. Sketch → Upload
```

**PlatformIO:**
```bash
pio run --target upload
```

---

### 6️⃣ Monitor Serial Output

```bash
# Arduino IDE: Tools → Serial Monitor (115200 baud)

# PlatformIO:
pio device monitor -b 115200
```

**Expected Output:**
```
========================================
   ESP32 Islamic Prayer Clock
   LVGL 9.2.0 + FreeRTOS
   VERSION 2.1 - MULTI-CLIENT
========================================

Backlight: OFF
TFT initialized
Touch initialized
LVGL initialized
LittleFS Mounted
WiFi credentials loaded
✅ AP Started: JWS ESP32
   Password: 12345678
   AP IP: 192.168.4.1
✅ Hostname Set: JWS ESP32
========================================
SYSTEM READY!
========================================
```

---

## 🚀 Setup Awal

### 1️⃣ First Boot
```
Device membuat Access Point:
   SSID: "JWS ESP32"
   Password: "12345678"
   IP: 192.168.4.1
```

### 2️⃣ Hubungkan ke AP
```
1. Cari WiFi "JWS ESP32" di HP/Laptop
2. Password: 12345678
3. Buka browser: http://192.168.4.1
```

### 3️⃣ Konfigurasi WiFi
```
1. Masukkan SSID & Password WiFi rumah
2. Klik "Simpan & Restart"
3. Device restart dan connect ke WiFi
4. Catat IP baru dari serial monitor
```

### 4️⃣ Pilih Kota
```
1. Buka web interface (IP baru)
2. Pilih kota dari dropdown (500+ kota Indonesia)
3. Prayer times auto-update!
```

---

## 🌐 Web Interface

### URL Access
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ESP32>  (cek serial monitor)
```

### Fitur Available
- ✅ **Device Status**: WiFi, IP, NTP, RTC, Uptime, Free Heap
- ✅ **WiFi Configuration**: Ubah SSID & Password WiFi
- ✅ **AP Configuration**: Ubah nama & password Access Point
- ✅ **City Selection**: Dropdown 500+ kota Indonesia
- ✅ **Prayer Times Display**: Subuh, Dzuhur, Ashar, Maghrib, Isya
- ✅ **Manual Time Sync**: Sinkronisasi waktu dari browser
- ✅ **Upload Cities JSON**: Update database kota (max 1MB)
- ✅ **Factory Reset**: Reset semua pengaturan ke default
- ✅ **Real-time Clock**: Update setiap 1 detik
- ✅ **Auto-refresh Status**: Device status update setiap 5 detik

---

## ⏰ Auto-Update System

### 1️⃣ Midnight Prayer Update
```cpp
// Setiap hari jam 00:00-00:05
// Auto-fetch prayer times dari Aladhan API
if (currentHour == 0 && currentMinute < 5 && !hasUpdatedToday) {
    getPrayerTimesByCoordinates(latitude, longitude);
}
```

### 2️⃣ Hourly NTP Sync
```cpp
// Setiap 1 jam (3600 detik)
// Auto-sync dengan NTP server
if (autoSyncCounter >= 3600) {
    xTaskNotifyGive(ntpTaskHandle);
}
```

### 3️⃣ RTC Sync Task
```cpp
// Setiap 1 menit
// Sync system time ← RTC time (jika selisih > 2 detik)
// NTP sync → RTC (setiap NTP berhasil)
```

### 4️⃣ NTP Server Fallback
```
Prioritas server NTP:
1. pool.ntp.org
2. id.pool.ntp.org
3. time.google.com
4. time.cloudflare.com
5. time.windows.com
```

---

## 📊 System Architecture

### FreeRTOS Tasks (Multi-Core)

| Task | Core | Priority | Stack | Function |
|------|------|----------|-------|----------|
| **UI Task** | 1 | 3 | 16KB | LVGL rendering @ 20 FPS |
| **WiFi Task** | 0 | 2 | 8KB | WiFi connection management |
| **NTP Task** | 0 | 2 | 8KB | Time synchronization |
| **Web Task** | 0 | 1 | 16KB | Web server (5 concurrent sessions) |
| **Prayer Task** | 0 | 1 | 8KB | Midnight auto-update |
| **Clock Task** | 0 | 2 | 4KB | Clock tick + hourly NTP |
| **RTC Sync** | 0 | 1 | 4KB | RTC ↔ System time sync |

### Memory Protection (Mutexes)

```cpp
displayMutex    // Display operations (SPI conflicts)
timeMutex       // Time data access
wifiMutex       // WiFi status updates
settingsMutex   // Configuration read/write
spiMutex        // SPI bus arbitration (display + touch)
```

### Queue System
```cpp
displayQueue    // UI update requests (10 items)
```

---

## ⚙️ Perilaku Sistem

### 🕐 Time Management

**Default Time Behavior:**
- Jika **RTC tersedia & valid** (tahun 2000-2100) → Gunakan waktu dari RTC
- Jika **RTC tidak ada/rusak/invalid** → Reset ke 01/01/2000 00:00:00
- Waktu akan auto-update saat **NTP sync berhasil**
- NTP sync otomatis **menyimpan waktu ke RTC** untuk persistensi
- Ini mencegah timestamp invalid (bug epoch 1970)

**Time Sync Priority:**
```
1. RTC time (saat boot)
2. NTP sync (setiap 1 jam)
3. Manual sync (via web interface)
```

### 🔐 Security Features

**Session Management:**
- Token-based session (32 karakter random)
- Session expired: 15 menit (900 detik)
- Maksimal 5 session bersamaan
- Auto-redirect ke `/notfound` jika session invalid
- Session auto-refresh saat digunakan

**URL Protection:**
- Protected endpoints: Redirect ke `/notfound` jika no session
- Static assets: Return 404 plain text
- Random URLs: Redirect ke `/notfound`

### 💡 Display Configuration

**Backlight PWM:**
```cpp
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)
#define TFT_BL_FREQ       5000 // 5kHz PWM
```

**Touch Calibration:**
```cpp
#define TS_MIN_X  370
#define TS_MAX_X  3700
#define TS_MIN_Y  470
#define TS_MAX_Y  3600
```

### ⏱️ Watchdog Timer

```cpp
timeout_ms = 60000      // 60 detik (bukan default 5 detik)
trigger_panic = true    // Auto-restart jika hang
```

**Monitored Tasks:**
- WiFi Task
- Web Task

### 🔄 Web Interface Refresh Rates

| Item | Refresh Rate | Method |
|------|--------------|--------|
| Real-time clock | 1 detik | Client-side increment |
| Device status | 5 detik | Fetch `/devicestatus` |
| Prayer times | 30 detik | Fetch `/getprayertimes` |

### 🌍 Prayer Time Calculation

**Metode Perhitungan:**
- API: Aladhan (https://aladhan.com)
- Method: `5` (Egyptian General Authority of Survey)
- Koordinat: Dari database `cities.json`

**Mengapa Metode Mesir?**
- Format perhitungan lama Indonesia
- Kemenag RI mengubah metode sejak 2024
- Metode baru belum digunakan secara nasional

### 📍 GPS Coordinates System

**Current Implementation:**
- Koordinat disimpan dalam `cities.json`
- Format: `{"city":"Jakarta","lat":"-6.2088","lon":"106.8456"}`
- Akurasi: 4 desimal (±11 meter)

**Future Enhancement:**
```cpp
// Bisa dimodifikasi menggunakan modul GPS (NEO-6M/NEO-7M)
// GPS → Real-time coordinates → Aladhan API
```

### 📤 Cities JSON Upload

**Validasi:**
- Filename: Harus `cities.json`
- Size: Max 1MB
- Format: Valid JSON array
- Structure: `[{"city":"...","lat":"...","lon":"..."}]`

**Auto-refresh:**
- Dropdown city otomatis reload setelah upload
- Tidak perlu restart device

---

## 🌐 REST API Endpoint

### 📡 GET `/api/data`

Endpoint untuk integrasi IoT - memberikan semua data dalam satu request.

**Access URL:**
```
http://192.168.4.1/api/data        # Via AP
http://192.168.1.100/api/data      # Via WiFi (ganti IP sesuai device)
```

**Response Format:**
```json
{
  "time": "12:34:56",
  "date": "08/12/2024",
  "day": "Sunday",
  "timestamp": 1733673296,

  "prayerTimes": {
    "subuh": "04:32",
    "dzuhur": "11:58",
    "ashar": "15:21",
    "maghrib": "17:52",
    "isya": "19:03"
  },

  "location": {
    "city": "Jakarta",
    "cityId": "Jakarta",
    "displayName": "Jakarta",
    "latitude": "-6.2088",
    "longitude": "106.8456"
  },

  "device": {
    "wifiConnected": true,
    "wifiSSID": "MyWiFi",
    "ip": "192.168.1.20",
    "apIP": "192.168.4.1",
    "ntpSynced": true,
    "ntpServer": "pool.ntp.org",
    "freeHeap": 257000,
    "uptime": 123456
  }
}
```

**Response Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `time` | string | Current time (HH:MM:SS) |
| `date` | string | Current date (DD/MM/YYYY) |
| `day` | string | Day name (Monday, Tuesday, etc) |
| `timestamp` | number | Unix timestamp |
| `prayerTimes.subuh` | string | Fajr prayer time (HH:MM) |
| `prayerTimes.dzuhur` | string | Dhuhr prayer time (HH:MM) |
| `prayerTimes.ashar` | string | Asr prayer time (HH:MM) |
| `prayerTimes.maghrib` | string | Maghrib prayer time (HH:MM) |
| `prayerTimes.isya` | string | Isha prayer time (HH:MM) |
| `location.city` | string | Selected city name |
| `location.latitude` | string | City latitude |
| `location.longitude` | string | City longitude |
| `device.wifiConnected` | boolean | WiFi status |
| `device.ip` | string | Device IP (STA mode) |
| `device.ntpSynced` | boolean | NTP sync status |
| `device.freeHeap` | number | Free RAM (bytes) |

**Usage Example (cURL):**
```bash
# Simple request
curl http://192.168.1.100/api/data

# Pretty print with jq
curl -s http://192.168.1.100/api/data | jq '.'

# Get only prayer times
curl -s http://192.168.1.100/api/data | jq '.prayerTimes'

# Watch mode (update every 1 second)
watch -n 1 'curl -s http://192.168.1.100/api/data | jq "."'
```

**Features:**
- ✅ CORS enabled
- ✅ No authentication required
- ✅ No rate limiting
- ✅ Real-time data (no caching)
- ❌ HTTP only (no HTTPS)
- ❌ Local network only

**Integration Notes:**
1. Set timeout 5-10 detik untuk request yang aman
2. Pastikan device di network yang sama
3. IP address bisa berubah jika DHCP
4. Gunakan hostname jika mDNS enabled

---

## 🔍 Troubleshooting

### ❌ Compile Errors

**`ledcAttach() not declared`**
```
Penyebab: ESP32 Board bukan v3.0.7
Solusi: 
1. Tools → Boards Manager
2. Uninstall ESP32 board
3. Install ESP32 v3.0.7 (PENTING: Pilih versi ini!)
```

**LVGL function errors**
```
Penyebab: LVGL bukan v9.2.0
Solusi:
1. Sketch → Include Library → Manage Libraries
2. Cari "LVGL"
3. Uninstall versi lama
4. Install LVGL 9.2.0 (PENTING: Pilih versi ini!)
```

**`lv_display_create()` undefined**
```
Penyebab: LVGL masih v8.x
Solusi: Install LVGL 9.2.0 (API berbeda di v9)
```

---

### ❌ Upload Errors

**`A fatal error occurred: Timed out waiting for packet header`**
```
Solusi:
1. Tekan tombol BOOT saat mulai upload
2. Atau: Tools → Upload Speed → 115200 (lebih lambat tapi stabil)
3. Ganti kabel USB (kualitas kabel penting!)
4. Pastikan driver CH340/CP2102 terinstall
```

**LittleFS upload failed**
```
Solusi:
1. Pastikan plugin LittleFS terinstall
2. Cek folder data/ ada dan berisi file
3. Tools → Partition Scheme → Default 4MB with spiffs
```

---

### ❌ Runtime Issues

**AP tidak muncul**
```
Solusi:
1. Cek serial monitor (115200 baud)
2. Pastikan power supply 5V min 2A
3. Restart device (tekan RESET button)
4. Cek LED blink pattern
```

**WiFi tidak connect**
```
Solusi:
1. Cek SSID/password benar (case-sensitive!)
2. Router harus support 2.4GHz (bukan 5GHz only)
3. Cek jarak ke router (sinyal lemah)
4. Restart device dan coba lagi
5. Factory reset: Tekan tombol RESET 3 detik
```

**Prayer times tidak update**
```
Solusi:
1. Cek WiFi connected (serial: "✅ WiFi Connected")
2. Cek city sudah dipilih (web interface)
3. Tunggu midnight (00:00) atau restart device
4. Cek koordinat city di cities.json valid
5. Test manual: Web interface → "Sync Prayer Times"
```

**Jam tidak akurat setelah mati lampu**
```
Solusi:
1. Install RTC DS3231 module (lihat pinout)
2. Pasang baterai CR2032 di RTC
3. RTC akan auto-detected saat boot
4. Serial akan tampilkan: "✓ DS3231 detected!"
```

**Jam masih 01/01/2000 setelah lama**
```
Solusi:
1. Normal behavior saat belum NTP sync
2. Cek WiFi connected
3. Tunggu auto NTP sync (max 1 menit)
4. Atau manual sync: Web interface → "Sync Time Now"
5. Jika punya RTC, jam akan auto-load dari RTC
```

**Display blank/putih**
```
Solusi:
1. Cek kabel display terpasang dengan baik
2. Backlight mungkin mati, cek:
   - Serial: "Backlight ON: 180/255"
3. Adjust brightness di code:
   - #define TFT_BL_BRIGHTNESS 255 (max)
4. Cek power supply cukup (min 2A)
```

**Touch tidak responsif**
```
Solusi:
1. Kalibrasi touch di code (TS_MIN/MAX_X/Y)
2. Clean layar (debu/kotoran)
3. Cek koneksi touch controller
4. Test via serial monitor (ada log touch event)
```

**Web interface lambat/timeout**
```
Solusi:
1. Cek sinyal WiFi (pindah lebih dekat ke router)
2. Reduce concurrent sessions (max 5)
3. Clear browser cache
4. Restart device
5. Gunakan AP mode jika WiFi bermasalah
```

**Session expired terus**
```
Solusi:
1. Normal jika idle > 15 menit
2. Refresh page untuk session baru
3. Jangan gunakan multiple tabs bersamaan
4. Clear browser cookies
```

**Cities dropdown kosong**
```
Solusi:
1. Cek cities.json sudah diupload (LittleFS)
2. File size cities.json < 1MB
3. Format JSON valid (test di jsonlint.com)
4. Re-upload data/ folder
```

---

### 🔧 Advanced Debugging

**Enable Verbose Logging:**
```cpp
// Di awal jws.ino, tambahkan:
#define DEBUG_WIFI
#define DEBUG_NTP
#define DEBUG_PRAYER

// Atau di serial monitor, cek:
Serial.setDebugOutput(true);
```

**Monitor Free Heap:**
```cpp
// Tambahkan di loop():
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
```

**Check Task States:**
```bash
# Via serial monitor, output otomatis setiap 30 detik:
=== WEB SERVER STATUS ===
Free Heap: 257000 bytes (251.0 KB)
Active Sessions: 2/5
Total: 2/5 sessions
========================
```

---

## 📁 File Structure

```
esp32-prayer-clock/
├── jws.ino                 # Main program (8000+ lines)
├── src/                    # EEZ Studio generated UI
│   ├── ui.h
│   ├── ui.cpp
│   ├── screens.h
│   ├── images.h
│   └── fonts.h
├── data/                   # LittleFS filesystem (upload ke ESP32)
│   ├── index.html          # Web interface (responsive)
│   ├── assets/
│   │   └── css/
│   │       └── foundation.css  # Zurb Foundation CSS
│   └── cities.json         # 500+ cities database
├── README.md               # This file
├── LICENSE                 # MIT License
└── platformio.ini          # PlatformIO config (optional)
```

**File Sizes:**
- `jws.ino`: ~300KB (source code)
- `index.html`: ~15KB (compressed HTML)
- `foundation.css`: ~150KB (minified CSS)
- `cities.json`: ~80KB (500+ cities)
- **Total LittleFS**: ~250KB

---

## 🔐 Security Considerations

### ⚠️ Default Credentials
```
AP SSID: JWS ESP32
AP Password: 12345678
```

### 🛡️ Security Recommendations

**DO:**
- ✅ Ganti AP password via web interface
- ✅ Gunakan strong WiFi password
- ✅ Akses web interface hanya dari trusted network
- ✅ Update firmware secara berkala
- ✅ Monitor session activity

**DON'T:**
- ❌ Expose ke public internet
- ❌ Gunakan default password di production
- ❌ Share session token
- ❌ Simpan sensitive data di device

### 🔒 Security Features

| Feature | Status | Notes |
|---------|--------|-------|
| HTTPS | ❌ | HTTP only (local network) |
| Authentication | ✅ | Session-based (15 min expire) |
| CORS | ✅ | Enabled untuk `/api/data` |
| Rate Limiting | ❌ | No limit (trust local network) |
| Input Validation | ✅ | Server-side validation |
| XSS Protection | ✅ | HTML escaping |
| CSRF Protection | ⚠️ | Session token (basic) |

**Threat Model:**
- ✅ Designed for: Home/office local network
- ❌ NOT designed for: Public internet exposure
- ⚠️ WiFi credentials: Stored plain text in LittleFS

---

## 🔄 Update & Maintenance

### Firmware Update
```
1. Download latest .ino dari GitHub
2. Upload via Arduino IDE/PlatformIO
3. LittleFS data tidak perlu diupload ulang (kecuali ada perubahan)
```

### Database Update
```
1. Web interface → Upload Cities JSON
2. Atau: Re-upload data/ folder via LittleFS
```

### Factory Reset
```
Option 1: Via web interface
   - Buka: http://<IP>/
   - Klik: "Factory Reset"

Option 2: Via serial monitor
   - Send command: "RESET"

Option 3: Hardware
   - Tekan BOOT + RESET bersamaan
   - Lepas RESET (masih tekan BOOT)
   - Lepas BOOT setelah 3 detik
```

---
