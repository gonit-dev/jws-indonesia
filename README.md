# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan web interface dan RTC backup

[![Version](https://img.shields.io/badge/version-2.1-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/) [![License](https://img.shields.io/badge/license-MIT-yellow)](LICENSE)

---

## 📸 Preview

<div align="center">

<img src="https://github.com/user-attachments/assets/d0e64d2a-2a00-4a62-b92b-9aa36d95f4b8" alt="Home" width="200">

<img src="https://github.com/user-attachments/assets/55d84d62-d986-460a-b2b2-3ac4d2b4aaf1" alt="WiFi" width="200">

<img src="https://github.com/user-attachments/assets/b9f1db1c-83f2-4492-aef8-7b62997da9a9" alt="Time" width="200">

<img src="https://github.com/user-attachments/assets/84408e8e-ef1b-4ee5-aa42-9c0f965bb031" alt="City" width="200">

<img src="https://github.com/user-attachments/assets/6479dfd9-99b9-4034-a0d2-29453d6c46d9" alt="Coords" width="200">

<img src="https://github.com/user-attachments/assets/1f105d28-80a6-490c-a4a9-d7f8174a3c3e" alt="Prayer" width="200">

</div>

---

## ✨ Fitur Utama

### 🕌 Prayer Times
- **Jadwal Sholat Otomatis** - Update otomatis tengah malam via Aladhan API
- **Visual Notification** - Blinking waktu sholat selama 1 menit saat masuk waktu
- **8 Metode Kalkulasi** - Kemenag, MWL, Egyptian, ISNA, Shia, Karachi, Makkah, Tehran
- **500+ Kota** - Database kota Indonesia dengan koordinat GPS akurat
- **Manual Coordinates** - Edit koordinat GPS dengan validasi real-time

### ⏰ Time Management
- **NTP Auto-Sync** - Sinkronisasi jam otomatis setiap 1 jam
- **Multiple NTP Servers** - Fallback ke pool.ntp.org, time.google.com, time.windows.com
- **RTC DS3231 Backup** - Jam tetap akurat saat mati lampu (battery backup)
- **Timezone Support** - UTC-12 hingga UTC+14 dengan validasi
- **Manual Sync** - Sync waktu dari browser client

### 🌐 Network Features
- **Dual Mode WiFi** - AP (Access Point) + STA (Station) concurrent
- **Event-Driven Connection** - Auto-reconnect cepat tanpa polling
- **Custom AP Settings** - SSID, Password, IP, Gateway, Subnet configurable
- **Connection Monitor** - Real-time WiFi status dan RSSI display
- **Fast Reconnect** - Auto-reconnect dalam 3-5 detik

### 🖥️ User Interface
- **LVGL 9.2.0** - Smooth touchscreen UI @ 20 FPS
- **EEZ Studio Design** - Professional UI layout
- **Responsive Web** - Mobile-friendly interface dengan Foundation CSS
- **Real-Time Display** - Clock, date, prayer times update otomatis
- **Loading Manager** - Progressive loading dengan status feedback

### 🔊 Buzzer Control
- **Per-Prayer Toggle** - Enable/disable buzzer untuk setiap waktu sholat
- **Volume Control** - 0-100% adjustable dengan live preview
- **PWM Output** - Smooth buzzer sound via GPIO26
- **Automatic Alert** - Buzzer ON/OFF sync dengan blinking display

### 💾 Data Persistence
- **LittleFS Storage** - Semua konfigurasi tersimpan di flash
- **Auto-Save** - Konfigurasi tersimpan otomatis setelah perubahan
- **Factory Reset** - Reset ke default settings dengan countdown safety

### 🔄 Smart Updates
- **Midnight Auto-Update** - Prayer times update otomatis tengah malam dengan NTP sync
- **On-Demand Update** - Manual update via web interface
- **Background Processing** - Non-blocking updates tanpa freeze UI
- **Time Validation** - Validasi timestamp sebelum API request

### 🛡️ Safety Features
- **Countdown System** - 60 detik countdown sebelum restart/reset
- **Debouncing Protection** - Prevent multiple rapid restarts (3 detik min interval)
- **Mutex Locking** - Thread-safe operations untuk semua tasks
- **Watchdog Timer** - 60 detik timeout dengan auto-recovery
- **Connection Type Detection** - Smart redirect berdasarkan akses (Local AP vs Remote)

---

## 🔧 Hardware Requirements

### Board: ESP32-2432S024
- **MCU:** ESP32 Dual-Core @ 240MHz
- **RAM:** 520KB SRAM
- **Flash:** 4MB (3MB APP + 1MB SPIFFS)
- **Display:** ILI9341 2.4" TFT (320x240)
- **Touch:** XPT2046 Resistive
- **WiFi:** 802.11 b/g/n (2.4GHz only)
- **Power:** 5V USB (min 2A recommended)

### Pinout
```
TFT Display:
- TFT_BL     → GPIO 27 (PWM Backlight)
- TFT_SPI    → VSPI (Default)

Touch Screen:
- TOUCH_CS   → GPIO 33
- TOUCH_IRQ  → GPIO 36
- TOUCH_MOSI → GPIO 13
- TOUCH_MISO → GPIO 12
- TOUCH_CLK  → GPIO 14

Buzzer:
- BUZZER_PIN → GPIO 26 (PWM)
```

### RTC DS3231 (Opsional - Highly Recommended)
```
DS3231       ESP32
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21
SCL     →    GPIO 22
```

**RTC Features:**
- Battery backup (CR2032) untuk time persistence
- Automatic leap year compensation
- Temperature compensated crystal oscillator (±2ppm accuracy)
- Hardware validation untuk detect faulty module

---

## 📦 Instalasi

### 1. Requirements

| Komponen | Versi | Wajib | Catatan |
|----------|-------|-------|---------|
| ESP32 Board | v3.0.7 | ✅ | `ledcAttach()` requires v3.x |
| LVGL | 9.2.0 | ✅ | v8.x tidak compatible |
| TFT_eSPI | 2.5.0+ | ✅ | |
| XPT2046_Touchscreen | 1.4+ | ✅ | |
| ArduinoJson | 6.21.0+ | ✅ | v7.x compatible |
| ESPAsyncWebServer | 1.2.3+ | ✅ | |
| AsyncTCP | 1.1.1+ | ✅ | |
| TimeLib | 1.6.1+ | ✅ | |
| RTClib | 2.1.1+ | ✅ | Adafruit version |
| Arduino IDE | 2.x+ | - | |

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Search: "esp32"
Install: esp32 by Espressif Systems v3.0.7
```

### 3. Install Libraries

Via Library Manager (Sketch → Include Library → Manage Libraries):

```
✅ LVGL                   9.2.0      (by LVGL)
✅ TFT_eSPI               2.5.0+     (by Bodmer)
✅ XPT2046_Touchscreen    1.4+       (by Paul Stoffregen)
✅ ArduinoJson            6.21.0+    (by Benoit Blanchon)
✅ ESPAsyncWebServer      1.2.3+     (by me-no-dev)
✅ AsyncTCP               1.1.1+     (by me-no-dev)
✅ TimeLib                1.6.1+     (by Michael Margolis)
✅ RTClib                 2.1.1+     (by Adafruit)
```

### 4. Configure TFT_eSPI

**Location:** `Arduino/libraries/TFT_eSPI/User_Setup_Select.h`

Comment default, uncomment:
```cpp
#include <User_Setups/Setup24_ST7789.h>  // atau sesuaikan dengan board
```

**Or create custom:** `User_Setup.h`
```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
// ... sesuaikan dengan board Anda
```

### 5. Upload

```bash
# Clone repository
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia

# Upload filesystem (data/ folder)
Tools → ESP32 Sketch Data Upload

# Upload code
Sketch → Upload (Ctrl+U)
```

**Board Settings:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
Core Debug Level: None (for production)
```

---

## 🚀 Quick Start

### Step 1: First Boot
```
📶 AP SSID: "JWS Indonesia"
🔐 Password: "12345678"
🌐 IP Address: http://192.168.4.1
⏰ Initial Time: 01/01/2000 00:00:00 (waiting NTP sync)
```

**Serial Monitor Output:**
```
========================================
ESP32 Islamic Prayer Clock
LVGL 9.2.0 + FreeRTOS
VERSION 2.1 - MULTI-CLIENT
========================================
DS3231 not found! (atau: DS3231 detected)
WiFi Mode: AP + STA
AP Started: JWS Indonesia
AP IP: 192.168.4.1
System Ready
========================================
```

### Step 2: Konfigurasi WiFi
1. **Connect to AP**
   - SSID: "JWS Indonesia"
   - Password: "12345678"

2. **Open Browser**
   - URL: `http://192.168.4.1`
   - Mobile responsive (works on phone)

3. **Configure WiFi Router**
   - Tab **WIFI** → Section "WiFi Router"
   - Masukkan SSID WiFi rumah (case-sensitive)
   - Masukkan Password (min 8 karakter)
   - Klik **Simpan**

4. **Wait for Connection**
   - Device akan auto-connect (~15 detik)
   - Check serial monitor untuk IP address
   - Access via: `http://<IP-ESP32>`

### Step 3: Set Lokasi & Metode
1. **Select City**
   - Tab **LOKASI** → Dropdown "Pilih Lokasi"
   - Pilih dari 500+ kota Indonesia
   - Auto-grouping by province

2. **Verify/Edit Coordinates** (Optional)
   - Koordinat muncul otomatis
   - Edit manual jika perlu koreksi
   - Format: -6.2088 (latitude), 106.8456 (longitude)
   - Validasi: lat (-90 to 90), lon (-180 to 180)

3. **Select Calculation Method**
   - Default: Egyptian General Authority of Survey
   - Options: Kemenag, MWL, ISNA, Shia, Karachi, Makkah, Tehran
   - Klik **Simpan**

4. **Auto-Update**
   - Prayer times akan auto-fetch dari API
   - Display update dalam 3-5 detik
   - Check serial monitor untuk logs

### Step 4: Timezone Configuration
```
Default: UTC+7 (WIB - Waktu Indonesia Barat)

Indonesia Timezones:
- WIB:  +7  (Jawa, Sumatera, Kalimantan Barat)
- WITA: +8  (Kalimantan Tengah/Timur, Sulawesi, Bali, NTB, NTT)
- WIT:  +9  (Papua, Maluku)
```

**Set Timezone:**
1. Tab **WAKTU**
2. Klik **🕐** (edit icon)
3. Input: +7, +8, +9, atau -12 to +14
4. Klik **💾** (save) atau Enter
5. NTP akan auto re-sync dengan timezone baru

### Step 5: Buzzer Configuration
1. Tab **JADWAL**
2. Toggle ON/OFF untuk setiap waktu sholat
3. Adjust volume slider (0-100%)
4. Auto-save setiap perubahan

---

## 🌐 Web Interface

### Access Methods

**AP Mode (Default):**
```
URL: http://192.168.4.1
Network: JWS Indonesia
No internet required
```

**STA Mode (After WiFi config):**
```
URL: http://<IP-ESP32>
Check serial monitor for IP
Internet connection available
```

**Dual Access:**
```
Akses via AP: http://192.168.4.1
Akses via WiFi: http://192.168.x.x
Both work simultaneously
```

### Tab Overview

#### 🏠 BERANDA
**Status Perangkat:**
- Jaringan WiFi (SSID yang terhubung)
- Alamat IP (Local IP dari router)
- Internet Status (Connected/Disconnected badge)
- Status NTP (Tersinkron/Belum tersinkron)
- Server NTP (pool.ntp.org, time.google.com, dll)
- Waktu Real-Time (update setiap detik)
- Tanggal (DD/MM/YYYY format)
- Uptime (HH:MM:SS atau Dhari HH:MM:SS)

**Actions:**
- **Mulai Ulang Perangkat** - Safe restart dengan 60 detik countdown

#### 📡 WIFI
**WiFi Router Configuration:**
- SSID Input (text, required)
- Password Input (text, min 8 chars)
- Simpan → Trigger reconnect dengan countdown
- Batal → Restore saved values

**Access Point Configuration:**
- SSID AP (text, required, custom name)
- Password AP (text, min 8 chars)
- Simpan → Restart AP dengan countdown
- Batal → Restore saved values

**Konfigurasi Jaringan AP:**
- IP Address (format xxx.xxx.xxx.xxx)
- Gateway (format xxx.xxx.xxx.xxx)
- Subnet Mask (format xxx.xxx.xxx.xxx)
- Real-time validation
- Simpan → Apply dan restart AP

**Safety Features:**
- Connection type detection (Local AP vs Remote)
- Smart countdown hanya untuk local AP users
- Debouncing protection (3 detik minimum)

#### ⏰ WAKTU
**Pembaruan Manual:**
- Button "Perbarui Waktu"
- Sync waktu dari browser client
- Update display dan RTC

**Pembaruan Otomatis (NTP):**
- Auto-sync setiap 1 jam
- Fallback ke multiple servers
- Status display (Tersinkron/Belum)
- Server yang digunakan

**Zona Waktu:**
- Inline editing (click 🕐 icon)
- Input UTC offset (-12 to +14)
- Validation real-time
- Auto NTP re-sync setelah save
- Prayer times auto-update dengan timezone baru

**Timezone Shortcuts:**
- 🇮🇩 WIB = +7
- 🇮🇩 WITA = +8
- 🇮🇩 WIT = +9

#### 🕌 LOKASI
**Pilih Lokasi:**
- Dropdown 500+ kota Indonesia
- Grouped by province
- Search functionality
- Koordinat display otomatis

**Ubah Koordinat:**
- Latitude input (-90 to 90)
- Longitude input (-180 to 180)
- Default coordinates dari JSON
- Reset button ke coordinates original
- Batal button restore saved values
- Real-time validation

**Location Type Display:**
- 🏙️ City (Kota)
- 🏞️ District (Kabupaten)
- 🏘️ Sub-District (Kecamatan)
- 🏡 Village (Kelurahan/Desa)

**Metode Kalkulasi:**
- 8 calculation methods
- Default: Egyptian General Authority
- Auto-update prayer times setelah change
- Current method display di 2 locations

**Perbarui Lokasi (Upload cities.json):**
- File input dengan drag & drop support
- Validation: filename must be "cities.json"
- Max size: 1MB
- JSON format validation
- GPS coordinates validation
- ArduinoJson size calculation
- Progress bar dengan percentage
- Auto-reload dropdown setelah success

#### 🙏 JADWAL
**Prayer Times Display:**
- Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya
- Format: HH:MM (24-hour)
- Auto-update dari server

**Buzzer Toggle Switches:**
- Individual ON/OFF untuk setiap waktu
- Visual switch (Foundation CSS)
- Auto-save on change

**Volume Control:**
- HTML5 range slider (0-100%)
- Real-time percentage display
- PWM control via GPIO26
- Debounced save (500ms)

**Current Method Display:**
- Show selected calculation method
- Update on method change

**Auto-Update Info:**
- Tengah malam automatic update
- NTP sync before update
- Background processing

#### ⚠️ RESET
**Factory Reset:**
- Delete all configuration files
- Reset to default AP settings
- Reset time to 01/01/2000
- Device auto-restart dengan countdown
- Confirmation dialog
- 60 detik countdown dengan UI lock

**Files Deleted:**
```
/wifi_creds.txt
/ap_creds.txt
/prayer_times.txt
/city_selection.txt
/method_selection.txt
/timezone.txt
/buzzer_config.txt
```

**Default Values:**
```
AP SSID: JWS Indonesia
AP Password: 12345678
AP IP: 192.168.4.1
Method: Egyptian (ID 5)
Timezone: UTC+7
```

### Countdown System

**Visual Feedback:**
- Toast notification (top center)
- Full-screen overlay (prevent interaction)
- Real-time countdown display
- Color changes:
  - Yellow warning (60-20 detik)
  - Orange alert (20-10 detik)
  - Red critical (10-0 detik)

**Server-Driven:**
- Polling setiap 1 detik dari `/api/countdown`
- Auto-sync dengan server time
- Graceful handling jika connection lost
- Auto-redirect setelah countdown complete

**Smart Redirect:**
- Local AP users: redirect to new AP IP
- Remote users: redirect to current URL
- Auto-detect connection type

---

## 📊 System Architecture

### FreeRTOS Tasks

| Task | Core | Priority | Stack | Interval | Function |
|------|------|----------|-------|----------|----------|
| **UI Task** | 1 | 3 (Highest) | 12KB | 50ms | LVGL rendering, touch handling, blink animation |
| **WiFi Task** | 0 | 2 (High) | 5KB | Event-driven | Connection management, auto-reconnect |
| **NTP Task** | 0 | 2 (High) | 6KB | On-demand | Time sync dengan multiple servers |
| **Web Task** | 0 | 1 (Low) | 5KB | 5s monitor | AsyncWebServer, connection cleanup |
| **Prayer Task** | 0 | 1 (Low) | 6KB | 1s check | Midnight update, background processing |
| **Clock Task** | 0 | 2 (High) | 2KB | 1s tick | System time increment, auto NTP sync |
| **RTC Sync** | 0 | 1 (Low) | 2KB | 60s | Bidirectional RTC ↔ System sync |

### Synchronization

**Semaphores/Mutexes:**
```cpp
displayMutex     → LVGL operations (UI Task)
timeMutex        → Time config (All tasks)
wifiMutex        → WiFi config (WiFi/Web tasks)
settingsMutex    → All config files (Multiple tasks)
spiMutex         → TFT/Touch SPI bus (UI Task)
i2cMutex         → RTC I2C bus (RTC Sync Task)
wifiRestartMutex → Restart protection (Web Task)
countdownMutex   → Countdown state (Web Task)
```

**Queue:**
```cpp
displayQueue → Push display updates from any task to UI Task
```

### Auto-Update System

**Midnight Prayer Update Flow:**
```
00:00-00:05 Detected
    ↓
Trigger NTP Sync Task
    ↓
Wait for NTP Complete (max 30s)
    ↓
Validate Timestamp (> 01/01/2000)
    ↓
Fetch Prayer Times API
    ↓
Save to LittleFS
    ↓
Queue Display Update
```

**Hourly NTP Sync:**
```
Clock Task Counter = 3600 seconds
    ↓
Trigger NTP Sync Task
    ↓
Sync dengan NTP servers (fallback)
    ↓
Update System Time
    ↓
Save to RTC (if available)
    ↓
Queue Display Update
```

**RTC Sync Logic:**
```
Every 60 seconds:
    Check RTC validity
        ↓
    Compare RTC vs System time
        ↓
    If NTP not synced → Use RTC time
    If NTP synced & System newer → Keep System time
    If RTC newer → Update System from RTC
```

### Event-Driven WiFi

**No Polling Design:**
```cpp
WiFi.onEvent() → Set event bits → Tasks wait with timeout
```

**Events:**
```cpp
WIFI_CONNECTED_BIT    → STA connected to router
WIFI_GOT_IP_BIT       → Got IP address, trigger NTP
WIFI_DISCONNECTED_BIT → Connection lost, trigger reconnect
```

**Benefits:**
- CPU idle saat stable
- Fast response (<10ms)
- Auto-reconnect tanpa user intervention
- AP tetap hidup saat STA disconnect

---

## 🔍 Troubleshooting

### Compile Errors

**`ledcAttach() not declared in this scope`**
```
Root Cause: ESP32 Board v2.x (old API)
Solution: Install ESP32 Board v3.0.7
Location: Tools → Board → Boards Manager → esp32

Verification:
Tools → Board → ESP32 Dev Module
Help → About Arduino IDE → Check installed version
```

**LVGL function errors (lv_xxx not declared)**
```
Root Cause: LVGL v8.x installed (old API)
Solution: Install LVGL v9.2.0
Location: Sketch → Include Library → Manage Libraries → LVGL

Verification:
Check: Arduino/libraries/lvgl/library.properties
Should show: version=9.2.0
```

**`AsyncWebServer.h: No such file or directory`**
```
Root Cause: Missing ESPAsyncWebServer library
Solution: Install via Library Manager

Dependencies:
1. ESPAsyncWebServer by me-no-dev (v1.2.3+)
2. AsyncTCP by me-no-dev (v1.1.1+)

Note: Install AsyncTCP FIRST, then ESPAsyncWebServer
```

**`RTClib.h: No such file or directory`**
```
Root Cause: Missing Adafruit RTClib
Solution: Install Adafruit RTClib v2.1.1+

Location: Library Manager → Search "RTClib"
Select: RTClib by Adafruit (NOT other forks)
```

### Upload Errors

**`A fatal error occurred: Timed out waiting for packet header`**
```
Solutions (Try in order):

1. Hardware Method:
   - Tekan & tahan tombol BOOT
   - Klik Upload di Arduino IDE
   - Tahan BOOT sampai "Connecting..." muncul
   - Lepas BOOT saat upload dimulai

2. Software Method:
   - Kurangi Upload Speed
   - Tools → Upload Speed → 115200 (from 921600)
   - Retry upload

3. Cable & Driver:
   - Ganti kabel USB (pastikan data+power, bukan charge-only)
   - Install driver CH340/CP2102
   - Windows: Check Device Manager
   - Mac: Install driver dari manufacturer

4. Port Selection:
   - Tools → Port → Select correct COM port
   - Disconnect other USB devices
   - Try different USB port on computer
```

**`Sketch too big: Program size exceeds available space`**
```
Solution: Change Partition Scheme
Tools → Partition Scheme → Default 4MB with spiffs (1.2MB APP)

If still too big:
1. Remove unused libraries from #include
2. Reduce LVGL buffer size
3. Disable debug Serial.println() statements
```

**`SPIFFS Upload Failed: Not Enough Space`**
```
Check:
1. data/ folder size must be < 1MB
2. Large files: index.html, foundation.min.css, cities.json

Optimization:
- Minify HTML/CSS
- Compress cities.json (remove whitespace)
- Remove unused assets
```

### Runtime Issues

**WiFi tidak connect / auto-disconnect**
```
Checks:
✅ SSID/Password case-sensitive (check typo)
✅ Router harus 2.4GHz (ESP32 tidak support 5GHz)
✅ Check router MAC filtering / whitelist
✅ Check router DHCP pool (available IP?)
✅ Distance ke router (signal strength)

Debug:
Serial monitor → Check RSSI value
RSSI > -50 dBm: Excellent
RSSI -50 to -70 dBm: Good
RSSI < -70 dBm: Weak (too far)

Solutions:
1. Factory reset → Reconfigure WiFi
2. Restart router
3. Try mobile hotspot for testing
4. Check router logs
```

**Jadwal sholat tidak akurat / selisih beberapa menit**
```
Root Cause: GPS coordinates tidak akurat

Solutions:
1. Edit koordinat GPS manual
   - Buka Google Maps
   - Klik exact location (rumah/masjid)
   - Copy latitude & longitude
   - Paste ke web interface

2. Ganti metode kalkulasi
   - Tab LOKASI → Dropdown "Metode Kalkulasi"
   - Try: Kemenag Indonesia (Method 20)
   - Or: Egyptian General Authority (Method 5)

3. Verify city selection
   - Pastikan city selected sesuai dengan lokasi fisik
   - Bukan koordinat provinsi/pusat kota

Validation:
- Check via https://aladhan.com/prayer-times-api
- Compare dengan jadwal sholat masjid terdekat
```

**Jam reset ke 01/01/2000 setelah mati lampu**
```
Status: NORMAL jika:
- Belum ada NTP sync
- RTC DS3231 tidak terpasang
- RTC battery (CR2032) habis

Solutions:
1. Install RTC DS3231 module
   - Wiring: SDA→GPIO21, SCL→GPIO22
   - Insert CR2032 battery (3V)
   - Power cycle device

2. Connect ke WiFi
   - Device akan auto NTP sync
   - Time akan accurate dalam 10-30 detik

3. Check RTC status
   - Serial monitor → Look for:
     "DS3231 detected" (OK)
     "DS3231 not found" (Module issue)
     "RTC hardware test PASSED" (OK)
     "RTC HARDWARE FAILURE" (Faulty module)

4. Replace faulty RTC
   - Symptom: Time returns garbage data
   - Temperature sensor works but time invalid
   - Solution: Buy new genuine DS3231 module
```

**Prayer times tidak update otomatis tengah malam**
```
Checks:
✅ WiFi connected (need internet untuk API)
✅ NTP synced (time harus accurate)
✅ City & coordinates selected
✅ Serial monitor → Check "MIDNIGHT DETECTED" log

Debug Logs:
00:00-00:05 → Should see:
"MIDNIGHT DETECTED - STARTING SEQUENCE"
"Triggering NTP Sync..."
"NTP SYNC COMPLETED"
"Updating Prayer Times..."

If stuck:
1. Manual update via web interface (Tab LOKASI → Save)
2. Check internet connection
3. Try different NTP server
4. Factory reset if persistent
```

**Web interface loading sangat lambat / timeout**
```
Possible Causes:
1. WiFi sleep enabled (should be disabled)
2. Weak signal (RSSI < -70 dBm)
3. Multiple clients simultaneously
4. Browser cache issue

Solutions:
1. Check WiFi sleep status
   Serial → Should see:
   "WiFi Sleep: DOUBLE DISABLED"
   
2. Move closer to device
3. Clear browser cache
   - Ctrl+Shift+Delete (Chrome/Firefox)
   - Select "Cached images and files"
   
4. Try incognito/private mode
5. Use different browser
6. Restart device
```

**Touch tidak responsif / coordinate salah**
```
Calibration:
1. Check constants di code:
   #define TS_MIN_X 370
   #define TS_MAX_X 3700
   #define TS_MIN_Y 470
   #define TS_MAX_Y 3600

2. Test touch corners
   - Top-left should register (0,0)
   - Bottom-right should register (320,240)

3. Adjust values if needed
   - Serial monitor → Check touch coordinates
   - Calculate new MIN/MAX values
   - Recompile & upload

4. Clean touchscreen
   - Fingerprint oil affects accuracy
   - Use microfiber cloth
```

**Display flicker / tearing**
```
Causes:
1. Insufficient power supply (< 2A)
2. Bad USB cable (voltage drop)
3. SPI speed too high/low

Solutions:
1. Use 5V 2A+ power adapter
2. Shorter USB cable (< 1 meter)
3. Check TFT_eSPI SPI frequency settings
4. Add capacitor 100-470µF near power input
```

**Buzzer tidak bunyi saat waktu sholat**
```
Checks:
✅ Buzzer toggle enabled (Tab JADWAL)
✅ Volume tidak 0% 
✅ GPIO26 connection (check wiring)
✅ PWM channel initialized

Debug:
Serial monitor → Saat waktu sholat:
"PRAYER TIME ENTER: SUBUH"
"Starting to blink for 1 minute..."

Solutions:
1. Test buzzer manual
   - Set volume 50%
   - Enable toggle untuk waktu sholat terdekat
   
2. Check GPIO26
   - Use multimeter/oscilloscope
   - Should see PWM signal saat blinking
   
3. Replace buzzer if faulty
   - Test dengan buzzer lain
   - Pastikan 3.3V compatible
```

**Memory leaks / device hang setelah lama berjalan**
```
Monitoring:
Serial → Check stack report (every 2 minutes):
"STACK USAGE ANALYSIS"
"CRITICAL TASKS: ..." (if any > 90%)

"MEMORY STATUS"
"Lost: XXX bytes" (should be < 20KB)

Solutions:
1. Increase stack size jika critical
   - Edit: #define UI_TASK_STACK_SIZE 12288
   - Increase by 2048 increments
   
2. Reduce heap fragmentation
   - Factory reset
   - Restart device periodically
   
3. Watchdog protection
   - Device auto-restart jika hang
   - Check serial for watchdog triggers
```

---

## 🌐 API Endpoints

### GET `/api/data` - IoT Integration

**Description:** Real-time system data untuk external integration (Node-RED, Home Assistant, MQTT bridge, dll)

**Request:**
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
    "cityId": "jakarta",
    "displayName": "Jakarta (Kota)",
    "latitude": "-6.175392",
    "longitude": "106.827153"
  },
  "device": {
    "wifiConnected": true,
    "apIP": "192.168.4.1",
    "ntpSynced": true,
    "ntpServer": "pool.ntp.org",
    "freeHeap": 245632,
    "uptime": 3600
  }
}
```

**Fields:**
- `time` - Current time (HH:MM:SS)
- `date` - Current date (DD/MM/YYYY)
- `day` - Day name (English)
- `timestamp` - Unix timestamp (seconds)
- `prayerTimes` - All prayer times (HH:MM format)
- `location` - City info + GPS coordinates
- `device.wifiConnected` - WiFi status (boolean)
- `device.ntpSynced` - NTP sync status (boolean)
- `device.freeHeap` - Available RAM (bytes)
- `device.uptime` - Device uptime (seconds)

**Use Cases:**
```javascript
// Node-RED: HTTP Request node
GET http://192.168.4.1/api/data
Parse JSON → Extract prayerTimes → Send notification

// Home Assistant: REST Sensor
sensor:
  - platform: rest
    resource: http://192.168.4.1/api/data
    name: JWS Prayer Times
    json_attributes:
      - prayerTimes
      - location
    value_template: '{{ value_json.time }}'

// Python Script
import requests
response = requests.get('http://192.168.4.1/api/data')
data = response.json()
next_prayer = data['prayerTimes']['subuh']
```

### GET `/api/countdown` - Countdown Status

**Description:** Server-driven countdown untuk restart/reset operations

**Request:**
```bash
curl http://192.168.4.1/api/countdown
```

**Response (Active):**
```json
{
  "active": true,
  "remaining": 45,
  "total": 60,
  "message": "Memulai ulang Access Point",
  "reason": "ap_restart",
  "serverTime": 123456789
}
```

**Response (Inactive):**
```json
{
  "active": false,
  "remaining": 0,
  "total": 0,
  "message": "",
  "reason": "",
  "serverTime": 123456789
}
```

**Reason Types:**
- `ap_restart` - Access Point restart
- `device_restart` - Full device restart
- `factory_reset` - Factory reset operation

**Frontend Implementation:**
```javascript
// Poll every 1 second
setInterval(async () => {
  const response = await fetch('/api/countdown');
  const data = await response.json();
  
  if (data.active) {
    // Show countdown overlay
    updateCountdownDisplay(data.remaining, data.message);
  } else {
    // Hide overlay, redirect if needed
    handleCountdownComplete(data.reason);
  }
}, 1000);
```

### GET `/devicestatus` - Device Status

**Request:**
```bash
curl http://192.168.4.1/devicestatus
```

**Response:**
```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip": "192.168.1.100",
  "ntpSynced": true,
  "ntpServer": "pool.ntp.org",
  "currentTime": "14:35:22",
  "currentDate": "19/12/2024",
  "uptime": 3600,
  "freeHeap": "245632"
}
```

### GET `/getprayertimes` - Prayer Times

**Response:**
```json
{
  "imsak": "03:57",
  "subuh": "04:07",
  "terbit": "05:32",
  "zuhur": "11:47",
  "ashar": "15:14",
  "maghrib": "18:01",
  "isya": "19:17"
}
```

### GET `/getcityinfo` - Current City

**Response:**
```json
{
  "selectedCity": "Jakarta",
  "selectedCityApi": "jakarta",
  "latitude": "-6.175392",
  "longitude": "106.827153",
  "hasSelection": true
}
```

### GET `/getmethod` - Calculation Method

**Response:**
```json
{
  "methodId": 5,
  "methodName": "Egyptian General Authority of Survey"
}
```

### GET `/gettimezone` - Timezone Offset

**Response:**
```json
{
  "offset": 7
}
```

### POST `/setwifi` - Configure WiFi

**Body:**
```
ssid=MyWiFi&password=MyPassword123
```

**Response:** `200 OK` or `400 Bad Request`

### POST `/setcity` - Set City

**Body:**
```
city=jakarta&cityName=Jakarta&lat=-6.175392&lon=106.827153
```

**Response:**
```json
{
  "success": true,
  "city": "Jakarta",
  "updating": true
}
```

### POST `/setmethod` - Set Calculation Method

**Body:**
```
methodId=20&methodName=Kementerian Agama Indonesia
```

**Response:**
```json
{
  "success": true,
  "methodId": 20,
  "methodName": "Kementerian Agama Indonesia",
  "prayerTimesUpdating": true
}
```

### POST `/synctime` - Manual Time Sync

**Body:**
```
y=2024&m=12&d=19&h=14&i=35&s=22
```

**Response:** `200 OK` with success message

---

## 🚀 Performance Optimizations

### WiFi Optimizations

**1. Sleep Mode DOUBLE-DISABLED**
```cpp
WiFi.setSleep(WIFI_PS_NONE);           // Arduino API
esp_wifi_set_ps(WIFI_PS_NONE);        // ESP-IDF API
```
**Impact:** Response time < 10ms (was 100-500ms)

**2. Event-Driven Connection**
```cpp
WiFi.onEvent() → FreeRTOS Event Groups
No polling loops → CPU idle when stable
```
**Impact:** 0% CPU usage saat WiFi stable

**3. TX Power Maximum**
```cpp
WiFi.setTxPower(WIFI_POWER_19_5dBm);
esp_wifi_set_max_tx_power(78);
```
**Impact:** Better range & stability

**4. Auto-Reconnect**
```cpp
WiFi.setAutoReconnect(true);
```
**Impact:** No manual intervention needed

### Web Server Optimizations

**1. Pre-allocated Buffers**
```cpp
char jsonBuffer[512];  // Stack allocation
snprintf(jsonBuffer, sizeof(jsonBuffer), ...);
```
**Impact:** No malloc/free overhead

**2. Browser Caching**
```cpp
response->addHeader("Cache-Control", "public, max-age=3600");
```
**Impact:** CSS loads once, subsequent loads instant

**3. Content-Length Header**
```cpp
response->setContentLength(LittleFS.open("/file").size());
```
**Impact:** Browser can show accurate progress

**4. Async Operations**
```cpp
ESPAsyncWebServer → Non-blocking
Multiple clients simultaneously
```
**Impact:** No request blocking

### UI Optimizations

**1. Partial Rendering**
```cpp
lv_display_set_buffers(display, buf, NULL, 
                       sizeof(buf), 
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```
**Impact:** Smooth 20 FPS dengan 10-line buffer

**2. Touch Debouncing**
```cpp
if (now - lastTouchTime < 50) return;  // Skip rapid reads
```
**Impact:** CPU tidak overload dengan touch events

**3. SPI Mutex**
```cpp
xSemaphoreTake(spiMutex, ...) → Exclusive access
```
**Impact:** No TFT/Touch SPI collisions

### Memory Optimizations

**1. Stack vs Heap**
```cpp
// Prefer stack (fast, no fragmentation)
char buffer[256];  // Good

// Avoid heap (slow, fragmentation)
char* buffer = malloc(256);  // Avoid if possible
```

**2. String Optimization**
```cpp
// Use String only when necessary
String city = "Jakarta";  // OK for config

// Use char[] for temporary
char timeStr[10];  // Better for display
sprintf(timeStr, "%02d:%02d", h, m);
```

**3. JSON Memory Management**
```cpp
JsonDocument doc;  // Stack allocation
deserializeJson(doc, payload);  // Parse in-place
// No manual memory management needed
```

### Task Optimizations

**1. Priority Tuning**
```
UI (3)    → Highest (visual responsiveness)
WiFi (2)  → High (network stability)
Web (1)   → Low (background processing)
```

**2. Stack Size Optimization**
```
Monitor via printStackReport():
- Usage < 60%: Optimal
- Usage 60-75%: Good fit
- Usage > 90%: Increase size!
```

**3. Watchdog Integration**
```cpp
esp_task_wdt_add(taskHandle);  // Each task
esp_task_wdt_reset();          // Every loop
```
**Impact:** Auto-recovery dari hang

### Result Metrics

**Before Optimizations:**
- Page load: 2-5 seconds
- WiFi response: 100-500ms
- Touch lag: noticeable
- Memory leaks: yes

**After Optimizations:**
- Page load: 200-500ms (10x faster!)
- WiFi response: < 10ms (50x faster!)
- Touch lag: none (smooth 20 FPS)
- Memory leaks: none (stable for days)

---

## 📁 File Structure

```
jws-indonesia/
├── jws.ino                          # Main program
│   ├── Pin definitions              # GPIO, PWM, SPI config
│   ├── Task configurations          # Stack sizes, priorities
│   ├── Global structures            # WiFiConfig, TimeConfig, etc
│   ├── Function declarations        # Forward declarations
│   ├── Implementation               # All functions
│   └── setup() & loop()             # Arduino entry points
│
├── src/                             # EEZ Studio UI (auto-generated)
│   ├── ui.h                         # LVGL UI initialization
│   ├── ui.cpp                       # UI implementation
│   ├── screens.h                    # Screen objects
│   ├── screens.cpp                  # Screen handlers
│   ├── images.h                     # Image declarations
│   ├── images.cpp                   # Image data (binary)
│   ├── fonts.h                      # Font declarations
│   └── fonts.cpp                    # Font data (binary)
│
├── data/                            # LittleFS filesystem
│   ├── index.html                   # Web interface (50KB)
│   │   ├── Foundation CSS framework
│   │   ├── Loading manager
│   │   ├── Tab navigation
│   │   ├── Form validation
│   │   ├── Countdown system
│   │   └── API integration
│   │
│   ├── assets/
│   │   └── css/
│   │       └── foundation.min.css  # CSS framework (70KB)
│   │
│   └── cities.json                  # 500+ cities (150KB)
│       └── Format:
│           [{
│             "api": "jakarta",
│             "display": "Jakarta (Kota)",
│             "province": "DKI Jakarta",
│             "lat": "-6.175392",
│             "lon": "106.827153"
│           }, ...]
│
└── README.md                        # This file
```

**Runtime Files (Auto-created in LittleFS):**
```
/wifi_creds.txt          → WiFi Router SSID & Password
/ap_creds.txt            → AP SSID, Password, IP, Gateway, Subnet
/prayer_times.txt        → Cached prayer times (7 lines + city)
/city_selection.txt      → Selected city + GPS coordinates
/method_selection.txt    → Calculation method ID & name
/timezone.txt            → UTC offset (-12 to +14)
/buzzer_config.txt       → Toggle states + volume (8 lines)
```

**File Access Patterns:**
- **Read:** On boot (all config files)
- **Write:** On user action via web interface
- **Update:** Prayer times (midnight automatic)

---

## 🛡️ Security Recommendations

### Default Credentials
```
⚠️ CHANGE THESE IMMEDIATELY AFTER FIRST BOOT ⚠️

AP SSID:     JWS Indonesia
AP Password: 12345678
AP IP:       192.168.4.1
```

### Best Practices

**1. Change AP Credentials**
```
Web Interface → Tab WIFI → Section "Access Point"
- Set strong SSID (unique name)
- Set strong password (min 12 chars, mix of A-Z, a-z, 0-9, symbols)
- Click Simpan
```

**2. Secure WiFi Router**
```
✅ WPA2/WPA3 encryption
✅ Strong password (20+ chars)
✅ Disable WPS
✅ Enable MAC filtering (optional)
✅ Hide SSID broadcast (optional)
```

**3. Network Isolation**
```
⚠️ DO NOT expose to public internet
✅ Access only from trusted local network
✅ Consider separate IoT VLAN
✅ Enable router firewall
```

**4. Physical Security**
```
✅ Device in secure location
✅ No exposed USB port (prevent firmware tampering)
✅ Monitor for unauthorized physical access
```

**5. Regular Updates**
```
✅ Check for firmware updates
✅ Update cities.json when available
✅ Monitor security advisories
```

### Known Limitations

**No Authentication:**
- Web interface tidak memiliki login/password
- Anyone dengan network access dapat mengubah settings
- Mitigasi: Network isolation & physical security

**HTTP Only:**
- No HTTPS/SSL encryption
- Data transmitted in plain text
- Mitigasi: Use only on trusted local network

**No OTA:**
- No Over-The-Air firmware updates
- Requires physical USB access untuk update
- Mitigasi: Plan maintenance windows

---

## 🔧 Advanced Configuration

### Custom Cities JSON

**Format Requirements:**
```json
[
  {
    "api": "city_id",           // Required: lowercase, no spaces
    "display": "City Name",      // Required: display text
    "province": "Province Name", // Required: for grouping
    "lat": "-6.175392",         // Required: GPS latitude
    "lon": "106.827153"         // Required: GPS longitude
  }
]
```

**Validation Rules:**
1. JSON must be valid array
2. All fields required (api, display, province, lat, lon)
3. Coordinates must be valid numbers
   - Latitude: -90 to 90
   - Longitude: -180 to 180
4. Max file size: 1MB
5. Max ArduinoJson size: 320KB (calculated)

**Adding Custom City:**
```json
{
  "api": "my_location",
  "display": "My Custom Location (Kota)",
  "province": "My Province",
  "lat": "-6.123456",
  "lon": "106.789012"
}
```

**Testing:**
1. Upload via web interface
2. Check serial monitor for validation
3. Dropdown should auto-reload
4. Select custom city → Save
5. Verify prayer times accuracy

### Custom NTP Servers

**Edit in code:**
```cpp
const char *ntpServers[] = {
  "pool.ntp.org",        // Default: Global pool
  "time.google.com",     // Google time servers
  "time.windows.com"     // Microsoft time servers
};
```

**Add Indonesia-specific:**
```cpp
const char *ntpServers[] = {
  "id.pool.ntp.org",     // Indonesia pool
  "time.bmkg.go.id",     // BMKG (if available)
  "pool.ntp.org"         // Global fallback
};
```

### Custom Backlight Brightness

**Edit in code:**
```cpp
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)
```

**PWM Options:**
- 0: Off (black screen)
- 128: ~50% brightness
- 180: ~70% brightness (default)
- 255: 100% brightness (max)

### Custom Touch Calibration

**If touch coordinates salah:**
```cpp
#define TS_MIN_X 370
#define TS_MAX_X 3700
#define TS_MIN_Y 470
#define TS_MAX_Y 3600
```

**Calibration Procedure:**
1. Enable debug: Uncomment `Serial.printf()` in `my_touchpad_read()`
2. Touch 4 corners, note coordinates
3. Calculate MIN/MAX values
4. Update constants
5. Recompile & upload

### Custom Stack Sizes

**If task crashes (Stack Overflow):**
```cpp
#define UI_TASK_STACK_SIZE 12288    // Increase if needed
#define WEB_TASK_STACK_SIZE 5120    // Increase for large requests
```

**Monitor via:**
```cpp
printStackReport();  // Called every 2 minutes in webTask
```

**Rules:**
- Increase by 2048 (2KB) increments
- Keep total < 200KB (RAM limit)
- Monitor for weeks to ensure stability

---

## 📚 External APIs

### Aladhan Prayer Times API

**Endpoint:**
```
http://api.aladhan.com/v1/timings/{date}
  ?latitude={lat}
  &longitude={lon}
  &method={method_id}
```

**Method IDs:**
- `0` - Shia Ithna-Ashari
- `1` - University of Islamic Sciences, Karachi
- `2` - Islamic Society of North America (ISNA)
- `3` - Muslim World League (MWL)
- `4` - Umm Al-Qura University, Makkah
- `5` - Egyptian General Authority of Survey (Default)
- `7` - Institute of Geophysics, University of Tehran
- `20` - Kementerian Agama Indonesia (Kemenag)

**Response Format:**
```json
{
  "data": {
    "timings": {
      "Fajr": "04:07",
      "Sunrise": "05:32",
      "Dhuhr": "11:47",
      "Asr": "15:14",
      "Maghrib": "18:01",
      "Isha": "19:17",
      "Imsak": "03:57"
    }
  }
}
```

**Rate Limits:**
- No official limit mentioned
- Recommended: Max 1 request per minute
- Auto-update: Once per day (midnight)

**Documentation:**
https://aladhan.com/prayer-times-api

---

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

### Reporting Bugs

1. Check existing issues first
2. Provide detailed information:
   - ESP32 board version
   - Arduino IDE version
   - Library versions
   - Serial monitor output
   - Steps to reproduce

### Suggesting Features

1. Check if already requested
2. Explain use case clearly
3. Provide mockups if UI-related

### Pull Requests

1. Fork repository
2. Create feature branch
3. Follow existing code style
4. Test thoroughly
5. Update documentation
6. Submit PR with clear description

### Code Style

```cpp
// Constants: UPPER_CASE
#define MAX_ATTEMPTS 5

// Variables: camelCase
int reconnectAttempts = 0;

// Functions: camelCase
void updatePrayerDisplay() { }

// Structs: PascalCase
struct WiFiConfig { };

// Comments: Clear & concise
// Check if WiFi connected before API request
if (WiFi.status() == WL_CONNECTED) {
  // ...
}
```

---

## 🙏 Acknowledgments

### Libraries & Frameworks
- **LVGL** - Light and Versatile Graphics Library
- **TFT_eSPI** - Bodmer's excellent TFT library
- **ESPAsyncWebServer** - Asynchronous web server
- **ArduinoJson** - Efficient JSON parsing
- **RTClib** - Adafruit RTC library
- **Foundation CSS** - Responsive framework

### APIs & Services
- **Aladhan API** - Prayer times calculation
- **NTP Pool** - Network time synchronization

### Tools
- **EEZ Studio** - UI design tool
- **Arduino IDE** - Development environment
- **Espressif ESP32** - Amazing microcontroller

---

## 📞 Support

### Community
- **GitHub Issues**: [Report bugs & request features](https://github.com/gonit-dev/jws-indonesia/issues)
- **Discussions**: [Ask questions & share ideas](https://github.com/gonit-dev/jws-indonesia/discussions)

### Documentation
- **Aladhan API Docs**: https://aladhan.com/prayer-times-api
- **LVGL Docs**: https://docs.lvgl.io/
- **ESP32 Docs**: https://docs.espressif.com/

### Useful Links
- **TFT_eSPI Setup**: https://github.com/Bodmer/TFT_eSPI
- **ESP32 Arduino**: https://github.com/espressif/arduino-esp32
- **Foundation CSS**: https://get.foundation/

---
