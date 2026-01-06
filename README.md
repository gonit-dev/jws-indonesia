# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan antarmuka web dan backup RTC

[![Version](https://img.shields.io/badge/version-2.2-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/) [![License](https://img.shields.io/badge/license-MIT-yellow)](LICENSE)

---

## 📸 Preview

<div align="center">

<img src="https://github.com/user-attachments/assets/d0e64d2a-2a00-4a62-b92b-9aa36d95f4b8" alt="Home" width="150">
<img src="https://github.com/user-attachments/assets/55d84d62-d986-460a-b2b2-3ac4d2b4aaf1" alt="WiFi" width="150">
<img src="https://github.com/user-attachments/assets/b9f1db1c-83f2-4492-aef8-7b62997da9a9" alt="Time" width="150">
<img src="https://github.com/user-attachments/assets/84408e8e-ef1b-4ee5-aa42-9c0f965bb031" alt="City" width="150">
<img src="https://github.com/user-attachments/assets/6479dfd9-99b9-4034-a0d2-29453d6c46d9" alt="Coords" width="150">
<img src="https://github.com/user-attachments/assets/1f105d28-80a6-490c-a4a9-d7f8174a3c3e" alt="Prayer" width="150">

</div>

---

## ✨ Fitur Utama

### 🕌 Waktu Sholat
- Jadwal sholat otomatis via Aladhan API
- Notifikasi visual (blink 1 menit) dan buzzer
- Toggle buzzer per-waktu sholat (Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya)
- 8 metode kalkulasi (Kemenag, MWL, Egyptian, ISNA, dll)
- 500+ kota Indonesia dengan koordinat GPS akurat
- Edit koordinat manual dengan validasi real-time
- **Touch adzan mode**: Tap waktu sholat saat blink untuk play audio adzan (10 menit)

### ⏰ Manajemen Waktu
- Sinkronisasi NTP otomatis setiap 1 jam
- Multiple NTP server (pool.ntp.org, time.google.com, time.windows.com)
- Backup RTC DS3231 (opsional, sangat disarankan)
- Dukungan zona waktu UTC-12 hingga UTC+14
- Sinkronisasi manual dari browser
- Auto-update jadwal sholat setiap tengah malam (00:00-00:05)

### 🌐 Fitur Jaringan
- WiFi Mode Ganda (AP + STA bersamaan)
- Auto-reconnect event-driven (tanpa polling)
- Pengaturan AP custom (SSID, Password, IP, Gateway, Subnet)
- Monitor koneksi real-time dengan RSSI
- WiFi Sleep Disabled untuk performa maksimal
- Hostname: "JWS-Indonesia"

### 🖥️ Antarmuka
- UI touchscreen LVGL 9.2.0 @ 50ms refresh (20 FPS)
- Web interface responsive Foundation CSS
- Tampilan real-time (jam, tanggal, kota, waktu sholat)
- Loading screen dengan progress bar
- Countdown visual untuk restart/reset (60 detik)

### 🔊 Kontrol Buzzer & Audio
- Toggle per-waktu sholat (7 waktu)
- Kontrol volume 0-100% (PWM buzzer) / 0-30 (DFPlayer)
- Test buzzer manual dengan auto-timeout 30 detik
- Output PWM (GPIO26, Channel 1, 2000 Hz)
- **Audio Adzan via DFPlayer Mini** (opsional)
  - Format MP3 (128-320kbps)
  - Touch waktu sholat untuk play (10 menit timeout)
  - Auto-stop setelah selesai
  - Volume independen dari buzzer (0-30)
  - Files: `/mp3/0001.mp3` hingga `/mp3/0007.mp3`
  - Speaker passive 3-5W (4-8Ω)

### 💾 Penyimpanan
- LittleFS storage untuk semua konfigurasi
- Auto-save setelah perubahan
- Factory reset dengan countdown safety 60 detik
- Persistent adzan state (survive restart)
- Upload cities.json via web (max 1MB, validasi otomatis)

---

## 🔧 Kebutuhan Hardware

### Board: ESP32-2432S024
- **MCU:** ESP32 Dual-Core @ 240MHz
- **RAM:** 520KB SRAM
- **Flash:** 4MB (3MB APP + 1MB SPIFFS)
- **Display:** ILI9341 2.4" TFT (320x240)
- **Touch:** XPT2046 Resistive
- **WiFi:** 802.11 b/g/n (2.4GHz)
- **Power:** 5V USB (minimal 2A)

### Pinout Display & Touch
```
TFT_BL:      GPIO 27 (PWM Backlight, 70% brightness)
TOUCH_CS:    GPIO 33
TOUCH_IRQ:   GPIO 36
TOUCH_MOSI:  GPIO 13
TOUCH_MISO:  GPIO 12
TOUCH_CLK:   GPIO 14
```

### Buzzer
```
BUZZER_PIN:  GPIO 26 (PWM Channel 1, 2000 Hz, 8-bit)
```

### RTC DS3231 (Opsional - Sangat Disarankan)
```
DS3231       ESP32
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21 (I2C)
SCL     →    GPIO 22 (I2C)
```

**Manfaat RTC:**
- Backup waktu saat mati lampu
- Akurasi ±2ppm dengan temperature compensation
- Baterai CR2032 untuk persistensi
- Auto-sync 1 menit sekali (RTC → Sistem)
- Validasi hardware otomatis saat boot

**⚠️ PENTING:** Jika RTC terdeteksi rusak/tidak valid, sistem akan tetap jalan dengan waktu reset ke 01/01/2000 sampai NTP sync berhasil.

### DFPlayer Mini + Speaker (Opsional - Untuk Audio Adzan)

**Wiring:**
```
DFPlayer Mini     ESP32-2432S024
─────────────     ──────────────
VCC          →    5V
GND          →    GND
TX           →    GPIO32 (ESP32 RX)
RX           →    GPIO25 (ESP32 TX)
SPK_1        →    Speaker + (Kanan)
SPK_2        →    Speaker - (Kiri)
DAC_R        →    Line-out audio (AUX Kanan)
DAC_L        →    Line-out audio (AUX Kiri)
```

**Setup Audio:**
1. Format **Micro SD Card** (FAT32, max 32GB)
2. Buat folder `/mp3/` di **root** SD Card
3. Copy file MP3 dengan nama **exact**:
   - `0001.mp3` = Adzan Imsak
   - `0002.mp3` = Adzan Subuh
   - `0003.mp3` = Adzan Terbit (opsional)
   - `0004.mp3` = Adzan Zuhur
   - `0005.mp3` = Adzan Ashar
   - `0006.mp3` = Adzan Maghrib
   - `0007.mp3` = Adzan Isya
4. Masukkan SD Card ke slot DFPlayer
5. Sambungkan speaker **passive** (3-5W, 4-8Ω) ke SPK_1 dan SPK_2
6. **Touch waktu sholat** saat blink untuk play audio (10 menit timeout)

**⚠️ PENTING:**
- **Jangan gunakan speaker aktif** (amplifier built-in) - akan rusak. Sudah ada pin AUX sebagai pengganti
- **File naming harus exact:** `0001.mp3`, `0002.mp3`, dst (4 digit, leading zero)
- **Folder harus `/mp3/`** di root SD Card (bukan `/adzan/` atau yang lain)
- **Format MP3:** 128-320 kbps, Stereo/Mono, Sample rate bebas (DFPlayer auto-detect)
- **Max file size:** 32MB per file
- **Max SD Card:** 32GB (FAT32 only, exFAT tidak support)

**Test Audio:**
1. Cek Serial Monitor saat boot:
```
   ========================================
   INITIALIZING DFPlayer Mini
   ========================================
   UART2: TX=GPIO25, RX=GPIO32
   DFPlayer initialized successfully!
   Files on SD: 7
   ========================================
```
2. Jika muncul "Files on SD: 0" → SD Card tidak terbaca atau folder salah
3. Jika muncul "DFPlayer connection FAILED!" → cek wiring TX/RX

---

## 📦 Instalasi

### 1. Kebutuhan Software

| Komponen | Versi | Wajib | Catatan |
|----------|-------|-------|---------|
| ESP32 Board | v3.0.7 | ✅ | ESP32 Core for Arduino |
| LVGL | 9.2.0 | ✅ | Tidak kompatibel v8.x |
| TFT_eSPI | 2.5.0+ | ✅ | Perlu konfigurasi manual |
| XPT2046_Touchscreen | 1.4+ | ✅ | |
| ArduinoJson | 7.x | ✅ | v6.x tidak kompatibel |
| ESPAsyncWebServer | 3.x (ESP32 v3.x) | ✅ | Pastikan versi match |
| AsyncTCP | Latest | ✅ | Dependency ESPAsync |
| TimeLib | 1.6.1+ | ✅ | |
| RTClib | 2.1.1+ | ✅ | Untuk DS3231 |

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Cari: "esp32"
Install: esp32 by Espressif Systems v3.0.7
```

**⚠️ PENTING:** Gunakan ESP32 Core v3.0.7, bukan v2.x karena ada breaking changes di WiFi API.

### 3. Install Library

Via Library Manager (Sketch → Include Library → Manage Libraries), install semua library di tabel kebutuhan di atas.

**Catatan ArduinoJson:**
- Gunakan v7.x (bukan v6.x)
- Kode menggunakan `JsonDocument` tanpa size template

### 4. Konfigurasi TFT_eSPI

Edit file `Arduino/libraries/TFT_eSPI/User_Setup_Select.h`:
```cpp
// Comment semua setup default
// #include <User_Setup.h>

// Uncomment setup untuk ESP32-2432S024
#include <User_Setups/Setup24_ST7789.h>  // atau sesuai board
```

Edit `User_Setup.h` atau file setup board:
```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH  320
#define TFT_HEIGHT 240
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1  // Connected to RST pin
#define TFT_BL   27  // Backlight control
```

### 5. Generate UI dengan EEZ Studio

Proyek ini menggunakan **EEZ Studio** untuk desain UI LVGL.

**Cara Generate:**
1. Download EEZ Studio: https://github.com/eez-open/studio
2. Buka file proyek `.eez-project` (jika ada)
3. Build → Generate Code for LVGL
4. Copy folder `src/` (ui.c, ui.h, screens.c, dll) ke folder sketch

**File UI yang dibutuhkan:**
```
sketch_folder/
  jws.ino
  src/
    ui.c
    ui.h
    screens.c
    screens.h
    images.c
    images.h
    fonts/
      font_*.c
```

**⚠️ PENTING:** File `src/*` tidak included di repository. Anda harus generate sendiri dari EEZ Studio atau request dari pembuat.

### 6. Upload Filesystem (LittleFS)

**Install Plugin:**
1. Download: https://github.com/lorol/arduino-esp32littlefs-plugin/releases
2. Extract ke `Arduino/tools/`
3. Restart Arduino IDE

**Upload Data:**
```
1. Buat folder 'data/' di root sketch
2. Copy files:
   - index.html
   - assets/css/foundation.min.css
   - cities.json
3. Tools → ESP32 Sketch Data Upload
```

**⚠️ CATATAN:** Upload filesystem dulu sebelum upload sketch!

### 7. Upload Sketch

**Pengaturan Board:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
Core Debug Level: None (atau Error untuk debugging)
```

**Compile & Upload:**
```
Sketch → Upload (Ctrl+U)
```

**Monitor Serial:**
```
Tools → Serial Monitor
Baud Rate: 115200
```

---

## 🚀 Panduan Cepat

### Langkah 1: Boot Pertama
```
AP SSID: "JWS Indonesia"
Password: "12345678"
URL: http://192.168.4.1
```

**Serial Monitor akan menampilkan:**
```
========================================
ESP32 Islamic Prayer Clock
LVGL 9.2.0 + FreeRTOS
VERSION 2.2 - STACK OPTIMIZED
========================================
...
AP Started: JWS Indonesia
AP IP: 192.168.4.1
========================================
SYSTEM READY
========================================
```

### Langkah 2: Konfigurasi WiFi
1. Sambungkan ke AP "JWS Indonesia"
2. Buka browser → `http://192.168.4.1`
3. Tab **WIFI** → Masukkan SSID dan Password WiFi rumah
4. Klik **Simpan** → Tunggu koneksi (~15-30 detik)

**Monitor koneksi di Serial:**
```
WiFi: Connected | RSSI: -45 dBm | IP: 192.168.1.100
NTP SYNC COMPLETED (UTC)
APPLYING TIMEZONE OFFSET (UTC+7)
```

### Langkah 3: Atur Lokasi & Metode
1. Tab **LOKASI** → Pilih provinsi → Pilih kota
2. Verifikasi/edit koordinat (opsional)
   - Tombol **Reset** = kembali ke koordinat default dari cities.json
   - Tombol **Batal** = batalkan edit koordinat
3. Pilih metode kalkulasi (default: Egyptian General Authority)
4. Klik **Simpan** → Jadwal sholat akan update otomatis

**Monitor update di Serial:**
```
========================================
Save City Selection
========================================
City: Jakarta (Kota)
Coordinates: -6.2088, 106.8456
========================================
PRAYER TASK: Processing Update
========================================
Fetching prayer times...
Prayer times updated successfully
```

### Langkah 4: Konfigurasi Zona Waktu
```
Default: UTC+7 (WIB)

Zona Waktu Indonesia:
- WIB:  +7  (Jawa, Sumatera, Kalimantan Barat)
- WITA: +8  (Kalimantan Tengah/Timur, Sulawesi, Bali, NTB, NTT)
- WIT:  +9  (Papua, Maluku)
```

Tab **WAKTU** → Klik ikon edit (🕐) → Input offset → Klik 💾 atau tekan Enter

**Auto-trigger NTP & Prayer Update:**
```
AUTO-TRIGGERING NTP RE-SYNC
Reason: Timezone changed to UTC+8
NTP Task will automatically:
  1. Sync time with new timezone
  2. Update prayer times with correct date
```

### Langkah 5: Konfigurasi Buzzer & Audio
1. Tab **JADWAL** → Toggle ON/OFF untuk setiap waktu sholat
2. Atur slider volume (0-100%)
3. Klik **Test Buzzer** untuk tes suara (auto-stop 30 detik)
4. **Opsional - Setup Audio Adzan:**
   - Format SD Card (FAT32)
   - Buat folder `/adzan/`
   - Copy file WAV: `subuh.wav`, `zuhur.wav`, `ashar.wav`, `maghrib.wav`, `isya.wav`
   - Reboot perangkat
   - **Cara play:** Saat waktu sholat tiba dan label blink, **tap label waktu sholat** (misal tap "SUBUH") → audio akan play otomatis

---

## 🌐 Antarmuka Web

### Tab BERANDA
- **Status Perangkat:**
  - Jaringan WiFi dan IP address
  - Status internet (Terhubung/Tidak)
  - Status NTP dan server yang digunakan
  - Waktu dan tanggal real-time
  - Uptime perangkat
- **Tombol:**
  - Mulai Ulang Perangkat (countdown 60 detik)

### Tab WIFI
- **WiFi Router:**
  - SSID dan Password
  - Tombol Simpan & Batal
- **Access Point:**
  - SSID dan Password (minimal 8 karakter)
  - Tombol Simpan & Batal
- **Konfigurasi Jaringan AP:**
  - IP Address (default: 192.168.4.1)
  - Gateway (default: 192.168.4.1)
  - Subnet Mask (default: 255.255.255.0)
  - Validasi format IP otomatis
  - Tombol Simpan & Batal

**⚠️ PENTING:** 
- Restart WiFi/AP menggunakan countdown 60 detik untuk keamanan
- Client yang terhubung akan di-redirect otomatis ke IP baru

### Tab WAKTU
- **Pembaruan Manual:**
  - Sync waktu dari browser (timezone-aware)
  - Tombol "Perbarui Waktu"
- **Pembaruan Otomatis (NTP):**
  - Auto-sync setiap 1 jam
  - Multiple fallback server
  - Zona waktu: UTC-12 hingga UTC+14 (inline edit)
  - Tombol edit (🕐) → Simpan (💾) → Batal (✖)
  - **Auto-trigger:** Saat zona waktu berubah, NTP dan prayer times auto-update

### Tab LOKASI
- **Pilih Lokasi:**
  - Dropdown 500+ kota Indonesia
  - Grouping per-provinsi
  - Menampilkan tipe lokasi (Kota/Kabupaten/Kecamatan/Kelurahan)
  - Koordinat GPS ditampilkan di dropdown
- **Edit Koordinat:**
  - Manual edit latitude & longitude
  - Validasi range otomatis (Lat: -90 to 90, Lon: -180 to 180)
  - Tombol Reset (kembali ke koordinat default)
  - Tombol Batal (batalkan edit)
- **Metode Kalkulasi:**
  - 8 pilihan metode (Kemenag, MWL, Egyptian, ISNA, dll)
  - Default: Egyptian General Authority of Survey
  - Auto-update jadwal saat ganti metode
- **Upload Cities.json:**
  - Drag & drop atau pilih file
  - Validasi otomatis (format, size, required fields)
  - Progress bar upload
  - Max size: 1MB
  - Auto-refresh dropdown setelah upload

### Tab JADWAL
- **Waktu Sholat:**
  - Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya
  - Toggle buzzer per-waktu (ON/OFF switch)
  - **Notifikasi visual:** Label blink 1 menit saat waktu masuk
  - **Audio adzan:** Tap label saat blink (10 menit timeout)
- **Kontrol Buzzer:**
  - Volume slider (0-100%)
  - Real-time preview saat drag
  - Tombol "Test Buzzer" (play/stop manual, auto-timeout 30s)
- **Info Tambahan:**
  - Metode kalkulasi yang digunakan
  - Auto-update setiap tengah malam
  - Auto-update saat WiFi connect (jika ada koordinat)

### Tab RESET
- **Factory Reset:**
  - Countdown 60 detik sebelum eksekusi
  - Visual progress bar
  - Auto-redirect ke IP default (192.168.4.1) setelah selesai
- **Data yang Dihapus:**
  - Kredensial WiFi
  - Konfigurasi Access Point
  - Jadwal sholat tersimpan
  - Lokasi yang dipilih
  - Metode kalkulasi
  - Zona waktu
  - Konfigurasi buzzer
  - Adzan state
- **Kembali ke Default:**
  - AP SSID: "JWS Indonesia"
  - AP Password: "12345678"
  - AP IP: 192.168.4.1
  - Timezone: UTC+7 (WIB)
  - Waktu: 01/01/2000 00:00:00

---

## 🔍 Troubleshooting

### WiFi Tidak Connect

**Penyebab:**
- SSID/Password salah (case-sensitive)
- Router hanya 5GHz (ESP32 hanya support 2.4GHz)
- MAC filtering aktif di router
- Sinyal terlalu lemah
- Channel WiFi tidak didukung (coba channel 1-11)

**Solusi:**
1. Cek SSID dan password (perhatikan huruf besar/kecil)
2. Pastikan router broadcast di 2.4GHz
3. Tambahkan MAC address ESP32 ke whitelist router
4. Dekatkan perangkat ke router
5. Ganti channel WiFi router ke 1-11
6. Factory reset → konfigurasi ulang

**Debug Serial Monitor:**
```
WiFi: Connected | RSSI: -45 dBm | IP: 192.168.1.100  → ✅ OK
WiFi: Connected | RSSI: -75 dBm | IP: 192.168.1.100  → ⚠️ Lemah
WiFi Disconnected | Reason Code: 15                  → ❌ Gagal
```

**Interpretasi RSSI:**
- RSSI > -50 dBm: Sangat baik (Excellent)
- RSSI -50 to -60 dBm: Baik (Good)
- RSSI -60 to -70 dBm: Cukup (Fair)
- RSSI < -70 dBm: Lemah (Weak) - terlalu jauh

---

### Jadwal Sholat Tidak Akurat

**Penyebab:**
- Koordinat GPS tidak tepat untuk lokasi Anda
- Metode kalkulasi tidak sesuai daerah
- Zona waktu salah

**Solusi:**
1. **Edit Koordinat GPS Manual:**
   - Buka Google Maps
   - Klik lokasi rumah/masjid
   - Klik koordinat di bagian bawah layar
   - Salin latitude & longitude (format: -6.2088, 106.8456)
   - Paste ke Tab LOKASI → Edit Koordinat
   - Klik **Simpan**

2. **Ganti Metode Kalkulasi:**
   - Tab LOKASI → Dropdown Metode
   - Coba: **Kementerian Agama Indonesia** (Metode 20) → paling akurat untuk Indonesia
   - Atau: **Egyptian General Authority** (Metode 5) → alternatif bagus
   - Klik **Simpan**

3. **Cek Zona Waktu:**
   - Tab WAKTU → Pastikan zona waktu sesuai lokasi
   - WIB = +7, WITA = +8, WIT = +9

4. **Bandingkan:**
   - Download app seperti Muslim Pro atau Adzan
   - Bandingkan dengan jadwal masjid terdekat
   - Adjust koordinat jika perlu (±0.01° = ±1km)

**Debug Serial Monitor:**
```
Fetching prayer times...
Date: 19-12-2024
URL: http://api.aladhan.com/v1/timings/...
Response code: 200                          → ✅ OK
Prayer times updated successfully           → ✅ Berhasil

Prayer times updated successfully
City: Jakarta
Subuh: 04:07  Zuhur: 11:54  Ashar: 15:14
```

---

### Jam Reset ke 01/01/2000

**Penyebab:**
- Belum ada sinkronisasi NTP (WiFi belum connect)
- RTC DS3231 tidak terpasang
- Baterai RTC habis/mati
- RTC hardware failure

**Solusi:**

**1. Sambungkan ke WiFi:**
```
Serial Monitor:
AUTO NTP SYNC STARTED
NTP SYNC COMPLETED (UTC)
UTC Time: 14:35:22 19/12/2024
APPLYING TIMEZONE OFFSET (UTC+7)
Time saved to system memory
```
Waktu akan akurat dalam 10-30 detik setelah WiFi connect.

**2. Pasang RTC DS3231:**
```
Wiring:
DS3231 VCC → ESP32 3.3V
DS3231 GND → ESP32 GND
DS3231 SDA → ESP32 GPIO21
DS3231 SCL → ESP32 GPIO22
Baterai CR2032 → Slot baterai RTC
```

Reboot, cek Serial Monitor:
```
========================================
INITIALIZING DS3231 RTC
========================================
DS3231 detected on I2C               → ✅ OK
RTC hardware test PASSED              → ✅ OK
RTC battery backup is good            → ✅ OK
Time loaded from RTC successfully
```

**3. Jika RTC Rusak:**
```
Serial Monitor:
*** RTC HARDWARE FAILURE ***
DS3231 chip is defective!
Time registers return garbage data
>>> SOLUTION: BUY NEW DS3231 MODULE <<<
```
**Tidak bisa diperbaiki - ganti modul baru.**

**4. Temporary Fix (Tanpa RTC):**
- Tab WAKTU → Tombol "Perbarui Waktu" (sync dari browser)
- Atau: Tunggu WiFi connect → NTP auto-sync

**⚠️ CATATAN:** Tanpa RTC, waktu akan reset setiap restart/mati lampu.

---

### Waktu Sholat Tidak Update Tengah Malam

**Pemeriksaan Serial Monitor (00:00-00:05):**

**✅ NORMAL:**
```
========================================
MIDNIGHT DETECTED - STARTING SEQUENCE
========================================
Time: 00:01:15
Date Now: 20/12/2024

Triggering NTP Sync...
Reason: Ensuring time is accurate before updating
NTP sync triggered successfully
========================================

NTP SYNC COMPLETED
New Time: 00:02:30
New Date: 20/12/2024

Updating Prayer Times...
City: Jakarta
Coordinate: -6.2088, 106.8456
Prayer times updated successfully
```

**❌ TIDAK UPDATE - Penyebab:**

**1. WiFi Tidak Connect:**
```
MIDNIGHT DETECTED - STARTING SEQUENCE
ERROR: NTP Task handle NULL         → WiFi belum connect
Skipping midnight update
```
**Solusi:** Cek koneksi WiFi di Tab BERANDA.

**2. Kota Belum Dipilih:**
```
MIDNIGHT DETECTED - STARTING SEQUENCE
NTP SYNC COMPLETED
WARNING: No city coordinates
Using existing prayer times            → Tidak ada lokasi
```
**Solusi:** Tab LOKASI → Pilih kota → Simpan.

**3. NTP Timeout:**
```
MIDNIGHT DETECTED - STARTING SEQUENCE
Waiting for NTP sync to complete...
NTP SYNC TIMEOUT                       → NTP gagal
Decision: Use existing prayer times
```
**Solusi:** Cek koneksi internet, retry auto di 00:01, 00:02, dst.

**Manual Update:**
- Tab LOKASI → Klik **Simpan** (akan force update)
- Atau: Restart perangkat saat WiFi sudah connect

---

### Antarmuka Web Lambat/Timeout

**Penyebab:**
- WiFi sleep aktif (seharusnya disabled)
- Sinyal lemah
- Multiple client bersamaan (>3)
- Cache browser lama
- Router bandwidth penuh

**Solusi:**

**1. Cek WiFi Sleep di Serial Monitor (saat boot):**
```
========================================
WIFI CONFIGURATION
========================================
WiFi Sleep: DOUBLE DISABLED
  Arduino: WIFI_PS_NONE              → ✅ OK
  ESP-IDF: WIFI_PS_NONE              → ✅ OK
```
Jika sleep aktif, ada bug di kode - report ke developer.

**2. Dekatkan Perangkat:**
- RSSI harus > -60 dBm untuk performa optimal
- Pindah lebih dekat ke router/AP

**3. Hapus Cache Browser:**
```
Chrome: Ctrl+Shift+Delete → Pilih "Cached images and files"
Firefox: Ctrl+Shift+Delete → Pilih "Cache"
Edge: Ctrl+Shift+Delete → Pilih "Cached data"
```

**4. Mode Incognito/Private:**
```
Chrome: Ctrl+Shift+N
Firefox: Ctrl+Shift+P
Edge: Ctrl+Shift+P
```

**5. Ganti Browser:**
- Chrome → Firefox
- Firefox → Edge
- Mobile browser → Desktop browser

**6. Restart Perangkat:**
- Tab BERANDA → Tombol "Mulai Ulang Perangkat"
- Atau: Serial Monitor → ketik `restart` (jika ada command parser)

**7. Batasi Client:**
- Max 3 client bersamaan untuk performa optimal
- Disconnect client yang tidak dipakai

**Debug Stack & Memory:**
```
Serial Monitor (otomatis tiap 2 menit):

========================================
STACK USAGE ANALYSIS
========================================
UI        :  8521/12288 (69.3%) [Free:  3767] FIT
Web       :  3024/ 5120 (59.1%) [Free:  2096] OPTIMAL
WiFi      :  1887/ 3072 (61.4%) [Free:  1185] OPTIMAL
NTP       :  2456/ 4096 (60.0%) [Free:  1640] OPTIMAL
Prayer    :  9887/16384 (60.3%) [Free:  6497] OPTIMAL → ✅ Sehat
Clock     :  1024/ 2048 (50.0%) [Free:  1024] OPTIMAL
========================================

MEMORY STATUS:
Current:  245632 bytes (239.88 KB)
Lowest:   243520 bytes (238.00 KB)
Highest:  247104 bytes (241.31 KB)
Peak Usage:  3584 bytes (3.50 KB)
Memory status: Normal
========================================
```

**Jika ada LEAK:**
```
LEAK DETECTED: 1024 bytes lost since last check  → ⚠️ Memory leak!
```
**Solusi:** Restart perangkat, report ke developer dengan log lengkap.

---

### Touch Tidak Responsif

**Penyebab:**
- Layar kotor/berminyak
- Kalibrasi touch tidak tepat
- Konflik pin dengan audio (GPIO33)

**Solusi:**
1. **Bersihkan layar** dengan kain microfiber lembab
2. **Sentuh lebih kuat** (resistive touch butuh tekanan)
3. **Cek konflik pin:**
   ```
   Serial Monitor saat boot:
   Touch initialized               → ✅ OK
   ```

4. **Kalibrasi manual** (edit di kode jika perlu):
   ```cpp
   // Baris 83-86 di jws.ino
   #define TS_MIN_X 370
   #define TS_MAX_X 3700
   #define TS_MIN_Y 470
   #define TS_MAX_Y 3600
   ```
   Adjust nilai sesuai hasil test touch Anda.

---
### Audio Adzan Tidak Play (DFPlayer Mini)

**Pemeriksaan Hardware Boot:**
```
Serial Monitor saat boot:
========================================
INITIALIZING DFPlayer Mini
========================================
UART2: TX=GPIO25, RX=GPIO32
DFPlayer initialized successfully!
Volume: 15/30
Files on SD: 7
========================================

// Jika gagal:
DFPlayer connection FAILED!
Check wiring:
  ESP32 TX (GPIO25) → DFPlayer RX
  ESP32 RX (GPIO32) → DFPlayer TX
```

**Troubleshooting Checklist:**

1. **Cek Wiring:**
```
   DFPlayer TX  → ESP32 GPIO32 (RX)  ✅ Harus silang!
   DFPlayer RX  → ESP32 GPIO25 (TX)  ✅ Harus silang!
   DFPlayer VCC → ESP32 5V
   DFPlayer GND → ESP32 GND
```
   **⚠️ PENTING:** TX-RX harus **SILANG** (tidak sama-sama TX atau RX)

2. **Cek SD Card:**
   - Format: **FAT32** (bukan exFAT/NTFS)
   - Size: Max 32GB
   - Folder: `/mp3/` di **root** (huruf kecil semua)
   - Files: `0001.mp3`, `0002.mp3`, hingga `0007.mp3`
   - File naming: **4 digit** dengan leading zero

3. **Test Manual:**
```
   Serial Monitor → Ketik:
   AT+VOL=15    // Set volume ke 15
   AT+PLAY=1    // Play file 0001.mp3
```
   Jika tidak ada command parser, cek files dengan:
```
   Files on SD: 7  → ✅ SD Card OK
   Files on SD: 0  → ❌ SD Card error
```

4. **Cek Speaker:**
   - Tipe: **Passive speaker** (tanpa amplifier)
   - Power: 3-5W
   - Impedance: 4-8Ω
   - **JANGAN** gunakan speaker aktif (akan rusak!)

5. **Cara Play Audio:**
```
   1. Tunggu waktu sholat masuk
   2. Label akan BLINK (kedip-kedip) selama 1 menit
   3. TAP AREA LABEL saat blink (misal tap "SUBUH")
   4. Audio akan play dari DFPlayer
```

6. **Debug Serial Monitor:**
```
   ✅ NORMAL:
   ADZAN AKTIF: subuh (10 menit)
   TOUCH ADZAN: subuh
   Audio system available - triggering playback
   ========================================
   PLAYING ADZAN: subuh
   ========================================
   Track: 2
   File: /mp3/0002.mp3
   ========================================
   DFPlayer stopped
   Adzan playback completed
   
   ❌ ERROR:
   WARNING: Audio system not available
   Reason: SD Card not detected or audio disabled
```

7. **Timeout System:**
   - Audio dapat di-play selama **10 menit** setelah waktu sholat masuk
   - Setelah 10 menit, touch tidak berfungsi (auto-expire)

8. **Volume Control:**
   - Buzzer volume (Tab JADWAL): **Tidak mempengaruhi** DFPlayer
   - DFPlayer volume: Hardcoded 15/30 di kode (edit jika perlu)
```cpp
   // Baris di initDFPlayer()
   dfPlayer.volume(15);  // 0-30 (ubah sesuai kebutuhan)
```

**Jika masih tidak berfungsi:**
- Ganti DFPlayer Mini (kemungkinan modul rusak)
- Test dengan sketch sederhana (DFPlayer example dari library)
- Cek Serial Monitor untuk error message detail
---

### Buzzer Tidak Bunyi

**Pemeriksaan:**
- Toggle buzzer diaktifkan (Tab JADWAL)
- Volume tidak 0%
- Pin GPIO26 tidak konflik

**Debug Serial Monitor:**
```
PRAYER TIME ENTER: SUBUH
Starting to blink for 1 minute...
```
Buzzer harusnya ON/OFF setiap 500ms (blink interval).

**Solusi:**
1. **Test buzzer manual:**
   - Tab JADWAL → Tombol "Test Buzzer"
   - Harus bunyi beep-beep selama 30 detik
   
2. **Cek wiring GPIO26:**
   ```
   Buzzer (+) → GPIO26
   Buzzer (-) → GND
   ```
   
3. **Cek output PWM:**
   ```cpp
   Serial Monitor:
   Buzzer initialized (GPIO26)    → ✅ Init OK
   ```
   
4. **Ganti buzzer** jika rusak (tes dengan multimeter/LED)

5. **Adjust frekuensi** jika buzzer tidak cocok:
   ```cpp
   // Baris 94 di jws.ino
   #define BUZZER_FREQ 2000  // Coba 1000-4000 Hz
   ```

---

### Display Flicker/Tearing

**Penyebab:**
- Power supply tidak cukup (<2A)
- Kabel USB jelek (voltage drop)
- SPI speed terlalu tinggi

**Solusi:**
1. **Gunakan adaptor 5V 2A** atau lebih (bukan USB PC)
2. **Kabel USB pendek** (<1 meter, kualitas bagus)
3. **Tambah kapasitor:**
   ```
   100-470µF elektrolit di VIN (5V) dan GND
   10µF ceramic di 3.3V dan GND
   ```
4. **Turunkan SPI clock** (edit TFT_eSPI):
   ```cpp
   #define SPI_FREQUENCY  27000000  // Coba 20MHz jika masih flicker
   ```

---

### Audio Adzan Tidak Play

**⚠️ PENTING - Konflik Pin:**
```cpp
// GPIO33 digunakan untuk 2 fungsi:
#define TOUCH_CS 33   // Touch chip select
#define I2S_DOUT 33   // Audio data out

// Ini akan menyebabkan crash saat audio play!
```

**Solusi Sementara:**
1. **Ganti I2S_DOUT** ke GPIO lain (edit kode):
   ```cpp
   // Baris 64 di jws.ino
   #define I2S_DOUT 26  // Atau GPIO lain yang tidak terpakai
   ```
2. Recompile & upload

**Pemeriksaan Standard:**

**1. Cek Hardware Boot:**
```
Serial Monitor saat boot:
I2S OK                              → ✅ I2S initialized
SD OK                               → ✅ SD Card detected
Audio Task OK                       → ✅ Task created

// Jika gagal:
SD FAIL                             → ❌ SD Card not detected
Audio DISABLED                      → ❌ System disabled
```

**2. Cek File Audio:**
- Format SD Card: **FAT32** (bukan exFAT/NTFS)
- Folder: `/adzan/` di root SD Card
- Files: `subuh.wav`, `zuhur.wav`, `ashar.wav`, `maghrib.wav`, `isya.wav`
- Format WAV: **44.1kHz, 16-bit, Stereo atau Mono**

**3. Cara Play Audio:**
```
1. Tunggu waktu sholat masuk
2. Label akan BLINK (kedip-kedip) selama 1 menit
3. TAP AREA LABEL saat blink (misal tap area "SUBUH")
4. Audio akan play otomatis
```

**Debug Serial Monitor:**
```
✅ NORMAL:
ADZAN AKTIF: subuh (10 menit)
TOUCH ADZAN: subuh
Audio system available - triggering playback
Playing audio: /adzan/subuh.wav
PLAYING: /adzan/subuh.wav
AUDIO DONE
Adzan state cleared

❌ ERROR:
WARNING: Audio system not available
Reason: SD Card not detected or audio disabled
Action: Clearing adzan state immediately
```

**4. Timeout System:**
- Audio dapat di-play selama **10 menit** setelah waktu sholat masuk
- Setelah 10 menit, touch tidak berfungsi (auto-expire)

**5. Format WAV:**
```bash
# Convert MP3 ke WAV dengan ffmpeg:
ffmpeg -i input.mp3 -ar 44100 -ac 2 -sample_fmt s16 output.wav

# Parameter:
-ar 44100       : Sample rate 44.1kHz
-ac 2           : Stereo (atau -ac 1 untuk mono)
-sample_fmt s16 : 16-bit signed integer
```

---

### RTC Time Invalid / Garbage Data

**Serial Monitor:**
```
*** RTC HARDWARE FAILURE ***
DS3231 chip is defective!
Time registers return garbage data
Temperature sensor works: 25.50°C

Possible causes:
  1. Counterfeit/clone DS3231 chip
  2. Crystal oscillator failure
  3. Internal SRAM corruption

>>> SOLUTION: BUY NEW DS3231 MODULE <<<
```

**Ini adalah hardware failure - tidak bisa diperbaiki!**

**Solusi:**
1. **Beli modul RTC DS3231 baru** (pastikan original, bukan clone)
2. **Atau:** Lepas RTC, sistem tetap jalan dengan NTP

**Tanpa RTC:**
- Waktu reset ke 01/01/2000 setiap restart
- NTP akan sync otomatis saat WiFi connect
- Jadwal sholat tetap update normal
- Prayer Task tetap jalan

---

### Prayer Task Crash / Stack Overflow

**Serial Monitor:**
```
========================================
CRITICAL: PRAYER TASK CRASHED
========================================
Detected state: DELETED
Action: Auto-restarting task...
========================================

Prayer Task restarted successfully
Stack: 16384 bytes
WDT: Re-registered
```

**Sistem punya auto-recovery - task akan restart otomatis!**

**Jika crash berulang:**
```
Prayer API] Stack available: 1800 bytes  → ⚠️ Terlalu kecil!
ERROR: Insufficient stack for HTTP request!
Aborting prayer times update to prevent crash
```

**Solusi:**
1. **Increase stack size** (edit kode):
   ```cpp
   // Baris 100 di jws.ino
   #define PRAYER_TASK_STACK_SIZE 20480  // Dari 16384 → 20480
   ```
2. Recompile & upload

**Monitor stack usage:**
```
[Prayer Task] Stack free: 6497 bytes  → ✅ Sehat (>6000)
[Prayer Task] Stack free: 1024 bytes  → ⚠️ Kritis (<2000)
```

---

### Upload cities.json Gagal

**Error di Browser:**
- "Invalid filename" → Nama file harus **exact** `cities.json`
- "File too large" → Max 1MB
- "Invalid JSON" → Format JSON rusak

**Validasi Manual:**
1. **Cek nama file:** Harus `cities.json` (lowercase, no space)
2. **Cek size:** Max 1MB (1,048,576 bytes)
3. **Validate JSON:**
   - Online: https://jsonlint.com/
   - Atau: `python -m json.tool cities.json`

**Format Required:**
```json
[
  {
    "api": "jakarta",
    "display": "Jakarta (Kota)",
    "province": "DKI Jakarta",
    "lat": "-6.2088",
    "lon": "106.8456"
  },
  ...
]
```

**Required fields:** `api`, `display`, `province` (wajib ada dan tidak boleh kosong)
**Optional fields:** `lat`, `lon` (boleh kosong, tapi tidak akan bisa hitung jadwal)

**Debug Upload:**
```
Serial Monitor:
========================================
CITIES.JSON UPLOAD STARTED
========================================
Filename: cities.json
Writing to LittleFS...
Progress: 5120 bytes (5.0 KB)
Progress: 10240 bytes (10.0 KB)
...
Upload complete
Total size: 45632 bytes (44.56 KB)
Duration: 2345 ms
File verified: 45632 bytes
JSON format looks valid
========================================
```

---

## 🛡️ Keamanan

### Kredensial Default
```
⚠️ SEGERA GANTI SETELAH BOOT PERTAMA ⚠️

AP SSID:     JWS Indonesia
AP Password: 12345678
AP IP:       192.168.4.1
```

### Rekomendasi Keamanan
1. **Ganti SSID dan password AP** dengan yang kuat:
   - SSID: Hindari nama yang menyebut "ESP32" atau "JWS"
   - Password: Minimal 12 karakter, kombinasi huruf/angka/simbol
   
2. **Gunakan WiFi router dengan WPA2/WPA3**
   
3. **Jangan ekspos ke internet publik:**
   - Port forwarding DISABLED
   - Hanya akses dari jaringan lokal (LAN/WiFi rumah)
   
4. **Pertimbangkan VLAN IoT terpisah:**
   - Isolasi perangkat IoT dari network utama
   - Firewall rules untuk restrict traffic
   
5. **Monitor akses fisik:**
   - Letakkan di tempat yang tidak mudah diakses
   - Serial port bisa digunakan untuk debug/hack
   
6. **Backup konfigurasi:**
   - Screenshot semua setting di web interface
   - Catat koordinat GPS dan metode kalkulasi
   
7. **Update firmware berkala:**
   - Check repository untuk update security patches
   - Test di development board dulu sebelum production

---

## 📊 Arsitektur Sistem

### FreeRTOS Tasks

| Task | Core | Priority | Stack (KB) | Refresh | Fungsi |
|------|------|----------|------------|---------|--------|
| **UI** | 1 | 3 (High) | 12 | 50ms | LVGL rendering, touch handler |
| **WiFi** | 0 | 2 (High) | 3 | Event | Connection manager, auto-reconnect |
| **NTP** | 0 | 2 (High) | 4 | Trigger | Time sync, timezone apply |
| **Web** | 0 | 1 (Low) | 5 | 5s | AsyncWebServer, memory monitor |
| **Prayer** | 0 | 1 (Low) | 16 | Trigger | API fetch, midnight update |
| **Clock** | 0 | 2 (High) | 2 | 1s | Time increment, NTP hourly trigger |
| **RTC Sync** | 0 | 1 (Low) | 2 | 60s | RTC → System time backup |
| **Audio** | 1 | 0 (Lowest) | 4 | Trigger | Play WAV from SD Card |

**Watchdog:**
- Timeout: **60 seconds**
- Registered: WiFi, Web, NTP, Prayer
- Auto-recovery: Prayer Task (monitor setiap 30s)

### Task Communication

**Semaphores (Mutex):**
- `displayMutex` - LVGL rendering protection
- `timeMutex` - Time config protection
- `wifiMutex` - WiFi state protection
- `settingsMutex` - Config file access
- `spiMutex` - SPI bus (TFT + Touch)
- `i2cMutex` - I2C bus (RTC)
- `audioMutex` - Audio state protection
- `sdMutex` - SD Card access
- `countdownMutex` - Countdown state
- `wifiRestartMutex` - WiFi/AP restart protection

**Queue:**
- `displayQueue` - UI update commands (size: 20)

**Event Group:**
- `wifiEventGroup` - WiFi connection events (CONNECTED, DISCONNECTED, GOT_IP)

### Memory Management

**Heap Usage (typical):**
```
Total:   320 KB
Used:    75 KB  (LVGL, tasks, buffers)
Free:    245 KB
Peak:    ~4 KB  (during HTTP requests)
```

**Stack Allocation:**
```
Total:   49 KB
UI:      12 KB (largest - LVGL buffers)
Prayer:  16 KB (HTTP + JSON parsing)
Others:  21 KB
```

**Critical Thresholds:**
- Free heap < 200 KB → Warning
- Task stack < 2000 bytes → Critical
- Stack usage > 95% → Immediate danger

### Data Flow

```
Boot → LittleFS Load → WiFi Init → AP Start
  ↓
WiFi Connect → NTP Sync → Time Set → RTC Save
  ↓
Midnight (00:00) → NTP Sync → Prayer API → Update Display
  ↓
Prayer Time → Blink Label → Touch Detect → Audio Play
```

### File System (LittleFS)

**Storage:**
- Total: 1.5 MB (partition)
- Files:
  - `/index.html` (~15 KB)
  - `/assets/css/foundation.min.css` (~60 KB)
  - `/cities.json` (~45 KB default, max 1 MB)
  - `/wifi_creds.txt` (~100 bytes)
  - `/ap_creds.txt` (~150 bytes)
  - `/prayer_times.txt` (~200 bytes)
  - `/city_selection.txt` (~150 bytes)
  - `/method_selection.txt` (~100 bytes)
  - `/timezone.txt` (~20 bytes)
  - `/buzzer_config.txt` (~100 bytes)
  - `/adzan_state.txt` (~150 bytes)

**Auto-save Mechanism:**
- Write immediately after user action
- 50-100ms delay before write (debouncing)
- Verification read after write
- No cache - direct LittleFS write

---

## 🌐 API Endpoints

### GET `/api/data`
**Deskripsi:** Data sistem real-time untuk integrasi IoT

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
    "terbit": "05:24",
    "zuhur": "11:54",
    "ashar": "15:14",
    "maghrib": "17:54",
    "isya": "19:07"
  },
  "location": {
    "city": "Jakarta",
    "cityId": "jakarta",
    "displayName": "Jakarta (Kota)",
    "latitude": "-6.2088",
    "longitude": "106.8456"
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

**Use Case:**
- Home Assistant integration
- Node-RED dashboard
- Mobile app data source
- Monitoring system

**Example (curl):**
```bash
curl http://192.168.4.1/api/data | jq .
```

---

### GET `/api/countdown`
**Deskripsi:** Status countdown restart/reset

**Response:**
```json
{
  "active": true,
  "remaining": 45,
  "total": 60,
  "message": "Memulai Ulang Perangkat",
  "reason": "device_restart",
  "serverTime": 123456789
}
```

**Reasons:**
- `device_restart` - Normal restart
- `ap_restart` - AP config change
- `factory_reset` - Factory reset

**Polling:**
```javascript
// Check setiap 1 detik
setInterval(async () => {
  const res = await fetch('/api/countdown');
  const data = await res.json();
  if (data.active) {
    console.log(`${data.message}: ${data.remaining}s`);
  }
}, 1000);
```

---

### GET `/devicestatus`
**Deskripsi:** Status perangkat lengkap (Tab BERANDA)

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

---

### POST `/setwifi`
**Deskripsi:** Set WiFi router credentials

**Body (URL-encoded):**
```
ssid=MyWiFi&password=MyPassword123
```

**Response:**
```
OK
```

**Behavior:**
- Save credentials to LittleFS
- Trigger WiFi reconnect (Task-based)
- No countdown (background reconnect)

---

### POST `/setap`
**Deskripsi:** Set Access Point config

**Body (URL-encoded):**
```
// Mode 1: Update SSID/Password only
ssid=MyAP&password=MyPass123&updateNetworkConfig=false

// Mode 2: Update Network Config only
apIP=192.168.5.1&gateway=192.168.5.1&subnet=255.255.255.0&updateNetworkConfig=true
```

**Response:**
```
OK
```

**Behavior:**
- Save config to LittleFS
- Trigger AP restart dengan countdown 60s (jika client dari local AP)
- Client akan auto-redirect ke IP baru

---

### POST `/setcity`
**Deskripsi:** Set lokasi dan koordinat

**Body (URL-encoded):**
```
city=jakarta&cityName=Jakarta&lat=-6.2088&lon=106.8456
```

**Response:**
```json
{
  "success": true,
  "city": "Jakarta",
  "updating": true
}
```

**Behavior:**
- Save city selection
- Jika WiFi connected → Trigger Prayer API fetch
- Update display setelah berhasil

---

### POST `/uploadcities`
**Deskripsi:** Upload cities.json baru

**Body:** `multipart/form-data` dengan file `cities.json`

**Response:**
```json
{
  "success": true
}
```

**Validation:**
- Filename must be `cities.json`
- Max size: 1 MB
- JSON format validation
- Required fields check
- ArduinoJson size estimation (max 320 KB heap usage)

---

### POST `/synctime`
**Deskripsi:** Manual time sync dari browser

**Body (URL-encoded):**
```
y=2024&m=12&d=19&h=14&i=35&s=22
```

**Response:**
```
Waktu berhasil di-sync!
```

**Behavior:**
- Set system time
- Save to RTC (if available)
- Update display immediately

---

### POST `/restart`
**Deskripsi:** Restart perangkat

**Response:**
```
OK
```

**Behavior:**
- Start countdown 60s
- Shutdown services at 35s remaining
- Force redirect after countdown

---

### POST `/reset`
**Deskripsi:** Factory reset

**Response:**
```
OK
```

**Behavior:**
- Delete all config files
- Reset to default values
- Start countdown 60s
- Redirect to 192.168.4.1 after reset

---

## 🔧 Konfigurasi Lanjutan

### Adjust Backlight Brightness

Edit di kode:
```cpp
// Baris 99 di jws.ino
#define TFT_BL_BRIGHTNESS 180  // 0-255 (70% default)

// 255 = 100% (sangat terang)
// 180 = 70%  (optimal)
// 128 = 50%  (hemat daya)
// 64  = 25%  (sangat gelap)
```

### Adjust Touch Calibration

Edit di kode:
```cpp
// Baris 83-86 di jws.ino
#define TS_MIN_X 370
#define TS_MAX_X 3700
#define TS_MIN_Y 470
#define TS_MAX_Y 3600

// Cara kalibrasi:
// 1. Upload sketch dengan debug touch enabled
// 2. Tap 4 corner layar
// 3. Catat nilai X/Y mentah dari serial monitor
// 4. Update konstanta di atas
```

### Adjust Buzzer Frequency

Edit di kode:
```cpp
// Baris 94 di jws.ino
#define BUZZER_FREQ 2000  // Hz

// 1000 Hz = Nada rendah
// 2000 Hz = Nada sedang (default)
// 4000 Hz = Nada tinggi
// 8000 Hz = Nada sangat tinggi
```

### Adjust Blink Duration

Edit di kode:
```cpp
// Baris 308-309 di jws.ino
const unsigned long BLINK_DURATION = 60000;   // 60 detik
const unsigned long BLINK_INTERVAL = 500;     // 500ms on/off

// BLINK_DURATION: Berapa lama label blink
// BLINK_INTERVAL: Kecepatan blink (on/off cycle)
```

### Adjust Adzan Timeout

Edit di kode:
```cpp
// Baris 398 di checkPrayerTime()
adzanState.deadlineTime = now_t + 600;  // 600 detik = 10 menit

// Ubah 600 ke nilai lain:
// 300  = 5 menit
// 600  = 10 menit (default)
// 900  = 15 menit
// 1800 = 30 menit
```

### Adjust NTP Sync Interval

Edit di kode:
```cpp
// Baris 2209 di clockTickTask()
if (autoSyncCounter >= 3600) {  // 3600 detik = 1 jam

// Ubah 3600 ke nilai lain:
// 1800  = 30 menit
// 3600  = 1 jam (default)
// 7200  = 2 jam
// 86400 = 24 jam (sekali sehari)
```

### Adjust Task Stack Size

Jika ada crash, increase stack:
```cpp
// Baris 89-101 di jws.ino
#define UI_TASK_STACK_SIZE 12288       // +4096 jika crash
#define WIFI_TASK_STACK_SIZE 3072      // +1024 jika crash
#define NTP_TASK_STACK_SIZE 4096       // +2048 jika crash
#define WEB_TASK_STACK_SIZE 5120       // +2048 jika crash
#define PRAYER_TASK_STACK_SIZE 16384   // +4096 jika crash
#define RTC_TASK_STACK_SIZE 2048       // +1024 jika crash
#define CLOCK_TASK_STACK_SIZE 2048     // OK (simple)
#define AUDIO_TASK_STACK_SIZE 4096     // +2048 jika crash
```

**⚠️ WARNING:** Total stack tidak boleh > 60 KB (risk of heap exhaustion)

---

## 📝 Changelog

### v2.2 (Current)
- ✅ Stack optimization untuk semua tasks
- ✅ Auto-recovery Prayer Task (watchdog)
- ✅ WiFi sleep double-disabled untuk performa
- ✅ Countdown visual untuk restart/reset (60s)
- ✅ Touch adzan mode (10 menit timeout)
- ✅ Audio adzan via SD Card + PCM5102A
- ✅ Persistent adzan state (survive restart)
- ✅ Memory leak detection
- ✅ RTC validation dan auto-recovery
- ✅ Timezone auto-trigger NTP + Prayer update
- ✅ WiFi/AP restart protection (debouncing + mutex)
- ✅ Upload cities.json validation (size + JSON)
- ✅ Coordinate edit dengan reset/cancel
- ✅ Test buzzer dengan auto-timeout 30s

## 🙏 Credits

**Developer:**
- GONIT - Global Network Identification Technology

**Libraries:**
- LVGL Team - https://lvgl.io/
- Espressif Systems - ESP32 Arduino Core
- Bodmer - TFT_eSPI
- Paul Stoffregen - XPT2046_Touchscreen
- Benoit Blanchon - ArduinoJson
- me-no-dev - ESPAsyncWebServer
- Adafruit - RTClib

**API:**
- Aladhan API - https://aladhan.com/

---

## 📞 Support

**Issues:** https://github.com/gonit-dev/jws-indonesia/issues
**Discussions:** https://github.com/gonit-dev/jws-indonesia/discussions
---

**GONIT - Global Network Identification Technology**
