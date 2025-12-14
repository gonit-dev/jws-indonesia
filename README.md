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
| 📤 **Upload Cities** | Update data kota/kabupaten via web interface (max 1MB) |
| 🌐 **REST API** | Akses data via HTTP endpoint untuk integrasi IoT |
| 🔄 **Auto WiFi Reconnect** | Otomatis reconnect jika koneksi terputus |
| 📊 **Multi-Core FreeRTOS** | Task scheduling optimal di dual-core ESP32 |
| 🔧 **Custom Hostname** | Hostname fixed: `JWS-Indonesia` (tidak dinamis) |
| 🕋 **8 Calculation Methods** | Pilih metode kalkulasi: Kemenag, MWL, Egyptian, ISNA, dll |
| 📍 **Manual Coordinates** | Edit koordinat GPS manual dengan tombol reset ke default JSON |
| 🕐 **Timezone Configuration** | Set timezone manual (UTC-12 hingga UTC+14) |
| 🔄 **Auto NTP Re-sync** | Otomatis re-sync setelah timezone berubah |

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
- ✅ Jam tetap akurat saat mati lampu (dengan baterai CR2032)
- ✅ Auto-load time saat boot (jika valid tahun 2000-2100)
- ✅ Auto-save time setiap NTP sync berhasil

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
✅ AP Started: JWS Indonesia
   Password: 12345678
   AP IP: 192.168.4.1
✅ Hostname Set: JWS Indonesia
========================================
SYSTEM READY!
========================================
```

---

## 🚀 Setup Awal

### 1️⃣ First Boot
```
Device membuat Access Point:
   SSID: "JWS Indonesia"
   Password: "12345678"
   IP: 192.168.4.1
```

### 2️⃣ Hubungkan ke AP
```
1. Cari WiFi "JWS Indonesia" di HP/Laptop
2. Password: 12345678
3. Buka browser: http://192.168.4.1
```

### 3️⃣ Konfigurasi WiFi
```
1. Masukkan SSID & Password WiFi rumah
2. Klik "Simpan"
3. Device langsung terhubung
4. Catat IP baru dari serial monitor
```

### 4️⃣ Pilih Kota & Edit Koordinat (Optional)
```
1. Buka web interface (IP baru)
2. Pilih kota dari dropdown (500+ kota Indonesia)
3. **Form koordinat muncul otomatis:**
   - Koordinat auto-fill dari cities.json
   - Edit manual jika perlu akurasi lebih tinggi
   - Klik "Default" untuk reset ke cities.json
4. (Opsional) Pilih metode kalkulasi:
   - Kemenag Indonesia (ID: 20) - Resmi RI
   - Egyptian (ID: 5) - Default/Klasik
   - 6 metode lainnya tersedia
5. Klik "Simpan Kota"
6. Prayer times auto-update
```

### 5️⃣ Konfigurasi Timezone (Optional)
```
1. Default: UTC+7 (WIB Indonesia)
2. Klik tombol 🕘 di bagian "Auto NTP Sync"
3. Edit offset (contoh: +8 untuk WITA, +9 untuk WIT)
4. Klik 💾 atau tekan Enter
5. NTP akan otomatis re-sync dengan timezone baru

**Timezone untuk Indonesia:**
- WIB (Jawa, Sumatera) = +7
- WITA (Kalimantan, Sulawesi) = +8
- WIT (Papua, Maluku) = +9
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
- ✅ **City Selection**: Dropdown 500+ kota Indonesia dengan koordinat GPS tersimpan
- ✅ **Manual Coordinates Editor**: Edit GPS coordinates dengan tombol reset default
- ✅ **Calculation Method**: Pilih dari 8 metode kalkulasi berbeda
- ✅ **Manual Restart**: Restart device tanpa reset settings
- ✅ **Prayer Times Display**: Subuh, Dzuhur, Ashar, Maghrib, Isya
- ✅ **Manual Time Sync**: Sinkronisasi waktu dari browser
- ✅ **Timezone Configuration**: Set UTC offset manual dengan inline editing
- ✅ **Auto NTP Re-sync**: Trigger otomatis setelah timezone berubah
- ✅ **Upload Cities JSON**: Update database kota (max 1MB)
- ✅ **Factory Reset**: Reset semua pengaturan ke default
- ✅ **Real-time Clock**: Server sync setiap 5 detik + client-side increment
- ✅ **Auto-refresh Status**: Device status update setiap 5 detik

---

## ⏰ Auto-Update System

### Midnight Prayer Update
```cpp
// Setiap hari jam 00:00-00:05
// Auto-fetch prayer times dari Aladhan API
if (currentHour == 0 && currentMinute < 5 && !hasUpdatedToday) {
    getPrayerTimesByCoordinates(latitude, longitude);
}
```

### Hourly NTP Sync
```cpp
// Setiap 1 jam (3600 detik) - berjalan di clockTickTask
// Auto-increment counter, trigger NTP sync saat counter >= 3600
// Counter reset ke 0 setelah sync
if (autoSyncCounter >= 3600) {
    xTaskNotifyGive(ntpTaskHandle);
}
```

### RTC Sync Task
```cpp
// Setiap 1 menit
// Sync system time ← RTC time (jika selisih > 2 detik)
// NTP sync → RTC (setiap NTP berhasil)
```

### NTP Server Fallback
```
Prioritas server NTP:
1. pool.ntp.org
2. id.pool.ntp.org
3. time.google.com
4. time.cloudflare.com
5. time.windows.com
```

### Method-Based Prayer Update
```cpp
// Saat ganti metode kalkulasi via web interface:
// 1. Method disimpan ke LittleFS (/method_selection.txt)
// 2. Prayer times auto-fetch dengan method baru

POST /setmethod → Save method → Auto getPrayerTimesByCoordinates()
```

### Timezone Auto-Apply
```cpp
// Saat timezone berubah via web interface:
// 1. Timezone disimpan ke LittleFS (/timezone.txt)
// 2. Update timeClient.setTimeOffset() dengan offset baru
// 3. Trigger NTP re-sync otomatis
// 4. Apply ke RTC (jika tersedia)

POST /settimezone → Save config → Auto NTP re-sync → Update RTC
```

**Validasi Timezone:**
- Offset valid: -12 hingga +14
- Format input: +7, -5, +9 (dengan tanda +/-)
- Default fallback: +7 (WIB)
````

---

## 📊 System Architecture

### ESP32 Core Usage

| Core | Tasks | Load |
|------|-------|------|
| **Core 0** | WiFi, NTP, Web, Prayer, Clock, RTC Sync | High |
| **Core 1** | UI Task (LVGL rendering) | Dedicated |

**Why Core 1 for UI:**
- Smooth 20 FPS rendering without blocking
- WiFi/network operations tidak ganggu UI
- Touch response lebih responsif

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

### Configuration Storage (LittleFS)

**Persistent Files:**
```cpp
/wifi_creds.txt      // WiFi SSID & Password
/ap_creds.txt        // Access Point credentials
/prayer_times.txt    // Cached prayer times
/city_selection.txt  // Selected city + coordinates
/method_selection.txt // Calculation method ID & name
/cities.json         // Cities database (uploaded)
/timezone.txt        // UTC offset configuration (default: +7)
```

**Auto-save Behavior:**
- WiFi credentials → Saved on change
- City selection → Saved immediately
- Calculation method → Saved immediately, auto-update prayer times
- Prayer times → Auto-saved setiap update dari API

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
1. RTC time (saat boot, jika valid tahun 2000-2100)
2. NTP sync (setiap 1 jam + saat timezone berubah)
3. Manual sync (via web interface)
4. Browser sync (via button "Sync Time Now")

**Timezone Behavior:**
- Timezone tersimpan di `/timezone.txt`
- Apply ke NTP offset: `timezoneOffset * 3600` seconds
- Perubahan timezone trigger auto NTP re-sync
- RTC menyimpan waktu lokal (bukan UTC)

### 🔐 Security Features

**URL Protection:**
- Static assets: Return 404 plain text jika not found
- Random URLs: Redirect ke `/notfound` (HTML 404 page)

### 💡 Display Configuration

### 🕋 Calculation Method Persistence

### 🕐 Timezone Management

**Timezone Storage:**
````cpp
File: /timezone.txt
Line 1: Offset integer (-12 hingga +14)
Example: 7 (untuk UTC+7)
````

**Default Behavior:**
- First boot → UTC+7 (WIB Indonesia)
- User change → Saved to LittleFS + auto NTP re-sync
- Device restart → Load saved timezone dari file
- Factory reset → Reset ke UTC+7

**Timezone Change Flow:**
````
User edit timezone → POST /settimezone
    ↓
Save to /timezone.txt
    ↓
Update timeClient.setTimeOffset(offset * 3600)
    ↓
Trigger NTP re-sync
    ↓
Save synced time to RTC (if available)
    ↓
Update display
````

**Available via:**
- Web interface: Inline editing dengan tombol 🕘
- REST API: GET /gettimezone, POST /settimezone
- Serial monitor: Tampilkan "Timezone loaded: UTC+7"

**Validation:**
- Range: -12 hingga +14 (sesuai standar timezone dunia)
- Format: Integer (dengan tanda +/- di display)
- Input: +7, -5, 0, +14, -12 (valid)
- Auto-validation di server-side

**Keyboard Shortcuts:**
- ESC = Cancel edit mode
- Enter = Save timezone

**Method Storage:**
```cpp
File: /method_selection.txt
Line 1: Method ID (0-20)
Line 2: Method Name (string)
```

**Default Behavior:**
- First boot → Egyptian (ID: 5) - Format klasik Indonesia
- User change → Saved to LittleFS + auto-update prayer times
- Device restart → Load saved method dari file
- Factory reset → Reset ke Egyptian (ID: 5)

**Method Change Flow:**
```
User select method → POST /setmethod
    ↓
Save to /method_selection.txt
    ↓
Auto fetch prayer times with new method
    ↓
Update display
```

**Available via:**
- Web interface: Dropdown "Metode Kalkulasi Jadwal Shalat"
- REST API: GET /getmethod, POST /setmethod

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
| Real-time clock | **5 detik** | Fetch `/devicestatus` |
| Device status | 5 detik | Fetch `/devicestatus` |
| Prayer times | 30 detik | Fetch `/getprayertimes` |

**Note:** Tidak ada client-side increment untuk jam. Waktu selalu fetch dari server setiap 5 detik.

### 🌍 Prayer Time Calculation

**API & Sumber Data:**
- API: Aladhan (https://aladhan.com/v1/timings)
- Koordinat: Dari database `cities.json` (500+ kota)
- Update: Otomatis setiap tengah malam (00:00-00:05 WIB)

**Metode Kalkulasi yang Tersedia:**

| ID | Method Name | Organization | Region |
|----|-------------|--------------|--------|
| **20** | **Kementerian Agama Indonesia** | **Kemenag RI** | **🇮🇩 Indonesia** |
| 3 | Muslim World League | MWL | 🌍 Global |
| **5** | **Egyptian General Authority** | Egypt Survey | 🇪🇬 Egypt *(Default)* |
| 2 | Islamic Society of North America | ISNA | 🇺🇸 North America |
| 0 | Shia Ithna-Ashari | Leva Institute | 🇮🇷 Shia |
| 1 | University of Islamic Sciences | Karachi University | 🇵🇰 Pakistan |
| 4 | Umm Al-Qura University | Makkah University | 🇸🇦 Saudi Arabia |
| 7 | Institute of Geophysics | Tehran University | 🇮🇷 Iran |

**Default Method:**
- Egyptian General Authority (ID: 5)
- Dipilih karena format perhitungan klasik Indonesia
- User bisa ganti via web interface

**Metode Kemenag Indonesia (ID: 20):**
- Metode resmi Kementerian Agama RI
- Direkomendasikan untuk pengguna di Indonesia

**Cara Ganti Metode:**
1. Buka web interface
2. Pilih metode dari dropdown "Metode Kalkulasi"
3. Klik "Simpan Metode"
4. Jadwal shalat otomatis update dengan metode baru

### 📍 GPS Coordinates System

**Current Implementation:**
- Koordinat disimpan dalam `cities.json`
- Format: `{"city":"Jakarta","lat":"-6.2088","lon":"106.8456"}`
- User dapat mengedit koordinat GPS secara manual untuk akurasi yang lebih presisi.
- Akurasi: 4 desimal (±11 meter)

**Flow Penggunaan:**
```
1. Pilih kota dari dropdown
   ↓
2. Form koordinat muncul otomatis
   ↓
3. Koordinat terisi dengan:
   - Data tersimpan (jika sudah pernah edit)
   - Atau default dari cities.json (first time)
   ↓
4. User bisa:
   a) Edit manual → Simpan
   b) Klik "Default" → Kembali ke cities.json → Simpan
```

**Form Components:**

| Field | Description | Validation |
|-------|-------------|------------|
| **Latitude** | Koordinat lintang | -90 hingga 90 |
| **Longitude** | Koordinat bujur | -180 hingga 180 |
| **Tombol Default** | Reset ke koordinat cities.json | - |
| **Info Default** | Menampilkan koordinat original | Read-only |

**Use Cases:**

1. **Koordinat cities.json kurang akurat**
   - Contoh: Jakarta center vs Jakarta Selatan
   - Solusi: Edit manual sesuai GPS smartphone
   
2. **Lokasi spesifik (RT/RW level)**
   - cities.json: Koordinat pusat kota
   - Manual edit: Koordinat rumah/masjid eksak
   
3. **Testing berbagai lokasi**
   - Test prayer times di lokasi berbeda
   - Tombol "Default" untuk reset cepat

**Perbedaan Koordinat:**

| Source | Akurasi | Update | Persistent |
|--------|---------|--------|------------|
| **cities.json** | ±1-5 km | Via upload file | Ya (device) |
| **Manual Edit** | Sesuai input user | Via web form | Ya (LittleFS) |
| **Tombol Default** | Reset ke cities.json | Instant | Setelah Save |


**Cara Dapat Koordinat Akurat:**

1. **Google Maps**
```
   - Buka Google Maps
   - Klik kanan di lokasi
   - Pilih koordinat (otomatis copy)
   - Paste ke form (format: -6.2088, 106.8456)
```

2. **Smartphone GPS**
```
   - Install app "GPS Coordinates"
   - Buka app di lokasi target
   - Copy koordinat decimal degrees
   - Input ke form web interface
```

3. **GPS Module (Future)**
```cpp
   // Bisa dimodifikasi menggunakan GPS NEO-6M/NEO-7M
   // GPS real-time → Auto-update coordinates
```

**Validation Rules:**
```javascript
// Format
- Harus angka (integer atau decimal)
- Gunakan titik (.) bukan koma (,)
- Contoh valid: -6.2088, 106.8456
- Contoh invalid: -6,2088 atau "enam koma dua"

// Range
- Latitude: -90.0000 hingga 90.0000
  - Negatif = Selatan (Indonesia)
  - Positif = Utara
  
- Longitude: -180.0000 hingga 180.0000
  - Positif = Timur (Indonesia: 95-141°)
  - Negatif = Barat
```

**Example Scenarios:**

**Scenario 1: Koordinat Default Akurat**
```
1. User pilih "Jakarta" → Lat: -6.2088, Lon: 106.8456
2. Test prayer times → Cocok dengan masjid lokal
3. Langsung simpan → Selesai
```

**Scenario 2: Perlu Edit Manual**
```
1. User pilih "Bandung" → Lat: -6.9175, Lon: 107.6191 (pusat kota)
2. Lokasi rumah: Lat: -6.9500, Lon: 107.6500 (pinggiran)
3. Edit form → Input koordinat rumah
4. Simpan → Prayer times lebih akurat
```

**Scenario 3: Reset ke Default**
```
1. User sudah edit: Lat: -6.9500, Lon: 107.6500
2. Mau coba koordinat default lagi
3. Klik tombol "Default" → Kembali: -6.9175, 107.6191
4. Simpan → Prayer times pakai koordinat cities.json
```

**Future Enhancement:**
```cpp
// Bisa dimodifikasi menggunakan modul GPS (NEO-6M/NEO-7M)
// GPS → Real-time coordinates → Aladhan API
```

**Notes:**

- ⚠️ Koordinat yang salah = Prayer times tidak akurat
- ✅ Gunakan koordinat masjid terdekat untuk hasil terbaik
- ✅ Akurasi 4 desimal = ±11 meter (cukup untuk prayer times)
- ❌ Jangan gunakan koordinat dari luar Indonesia (validasi server)
- 💡 cities.json tetap original (tidak di-overwrite)

### 📤 Cities JSON Upload

**Validasi:**
- Filename: Harus `cities.json`
- Size: Max 1MB
- Format: Valid JSON array
- Structure: `[{"city":"...","lat":"...","lon":"..."}]`

**Auto-refresh:**
- Dropdown city otomatis reload setelah upload

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
    "cityId": "Jakarta",
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
| `method.id` | number | Calculation method ID (0-20) |
| `method.name` | string | Calculation method name |
| `device.ntpSynced` | boolean | NTP sync status |
| `device.freeHeap` | number | Free RAM (bytes) |
| `timezone` | number | Current UTC offset (-12 hingga +14) |

**Features:**
- ✅ Real-time data (no caching)
- ❌ HTTP only (no HTTPS)

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

**Prayer times tidak akurat dengan masjid lokal**
```
Solusi:
1. Cek koordinat yang digunakan (web interface)
2. Koordinat mungkin pusat kota (kurang akurat)
3. Edit koordinat manual:
   - Buka Google Maps di lokasi masjid
   - Klik kanan → Copy koordinat
   - Paste ke form "Edit Koordinat"
   - Klik "Simpan Kota"
4. Atau ganti metode kalkulasi (8 pilihan)
5. Koordinat akurat = prayer times akurat (±1-2 menit)
```

**Tombol "Default" tidak bekerja**
```
Solusi:
1. Pastikan sudah pilih kota dari dropdown
2. Tombol hanya aktif jika ada kota terpilih
3. Cek console browser (F12) untuk error
4. Refresh halaman dan coba lagi
5. Koordinat default berasal dari cities.json
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

**Web interface tidak bisa diakses (403 Forbidden)**
```
Solusi:
1. Pastikan akses dari browser yang sama yang buka halaman utama
2. Jangan akses endpoint langsung (harus via index.html)
3. Cek Referer header tidak di-block oleh browser/extension
4. Disable browser extension yang mengubah header (privacy tools)
5. Gunakan Incognito/Private mode untuk test
6. Restart browser
```

**Cities dropdown kosong**
```
Solusi:
1. Cek cities.json sudah diupload (LittleFS)
2. File size cities.json < 1MB
3. Format JSON valid (test di jsonlint.com)
4. Re-upload data/ folder
```

**Metode kalkulasi tidak tersimpan setelah restart**
```
Solusi:
1. Cek LittleFS mounted: Serial → "LittleFS Mounted"
2. Cek file saved: Serial → "Method selection saved"
3. Pastikan tidak factory reset setelah set method
4. Test write: Upload cities.json (jika berhasil = LittleFS OK)
5. Ganti metode lagi, tunggu 3 detik, restart manual
```

**Jadwal shalat berbeda dengan masjid lokal**
```
Solusi:
1. Cek metode yang digunakan: Web interface → bagian atas
2. Tanya masjid menggunakan metode apa
3. Ganti metode via dropdown (8 pilihan tersedia)
4. Metode Kemenag (ID: 20) = resmi Indonesia
5. Egyptian (ID: 5) = format lama/klasik Indonesia
6. Koordinat city bisa berbeda beberapa menit
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
# Serial monitor menampilkan event log secara real-time:

[WIFI] Connecting... 5/20 (Disconnected)
✅ WiFi Connected Successfully!
SSID: MyWiFi
IP: 192.168.1.100
RSSI: -45 dBm

✅ NTP Sync successful!
Server: pool.ntp.org
Time: 12:34:56 08/12/2024

📄 Prayer times updated successfully
```

**Timezone tidak tersimpan setelah restart**
````
Solusi:
1. Cek LittleFS mounted: Serial → "LittleFS Mounted"
2. Cek file saved: Serial → "Timezone saved: UTC+X"
3. Pastikan tidak factory reset setelah set timezone
4. Test write: Upload cities.json (jika berhasil = LittleFS OK)
5. Ganti timezone lagi, tunggu 3 detik, restart manual
6. Verifikasi: Cek serial boot → "Timezone loaded: UTC+X"
````

**Waktu tidak sesuai timezone setelah NTP sync**
````
Solusi:
1. Cek timezone yang digunakan: Web interface → bagian Auto NTP Sync
2. Pastikan offset benar (WIB=+7, WITA=+8, WIT=+9)
3. Edit timezone → Klik 💾 → Tunggu auto re-sync
4. Cek serial: "✅ NTP Sync successful! ... Using timezone: UTC+X"
5. Jika masih salah, factory reset dan set ulang
````

**NTP sync berhasil tapi jam masih salah beberapa jam**
````
Solusi:
1. Kemungkinan besar timezone salah
2. Contoh: Set UTC+7 tapi lokasi UTC+8 = selisih 1 jam
3. Verifikasi timezone: Serial → "Using timezone: UTC+X (Y seconds)"
4. Ganti timezone sesuai lokasi sebenarnya
5. Atau gunakan browser sync jika NTP tidak akurat
````

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
│   ├── index.html          # Web interface (responsive, Foundation CSS)
│   ├── assets/
│   │   └── css/
│   │       └── foundation.css  # Zurb Foundation 6.x (minified)
│   └── cities.json         # 500+ cities database (with GPS coordinates)
├── runtime_generated/      # Auto-created by device (DO NOT UPLOAD)
│   ├── wifi_creds.txt      # WiFi credentials (plain text)
│   ├── ap_creds.txt        # AP credentials
│   ├── prayer_times.txt    # Cached prayer times
│   ├── city_selection.txt  # City + lat/lon
│   ├── method_selection.txt # Calculation method
│   └── timezone.txt        # Timezone offset (default: 7)
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
| Rate Limiting | ❌ | No limit (trust local network) |
| Input Validation | ✅ | Server-side validation |

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

