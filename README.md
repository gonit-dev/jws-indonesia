# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan antarmuka web dan backup RTC

[![Version](https://img.shields.io/badge/version-2.1-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/) [![License](https://img.shields.io/badge/license-MIT-yellow)](LICENSE)

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
- **Jadwal Sholat Otomatis** - Pembaruan otomatis tengah malam via Aladhan API
- **Notifikasi Visual** - Tampilan berkedip selama 1 menit saat masuk waktu sholat
- **8 Metode Kalkulasi** - Kemenag, MWL, Egyptian, ISNA, Shia, Karachi, Makkah, Tehran
- **500+ Kota Indonesia** - Database kota dengan koordinat GPS akurat
- **Koordinat Manual** - Edit koordinat GPS dengan validasi real-time

### ⏰ Manajemen Waktu
- **Sinkronisasi NTP Otomatis** - Sinkronisasi jam otomatis setiap 1 jam
- **Multiple NTP Server** - Fallback ke pool.ntp.org, time.google.com, time.windows.com
- **Backup RTC DS3231** - Jam tetap akurat saat mati lampu (backup baterai)
- **Dukungan Zona Waktu** - UTC-12 hingga UTC+14 dengan validasi
- **Sinkronisasi Manual** - Sinkronisasi waktu dari browser

### 🌐 Fitur Jaringan
- **WiFi Mode Ganda** - AP (Access Point) + STA (Station) bersamaan
- **Koneksi Event-Driven** - Auto-reconnect cepat tanpa polling
- **Pengaturan AP Custom** - SSID, Password, IP, Gateway, Subnet dapat dikonfigurasi
- **Monitor Koneksi** - Tampilan status WiFi dan RSSI secara real-time
- **Reconnect Cepat** - Auto-reconnect dalam 3-5 detik

### 🖥️ Antarmuka Pengguna
- **LVGL 9.2.0** - UI touchscreen yang smooth @ 20 FPS
- **Desain EEZ Studio** - Layout UI profesional
- **Web Responsif** - Antarmuka mobile-friendly dengan Foundation CSS
- **Tampilan Real-Time** - Jam, tanggal, waktu sholat update otomatis
- **Loading Manager** - Progressive loading dengan feedback status

### 🔊 Kontrol Buzzer
- **Toggle Per-Sholat** - Enable/disable buzzer untuk setiap waktu sholat
- **Kontrol Volume** - 0-100% dapat diatur dengan preview langsung
- **Output PWM** - Suara buzzer smooth via GPIO26
- **Alert Otomatis** - Buzzer ON/OFF sinkron dengan tampilan berkedip

### 💾 Penyimpanan Data
- **LittleFS Storage** - Semua konfigurasi tersimpan di flash
- **Auto-Save** - Konfigurasi tersimpan otomatis setelah perubahan
- **Factory Reset** - Reset ke pengaturan default dengan countdown safety

### 🔄 Pembaruan Pintar
- **Auto-Update Tengah Malam** - Waktu sholat update otomatis tengah malam dengan sinkronisasi NTP
- **Update On-Demand** - Pembaruan manual via antarmuka web
- **Pemrosesan Background** - Pembaruan non-blocking tanpa freeze UI
- **Validasi Waktu** - Validasi timestamp sebelum request API

### 🛡️ Fitur Keamanan
- **Sistem Countdown** - Countdown 60 detik sebelum restart/reset
- **Proteksi Debouncing** - Mencegah multiple restart cepat (interval min 3 detik)
- **Mutex Locking** - Operasi thread-safe untuk semua task
- **Watchdog Timer** - Timeout 60 detik dengan auto-recovery
- **Deteksi Tipe Koneksi** - Redirect pintar berdasarkan akses (Local AP vs Remote)

---

## 🔧 Kebutuhan Hardware

### Board: ESP32-2432S024
- **MCU:** ESP32 Dual-Core @ 240MHz
- **RAM:** 520KB SRAM
- **Flash:** 4MB (3MB APP + 1MB SPIFFS)
- **Display:** ILI9341 2.4" TFT (320x240)
- **Touch:** XPT2046 Resistive
- **WiFi:** 802.11 b/g/n (2.4GHz saja)
- **Power:** 5V USB (minimal 2A disarankan)

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

### RTC DS3231 (Opsional - Sangat Disarankan)
```
DS3231       ESP32
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21
SCL     →    GPIO 22
```

**Fitur RTC:**
- Backup baterai (CR2032) untuk persistensi waktu
- Kompensasi tahun kabisat otomatis
- Temperature compensated crystal oscillator (akurasi ±2ppm)
- Validasi hardware untuk deteksi modul rusak

---

## 📦 Instalasi

### 1. Kebutuhan

| Komponen | Versi | Wajib | Catatan |
|----------|-------|-------|---------|
| ESP32 Board | v3.0.7 | ✅ | `ledcAttach()` memerlukan v3.x |
| LVGL | 9.2.0 | ✅ | v8.x tidak kompatibel |
| TFT_eSPI | 2.5.0+ | ✅ | |
| XPT2046_Touchscreen | 1.4+ | ✅ | |
| ArduinoJson | 6.21.0+ | ✅ | v7.x kompatibel |
| ESPAsyncWebServer | 1.2.3+ | ✅ | |
| AsyncTCP | 1.1.1+ | ✅ | |
| TimeLib | 1.6.1+ | ✅ | |
| RTClib | 2.1.1+ | ✅ | Versi Adafruit |
| Arduino IDE | 2.x+ | - | |

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Cari: "esp32"
Install: esp32 by Espressif Systems v3.0.7
```

### 3. Install Library

Via Library Manager (Sketch → Include Library → Manage Libraries):

```
✅ LVGL                   9.2.0      (oleh LVGL)
✅ TFT_eSPI               2.5.0+     (oleh Bodmer)
✅ XPT2046_Touchscreen    1.4+       (oleh Paul Stoffregen)
✅ ArduinoJson            6.21.0+    (oleh Benoit Blanchon)
✅ ESPAsyncWebServer      1.2.3+     (oleh me-no-dev)
✅ AsyncTCP               1.1.1+     (oleh me-no-dev)
✅ TimeLib                1.6.1+     (oleh Michael Margolis)
✅ RTClib                 2.1.1+     (oleh Adafruit)
```

### 4. Konfigurasi TFT_eSPI

**Lokasi:** `Arduino/libraries/TFT_eSPI/User_Setup_Select.h`

Comment default, uncomment:
```cpp
#include <User_Setups/Setup24_ST7789.h>  // atau sesuaikan dengan board
```

**Atau buat custom:** `User_Setup.h`
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

# Upload filesystem (folder data/)
Tools → ESP32 Sketch Data Upload

# Upload kode
Sketch → Upload (Ctrl+U)
```

**Pengaturan Board:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
Core Debug Level: None (untuk produksi)
```

---

## 🚀 Panduan Cepat

### Langkah 1: Boot Pertama
```
📶 AP SSID: "JWS Indonesia"
🔐 Password: "12345678"
🌐 Alamat IP: http://192.168.4.1
⏰ Waktu Awal: 01/01/2000 00:00:00 (menunggu sinkronisasi NTP)
```

**Output Serial Monitor:**
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

### Langkah 2: Konfigurasi WiFi
1. **Sambungkan ke AP**
   - SSID: "JWS Indonesia"
   - Password: "12345678"

2. **Buka Browser**
   - URL: `http://192.168.4.1`
   - Responsif mobile (berfungsi di ponsel)

3. **Konfigurasi WiFi Router**
   - Tab **WIFI** → Bagian "WiFi Router"
   - Masukkan SSID WiFi rumah (case-sensitive)
   - Masukkan Password (minimal 8 karakter)
   - Klik **Simpan**

4. **Tunggu Koneksi**
   - Perangkat akan auto-connect (~15 detik)
   - Cek serial monitor untuk alamat IP
   - Akses via: `http://<IP-ESP32>`

### Langkah 3: Atur Lokasi & Metode
1. **Pilih Kota**
   - Tab **LOKASI** → Dropdown "Pilih Lokasi"
   - Pilih dari 500+ kota Indonesia
   - Auto-grouping berdasarkan provinsi

2. **Verifikasi/Edit Koordinat** (Opsional)
   - Koordinat muncul otomatis
   - Edit manual jika perlu koreksi
   - Format: -6.2088 (latitude), 106.8456 (longitude)
   - Validasi: lat (-90 hingga 90), lon (-180 hingga 180)

3. **Pilih Metode Kalkulasi**
   - Default: Egyptian General Authority of Survey
   - Pilihan: Kemenag, MWL, ISNA, Shia, Karachi, Makkah, Tehran
   - Klik **Simpan**

4. **Auto-Update**
   - Waktu sholat akan otomatis diambil dari API
   - Display update dalam 3-5 detik
   - Cek serial monitor untuk logs

### Langkah 4: Konfigurasi Zona Waktu
```
Default: UTC+7 (WIB - Waktu Indonesia Barat)

Zona Waktu Indonesia:
- WIB:  +7  (Jawa, Sumatera, Kalimantan Barat)
- WITA: +8  (Kalimantan Tengah/Timur, Sulawesi, Bali, NTB, NTT)
- WIT:  +9  (Papua, Maluku)
```

**Atur Zona Waktu:**
1. Tab **WAKTU**
2. Klik **🕐** (ikon edit)
3. Input: +7, +8, +9, atau -12 hingga +14
4. Klik **💾** (simpan) atau Enter
5. NTP akan auto re-sync dengan zona waktu baru

### Langkah 5: Konfigurasi Buzzer
1. Tab **JADWAL**
2. Toggle ON/OFF untuk setiap waktu sholat
3. Atur slider volume (0-100%)
4. Auto-save setiap perubahan

---

## 🌐 Antarmuka Web

### Metode Akses

**Mode AP (Default):**
```
URL: http://192.168.4.1
Network: JWS Indonesia
Tidak memerlukan internet
```

**Mode STA (Setelah konfigurasi WiFi):**
```
URL: http://<IP-ESP32>
Cek serial monitor untuk IP
Koneksi internet tersedia
```

**Akses Ganda:**
```
Akses via AP: http://192.168.4.1
Akses via WiFi: http://192.168.x.x
Keduanya berfungsi bersamaan
```

### Gambaran Tab

#### 🏠 BERANDA
**Status Perangkat:**
- Jaringan WiFi (SSID yang terhubung)
- Alamat IP (IP lokal dari router)
- Status Internet (Badge Connected/Disconnected)
- Status NTP (Tersinkron/Belum tersinkron)
- Server NTP (pool.ntp.org, time.google.com, dll)
- Waktu Real-Time (update setiap detik)
- Tanggal (format DD/MM/YYYY)
- Uptime (HH:MM:SS atau Dhari HH:MM:SS)

**Aksi:**
- **Mulai Ulang Perangkat** - Restart aman dengan countdown 60 detik

#### 📡 WIFI
**Konfigurasi WiFi Router:**
- Input SSID (text, wajib)
- Input Password (text, minimal 8 karakter)
- Simpan → Trigger reconnect dengan countdown
- Batal → Kembalikan nilai tersimpan

**Konfigurasi Access Point:**
- SSID AP (text, wajib, nama custom)
- Password AP (text, minimal 8 karakter)
- Simpan → Restart AP dengan countdown
- Batal → Kembalikan nilai tersimpan

**Konfigurasi Jaringan AP:**
- Alamat IP (format xxx.xxx.xxx.xxx)
- Gateway (format xxx.xxx.xxx.xxx)
- Subnet Mask (format xxx.xxx.xxx.xxx)
- Validasi real-time
- Simpan → Terapkan dan restart AP

**Fitur Keamanan:**
- Deteksi tipe koneksi (Local AP vs Remote)
- Countdown pintar hanya untuk pengguna local AP
- Proteksi debouncing (minimal 3 detik)

#### ⏰ WAKTU
**Pembaruan Manual:**
- Tombol "Perbarui Waktu"
- Sinkronisasi waktu dari browser
- Update display dan RTC

**Pembaruan Otomatis (NTP):**
- Auto-sync setiap 1 jam
- Fallback ke multiple server
- Tampilan status (Tersinkron/Belum)
- Server yang digunakan

**Zona Waktu:**
- Inline editing (klik ikon 🕐)
- Input offset UTC (-12 hingga +14)
- Validasi real-time
- Auto NTP re-sync setelah save
- Waktu sholat auto-update dengan zona waktu baru

**Shortcut Zona Waktu:**
- 🇮🇩 WIB = +7
- 🇮🇩 WITA = +8
- 🇮🇩 WIT = +9

#### 🕌 LOKASI
**Pilih Lokasi:**
- Dropdown 500+ kota Indonesia
- Dikelompokkan berdasarkan provinsi
- Fungsi pencarian
- Tampilan koordinat otomatis

**Ubah Koordinat:**
- Input Latitude (-90 hingga 90)
- Input Longitude (-180 hingga 180)
- Koordinat default dari JSON
- Tombol Reset ke koordinat original
- Tombol Batal kembalikan nilai tersimpan
- Validasi real-time

**Tampilan Tipe Lokasi:**
- 🏙️ Kota
- 🏞️ Kabupaten
- 🏘️ Kecamatan
- 🏡 Kelurahan/Desa

**Metode Kalkulasi:**
- 8 metode kalkulasi
- Default: Egyptian General Authority
- Auto-update waktu sholat setelah perubahan
- Tampilan metode saat ini di 2 lokasi

**Perbarui Lokasi (Upload cities.json):**
- Input file dengan dukungan drag & drop
- Validasi: nama file harus "cities.json"
- Ukuran maksimal: 1MB
- Validasi format JSON
- Validasi koordinat GPS
- Kalkulasi ukuran ArduinoJson
- Progress bar dengan persentase
- Auto-reload dropdown setelah berhasil

#### 🙏 JADWAL
**Tampilan Waktu Sholat:**
- Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya
- Format: HH:MM (24-jam)
- Auto-update dari server

**Tombol Toggle Buzzer:**
- ON/OFF individual untuk setiap waktu
- Switch visual (Foundation CSS)
- Auto-save saat perubahan

**Kontrol Volume:**
- Slider HTML5 range (0-100%)
- Tampilan persentase real-time
- Kontrol PWM via GPIO26
- Debounced save (500ms)

**Tampilan Metode Saat Ini:**
- Menampilkan metode kalkulasi terpilih
- Update saat metode berubah

**Info Auto-Update:**
- Update otomatis tengah malam
- Sinkronisasi NTP sebelum update
- Pemrosesan background

#### ⚠️ RESET
**Factory Reset:**
- Hapus semua file konfigurasi
- Reset ke pengaturan AP default
- Reset waktu ke 01/01/2000
- Auto-restart perangkat dengan countdown
- Dialog konfirmasi
- Countdown 60 detik dengan UI terkunci

**File yang Dihapus:**
```
/wifi_creds.txt
/ap_creds.txt
/prayer_times.txt
/city_selection.txt
/method_selection.txt
/timezone.txt
/buzzer_config.txt
```

**Nilai Default:**
```
AP SSID: JWS Indonesia
AP Password: 12345678
AP IP: 192.168.4.1
Metode: Egyptian (ID 5)
Zona Waktu: UTC+7
```

### Sistem Countdown

**Feedback Visual:**
- Notifikasi toast (atas tengah)
- Overlay layar penuh (cegah interaksi)
- Tampilan countdown real-time
- Perubahan warna:
  - Peringatan kuning (60-20 detik)
  - Alert oranye (20-10 detik)
  - Kritis merah (10-0 detik)

**Server-Driven:**
- Polling setiap 1 detik dari `/api/countdown`
- Auto-sync dengan waktu server
- Penanganan graceful jika koneksi terputus
- Auto-redirect setelah countdown selesai

**Redirect Pintar:**
- Pengguna local AP: redirect ke IP AP baru
- Pengguna remote: redirect ke URL saat ini
- Auto-deteksi tipe koneksi

---

## 📊 Arsitektur Sistem

### Task FreeRTOS

| Task | Core | Priority | Stack | Interval | Fungsi |
|------|------|----------|-------|----------|----------|
| **UI Task** | 1 | 3 (Tertinggi) | 12KB | 50ms | Rendering LVGL, penanganan touch, animasi blink |
| **WiFi Task** | 0 | 2 (Tinggi) | 5KB | Event-driven | Manajemen koneksi, auto-reconnect |
| **NTP Task** | 0 | 2 (Tinggi) | 6KB | On-demand | Sinkronisasi waktu dengan multiple server |
| **Web Task** | 0 | 1 (Rendah) | 5KB | Monitor 5s | AsyncWebServer, pembersihan koneksi |
| **Prayer Task** | 0 | 1 (Rendah) | 6KB | Cek 1s | Update tengah malam, pemrosesan background |
| **Clock Task** | 0 | 2 (Tinggi) | 2KB | Tick 1s | Increment waktu sistem, auto NTP sync |
| **RTC Sync** | 0 | 1 (Rendah) | 2KB | 60s | Sinkronisasi bidirectional RTC ↔ Sistem |

### Sinkronisasi

**Semaphore/Mutex:**
```cpp
displayMutex     → Operasi LVGL (UI Task)
timeMutex        → Konfigurasi waktu (Semua task)
wifiMutex        → Konfigurasi WiFi (Task WiFi/Web)
settingsMutex    → Semua file konfigurasi (Multiple task)
spiMutex         → TFT/Touch SPI bus (UI Task)
i2cMutex         → RTC I2C bus (RTC Sync Task)
wifiRestartMutex → Proteksi restart (Web Task)
countdownMutex   → Status countdown (Web Task)
```

**Queue:**
```cpp
displayQueue → Push update display dari task mana pun ke UI Task
```

### Sistem Auto-Update

**Alur Update Sholat Tengah Malam:**
```
Terdeteksi 00:00-00:05
    ↓
Trigger NTP Sync Task
    ↓
Tunggu NTP Complete (maks 30s)
    ↓
Validasi Timestamp (> 01/01/2000)
    ↓
Fetch Prayer Times API
    ↓
Simpan ke LittleFS
    ↓
Queue Display Update
```

**Sinkronisasi NTP Per Jam:**
```
Clock Task Counter = 3600 detik
    ↓
Trigger NTP Sync Task
    ↓
Sync dengan NTP server (fallback)
    ↓
Update Waktu Sistem
    ↓
Simpan ke RTC (jika tersedia)
    ↓
Queue Display Update
```

**Logika Sinkronisasi RTC:**
```
Setiap 60 detik:
    Cek validitas RTC
        ↓
    Bandingkan RTC vs Waktu Sistem
        ↓
    Jika NTP belum sync → Gunakan waktu RTC
    Jika NTP sync & Sistem lebih baru → Pertahankan waktu Sistem
    Jika RTC lebih baru → Update Sistem dari RTC
```

### WiFi Event-Driven

**Desain Tanpa Polling:**
```cpp
WiFi.onEvent() → Set event bits → Task menunggu dengan timeout
```

**Event:**
```cpp
WIFI_CONNECTED_BIT    → STA terhubung ke router
WIFI_GOT_IP_BIT       → Dapat alamat IP, trigger NTP
WIFI_DISCONNECTED_BIT → Koneksi terputus, trigger reconnect
```

**Keuntungan:**
- CPU idle saat stabil
- Respons cepat (<10ms)
- Auto-reconnect tanpa intervensi pengguna
- AP tetap hidup saat STA disconnect

---

## 🔍 Pemecahan Masalah

### Error Kompilasi

**`ledcAttach() not declared in this scope`**
```
Penyebab: ESP32 Board v2.x (API lama)
Solusi: Install ESP32 Board v3.0.7
Lokasi: Tools → Board → Boards Manager → esp32

Verifikasi:
Tools → Board → ESP32 Dev Module
Help → About Arduino IDE → Cek versi terinstal
```

**Error fungsi LVGL (lv_xxx not declared)**
```
Penyebab: LVGL v8.x terinstal (API lama)
Solusi: Install LVGL v9.2.0
Lokasi: Sketch → Include Library → Manage Libraries → LVGL

Verifikasi:
Cek: Arduino/libraries/lvgl/library.properties
Harus menampilkan: version=9.2.0
```

**`AsyncWebServer.h: No such file or directory`**
```
Penyebab: Library ESPAsyncWebServer tidak ada
Solusi: Install via Library Manager

Dependensi:
1. ESPAsyncWebServer by me-no-dev (v1.2.3+)
2. AsyncTCP by me-no-dev (v1.1.1+)

Catatan: Install AsyncTCP DULU, kemudian ESPAsyncWebServer
```

**`RTClib.h: No such file or directory`**
```
Penyebab: Adafruit RTClib tidak ada
Solusi: Install Adafruit RTClib v2.1.1+

Lokasi: Library Manager → Cari "RTClib"
Pilih: RTClib by Adafruit (BUKAN fork lain)
```

### Error Upload

**`A fatal error occurred: Timed out waiting for packet header`**
```
Solusi (Coba berurutan):

1. Metode Hardware:
   - Tekan & tahan tombol BOOT
   - Klik Upload di Arduino IDE
   - Tahan BOOT sampai "Connecting..." muncul
   - Lepas BOOT saat upload dimulai

2. Metode Software:
   - Kurangi Upload Speed
   - Tools → Upload Speed → 115200 (dari 921600)
   - Coba upload lagi

3. Kabel & Driver:
   - Ganti kabel USB (pastikan data+power, bukan charge-only)
   - Install driver CH340/CP2102
   - Windows: Cek Device Manager
   - Mac: Install driver dari manufacturer

4. Pemilihan Port:
   - Tools → Port → Pilih port COM yang benar
   - Lepas perangkat USB lain
   - Coba port USB berbeda di komputer
```

**`Sketch too big: Program size exceeds available space`**
```
Solusi: Ganti Partition Scheme
Tools → Partition Scheme → Default 4MB with spiffs (1.2MB APP)

Jika masih terlalu besar:
1. Hapus library yang tidak digunakan dari #include
2. Kurangi ukuran buffer LVGL
3. Nonaktifkan statement debug Serial.println()
```

**`SPIFFS Upload Failed: Not Enough Space`**
```
Cek:
1. Ukuran folder data/ harus < 1MB
2. File besar: index.html, foundation.min.css, cities.json

Optimasi:
- Minify HTML/CSS
- Kompres cities.json (hapus whitespace)
- Hapus aset yang tidak digunakan
```

### Masalah Runtime

**WiFi tidak connect / auto-disconnect**
```
Pemeriksaan:
✅ SSID/Password case-sensitive (cek typo)
✅ Router harus 2.4GHz (ESP32 tidak support 5GHz)
✅ Cek MAC filtering / whitelist router
✅ Cek DHCP pool router (IP tersedia?)
✅ Jarak ke router (kekuatan sinyal)

Debug:
Serial monitor → Cek nilai RSSI
RSSI > -50 dBm: Sangat baik
RSSI -50 hingga -70 dBm: Baik
RSSI < -70 dBm: Lemah (terlalu jauh)

Solusi:
1. Factory reset → Konfigurasi ulang WiFi
2. Restart router
3. Coba hotspot mobile untuk testing
4. Cek log router
```

**Jadwal sholat tidak akurat / selisih beberapa menit**
```
Penyebab: Koordinat GPS tidak akurat

Solusi:
1. Edit koordinat GPS manual
   - Buka Google Maps
   - Klik lokasi yang tepat (rumah/masjid)
   - Salin latitude & longitude
   - Tempel ke antarmuka web

2. Ganti metode kalkulasi
   - Tab LOKASI → Dropdown "Metode Kalkulasi"
   - Coba: Kemenag Indonesia (Metode 20)
   - Atau: Egyptian General Authority (Metode 5)

3. Verifikasi pemilihan kota
   - Pastikan kota yang dipilih sesuai dengan lokasi fisik
   - Bukan koordinat provinsi/pusat kota

Validasi:
- Cek via https://aladhan.com/prayer-times-api
- Bandingkan dengan jadwal sholat masjid terdekat
```

**Jam reset ke 01/01/2000 setelah mati lampu**
```
Status: NORMAL jika:
- Belum ada sinkronisasi NTP
- RTC DS3231 tidak terpasang
- Baterai RTC (CR2032) habis

Solusi:
1. Pasang modul RTC DS3231
   - Kabel: SDA→GPIO21, SCL→GPIO22
   - Masukkan baterai CR2032 (3V)
   - Power cycle perangkat

2. Sambungkan ke WiFi
   - Perangkat akan auto NTP sync
   - Waktu akan akurat dalam 10-30 detik

3. Cek status RTC
   - Serial monitor → Cari:
     "DS3231 detected" (OK)
     "DS3231 not found" (Masalah modul)
     "RTC hardware test PASSED" (OK)
     "RTC HARDWARE FAILURE" (Modul rusak)

4. Ganti RTC yang rusak
   - Gejala: Waktu mengembalikan data sampah
   - Sensor suhu berfungsi tapi waktu tidak valid
   - Solusi: Beli modul DS3231 asli yang baru
```

**Waktu sholat tidak update otomatis tengah malam**
```
Pemeriksaan:
✅ WiFi terhubung (perlu internet untuk API)
✅ NTP tersinkronisasi (waktu harus akurat)
✅ Kota & koordinat dipilih
✅ Serial monitor → Cek log "MIDNIGHT DETECTED"

Log Debug:
00:00-00:05 → Harus melihat:
"MIDNIGHT DETECTED - STARTING SEQUENCE"
"Triggering NTP Sync..."
"NTP SYNC COMPLETED"
"Updating Prayer Times..."

Jika stuck:
1. Update manual via antarmuka web (Tab LOKASI → Simpan)
2. Cek koneksi internet
3. Coba server NTP berbeda
4. Factory reset jika persisten
```

**Antarmuka web loading sangat lambat / timeout**
```
Kemungkinan Penyebab:
1. WiFi sleep diaktifkan (harus dinonaktifkan)
2. Sinyal lemah (RSSI < -70 dBm)
3. Multiple client secara bersamaan
4. Masalah cache browser

Solusi:
1. Cek status WiFi sleep
   Serial → Harus melihat:
   "WiFi Sleep: DOUBLE DISABLED"
   
2. Pindah lebih dekat ke perangkat
3. Hapus cache browser
   - Ctrl+Shift+Delete (Chrome/Firefox)
   - Pilih "Cached images and files"
   
4. Coba mode incognito/private
5. Gunakan browser berbeda
6. Restart perangkat
```

**Touch tidak responsif / koordinat salah**
```
Kalibrasi:
1. Cek konstanta di kode:
   #define TS_MIN_X 370
   #define TS_MAX_X 3700
   #define TS_MIN_Y 470
   #define TS_MAX_Y 3600

2. Test sudut touch
   - Kiri-atas harus register (0,0)
   - Kanan-bawah harus register (320,240)

3. Sesuaikan nilai jika perlu
   - Serial monitor → Cek koordinat touch
   - Hitung nilai MIN/MAX baru
   - Recompile & upload

4. Bersihkan layar sentuh
   - Minyak sidik jari mempengaruhi akurasi
   - Gunakan kain microfiber
```

**Display flicker / tearing**
```
Penyebab:
1. Power supply tidak cukup (< 2A)
2. Kabel USB buruk (voltage drop)
3. Kecepatan SPI terlalu tinggi/rendah

Solusi:
1. Gunakan adaptor 5V 2A+
2. Kabel USB lebih pendek (< 1 meter)
3. Cek pengaturan frekuensi SPI TFT_eSPI
4. Tambahkan kapasitor 100-470µF dekat input power
```

**Buzzer tidak bunyi saat waktu sholat**
```
Pemeriksaan:
✅ Toggle buzzer diaktifkan (Tab JADWAL)
✅ Volume tidak 0% 
✅ Koneksi GPIO26 (cek kabel)
✅ PWM channel diinisialisasi

Debug:
Serial monitor → Saat waktu sholat:
"PRAYER TIME ENTER: SUBUH"
"Starting to blink for 1 minute..."

Solusi:
1. Test buzzer manual
   - Set volume 50%
   - Aktifkan toggle untuk waktu sholat terdekat
   
2. Cek GPIO26
   - Gunakan multimeter/oscilloscope
   - Harus melihat sinyal PWM saat blinking
   
3. Ganti buzzer jika rusak
   - Test dengan buzzer lain
   - Pastikan kompatibel 3.3V
```

**Memory leak / perangkat hang setelah lama berjalan**
```
Monitoring:
Serial → Cek laporan stack (setiap 2 menit):
"STACK USAGE ANALYSIS"
"CRITICAL TASKS: ..." (jika ada > 90%)

"MEMORY STATUS"
"Lost: XXX bytes" (harus < 20KB)

Solusi:
1. Tingkatkan ukuran stack jika kritis
   - Edit: #define UI_TASK_STACK_SIZE 12288
   - Tingkatkan dengan increment 2048
   
2. Kurangi fragmentasi heap
   - Factory reset
   - Restart perangkat secara berkala
   
3. Proteksi watchdog
   - Perangkat auto-restart jika hang
   - Cek serial untuk watchdog trigger
```

---

## 🌐 API Endpoints

### GET `/api/data` - Integrasi IoT

**Deskripsi:** Data sistem real-time untuk integrasi eksternal (Node-RED, Home Assistant, MQTT bridge, dll)

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

**Field:**
- `time` - Waktu saat ini (HH:MM:SS)
- `date` - Tanggal saat ini (DD/MM/YYYY)
- `day` - Nama hari (English)
- `timestamp` - Unix timestamp (detik)
- `prayerTimes` - Semua waktu sholat (format HH:MM)
- `location` - Info kota + koordinat GPS
- `device.wifiConnected` - Status WiFi (boolean)
- `device.ntpSynced` - Status sinkronisasi NTP (boolean)
- `device.freeHeap` - RAM tersedia (bytes)
- `device.uptime` - Uptime perangkat (detik)

**Contoh Penggunaan:**
```javascript
// Node-RED: HTTP Request node
GET http://192.168.4.1/api/data
Parse JSON → Extract prayerTimes → Kirim notifikasi

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

### GET `/api/countdown` - Status Countdown

**Deskripsi:** Countdown yang dikendalikan server untuk operasi restart/reset

**Request:**
```bash
curl http://192.168.4.1/api/countdown
```

**Response (Aktif):**
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

**Response (Tidak Aktif):**
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

**Tipe Reason:**
- `ap_restart` - Restart Access Point
- `device_restart` - Restart perangkat penuh
- `factory_reset` - Operasi factory reset

**Implementasi Frontend:**
```javascript
// Polling setiap 1 detik
setInterval(async () => {
  const response = await fetch('/api/countdown');
  const data = await response.json();
  
  if (data.active) {
    // Tampilkan overlay countdown
    updateCountdownDisplay(data.remaining, data.message);
  } else {
    // Sembunyikan overlay, redirect jika perlu
    handleCountdownComplete(data.reason);
  }
}, 1000);
```

### GET `/devicestatus` - Status Perangkat

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

### GET `/getprayertimes` - Waktu Sholat

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

### GET `/getcityinfo` - Kota Saat Ini

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

### GET `/getmethod` - Metode Kalkulasi

**Response:**
```json
{
  "methodId": 5,
  "methodName": "Egyptian General Authority of Survey"
}
```

### GET `/gettimezone` - Offset Zona Waktu

**Response:**
```json
{
  "offset": 7
}
```

### POST `/setwifi` - Konfigurasi WiFi

**Body:**
```
ssid=MyWiFi&password=MyPassword123
```

**Response:** `200 OK` atau `400 Bad Request`

### POST `/setcity` - Atur Kota

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

### POST `/setmethod` - Atur Metode Kalkulasi

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

### POST `/synctime` - Sinkronisasi Waktu Manual

**Body:**
```
y=2024&m=12&d=19&h=14&i=35&s=22
```

**Response:** `200 OK` dengan pesan sukses

---

## 🚀 Optimasi Performa

### Optimasi WiFi

**1. Sleep Mode DOUBLE-DISABLED**
```cpp
WiFi.setSleep(WIFI_PS_NONE);           // Arduino API
esp_wifi_set_ps(WIFI_PS_NONE);        // ESP-IDF API
```
**Dampak:** Waktu respons < 10ms (sebelumnya 100-500ms)

**2. Koneksi Event-Driven**
```cpp
WiFi.onEvent() → FreeRTOS Event Groups
Tidak ada polling loop → CPU idle saat stabil
```
**Dampak:** 0% penggunaan CPU saat WiFi stabil

**3. TX Power Maksimum**
```cpp
WiFi.setTxPower(WIFI_POWER_19_5dBm);
esp_wifi_set_max_tx_power(78);
```
**Dampak:** Jangkauan & stabilitas lebih baik

**4. Auto-Reconnect**
```cpp
WiFi.setAutoReconnect(true);
```
**Dampak:** Tidak perlu intervensi manual

### Optimasi Web Server

**1. Buffer Pre-allocated**
```cpp
char jsonBuffer[512];  // Alokasi stack
snprintf(jsonBuffer, sizeof(jsonBuffer), ...);
```
**Dampak:** Tidak ada overhead malloc/free

**2. Caching Browser**
```cpp
response->addHeader("Cache-Control", "public, max-age=3600");
```
**Dampak:** CSS dimuat sekali, loading berikutnya instan

**3. Header Content-Length**
```cpp
response->setContentLength(LittleFS.open("/file").size());
```
**Dampak:** Browser dapat menampilkan progress akurat

**4. Operasi Async**
```cpp
ESPAsyncWebServer → Non-blocking
Multiple client secara bersamaan
```
**Dampak:** Tidak ada blocking request

### Optimasi UI

**1. Rendering Parsial**
```cpp
lv_display_set_buffers(display, buf, NULL, 
                       sizeof(buf), 
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```
**Dampak:** Smooth 20 FPS dengan buffer 10-line

**2. Debouncing Touch**
```cpp
if (now - lastTouchTime < 50) return;  // Skip pembacaan cepat
```
**Dampak:** CPU tidak overload dengan event touch

**3. Mutex SPI**
```cpp
xSemaphoreTake(spiMutex, ...) → Akses eksklusif
```
**Dampak:** Tidak ada collision SPI TFT/Touch

### Optimasi Memori

**1. Stack vs Heap**
```cpp
// Lebih suka stack (cepat, tidak ada fragmentasi)
char buffer[256];  // Bagus

// Hindari heap (lambat, fragmentasi)
char* buffer = malloc(256);  // Hindari jika memungkinkan
```

**2. Optimasi String**
```cpp
// Gunakan String hanya jika perlu
String city = "Jakarta";  // OK untuk config

// Gunakan char[] untuk temporary
char timeStr[10];  // Lebih baik untuk display
sprintf(timeStr, "%02d:%02d", h, m);
```

**3. Manajemen Memori JSON**
```cpp
JsonDocument doc;  // Alokasi stack
deserializeJson(doc, payload);  // Parse in-place
// Tidak perlu manajemen memori manual
```

### Optimasi Task

**1. Tuning Priority**
```
UI (3)    → Tertinggi (responsivitas visual)
WiFi (2)  → Tinggi (stabilitas jaringan)
Web (1)   → Rendah (pemrosesan background)
```

**2. Optimasi Ukuran Stack**
```
Monitor via printStackReport():
- Penggunaan < 60%: Optimal
- Penggunaan 60-75%: Pas
- Penggunaan > 90%: Tingkatkan ukuran!
```

**3. Integrasi Watchdog**
```cpp
esp_task_wdt_add(taskHandle);  // Setiap task
esp_task_wdt_reset();          // Setiap loop
```
**Dampak:** Auto-recovery dari hang

### Metrik Hasil

**Sebelum Optimasi:**
- Loading halaman: 2-5 detik
- Respons WiFi: 100-500ms
- Lag touch: terasa
- Memory leak: ada

**Setelah Optimasi:**
- Loading halaman: 200-500ms (10x lebih cepat!)
- Respons WiFi: < 10ms (50x lebih cepat!)
- Lag touch: tidak ada (smooth 20 FPS)
- Memory leak: tidak ada (stabil berhari-hari)

---

## 📁 Struktur File

```
jws-indonesia/
├── jws.ino                          # Program utama
│   ├── Definisi pin                 # GPIO, PWM, config SPI
│   ├── Konfigurasi task             # Ukuran stack, prioritas
│   ├── Struktur global              # WiFiConfig, TimeConfig, dll
│   ├── Deklarasi fungsi             # Forward declarations
│   ├── Implementasi                 # Semua fungsi
│   └── setup() & loop()             # Entry point Arduino
│
├── src/                             # EEZ Studio UI (auto-generated)
│   ├── ui.h                         # Inisialisasi LVGL UI
│   ├── ui.cpp                       # Implementasi UI
│   ├── screens.h                    # Objek screen
│   ├── screens.cpp                  # Handler screen
│   ├── images.h                     # Deklarasi gambar
│   ├── images.cpp                   # Data gambar (binary)
│   ├── fonts.h                      # Deklarasi font
│   └── fonts.cpp                    # Data font (binary)
│
├── data/                            # Filesystem LittleFS
│   ├── index.html                   # Antarmuka web (50KB)
│   │   ├── Framework Foundation CSS
│   │   ├── Manager loading
│   │   ├── Navigasi tab
│   │   ├── Validasi form
│   │   ├── Sistem countdown
│   │   └── Integrasi API
│   │
│   ├── assets/
│   │   └── css/
│   │       └── foundation.min.css  # Framework CSS (70KB)
│   │
│   └── cities.json                  # 500+ kota (150KB)
│       └── Format:
│           [{
│             "api": "jakarta",
│             "display": "Jakarta (Kota)",
│             "province": "DKI Jakarta",
│             "lat": "-6.175392",
│             "lon": "106.827153"
│           }, ...]
│
└── README.md                        # File ini
```

**File Runtime (Dibuat otomatis di LittleFS):**
```
/wifi_creds.txt          → SSID & Password WiFi Router
/ap_creds.txt            → SSID, Password, IP, Gateway, Subnet AP
/prayer_times.txt        → Cache waktu sholat (7 baris + kota)
/city_selection.txt      → Kota terpilih + koordinat GPS
/method_selection.txt    → ID & nama metode kalkulasi
/timezone.txt            → Offset UTC (-12 hingga +14)
/buzzer_config.txt       → Status toggle + volume (8 baris)
```

**Pola Akses File:**
- **Read:** Saat boot (semua file config)
- **Write:** Saat aksi pengguna via antarmuka web
- **Update:** Waktu sholat (otomatis tengah malam)

---

## 🛡️ Rekomendasi Keamanan

### Kredensial Default
```
⚠️ SEGERA GANTI SETELAH BOOT PERTAMA ⚠️

AP SSID:     JWS Indonesia
AP Password: 12345678
AP IP:       192.168.4.1
```

### Praktik Terbaik

**1. Ganti Kredensial AP**
```
Antarmuka Web → Tab WIFI → Bagian "Access Point"
- Set SSID kuat (nama unik)
- Set password kuat (min 12 karakter, campuran A-Z, a-z, 0-9, simbol)
- Klik Simpan
```

**2. Amankan WiFi Router**
```
✅ Enkripsi WPA2/WPA3
✅ Password kuat (20+ karakter)
✅ Nonaktifkan WPS
✅ Aktifkan MAC filtering (opsional)
✅ Sembunyikan SSID broadcast (opsional)
```

**3. Isolasi Jaringan**
```
⚠️ JANGAN ekspos ke internet publik
✅ Akses hanya dari jaringan lokal terpercaya
✅ Pertimbangkan VLAN IoT terpisah
✅ Aktifkan firewall router
```

**4. Keamanan Fisik**
```
✅ Perangkat di lokasi aman
✅ Tidak ada port USB terbuka (cegah tampering firmware)
✅ Monitor akses fisik tidak sah
```

**5. Update Berkala**
```
✅ Cek update firmware
✅ Update cities.json saat tersedia
✅ Monitor advisory keamanan
```

### Keterbatasan yang Diketahui

**Tidak Ada Autentikasi:**
- Antarmuka web tidak memiliki login/password
- Siapa pun dengan akses jaringan dapat mengubah pengaturan
- Mitigasi: Isolasi jaringan & keamanan fisik

**HTTP Saja:**
- Tidak ada enkripsi HTTPS/SSL
- Data dikirim dalam teks biasa
- Mitigasi: Gunakan hanya di jaringan lokal terpercaya

**Tidak Ada OTA:**
- Tidak ada update firmware Over-The-Air
- Memerlukan akses USB fisik untuk update
- Mitigasi: Rencanakan jendela maintenance

---

## 🔧 Konfigurasi Lanjutan

### Cities JSON Custom

**Persyaratan Format:**
```json
[
  {
    "api": "city_id",           // Wajib: lowercase, tanpa spasi
    "display": "City Name",      // Wajib: teks display
    "province": "Province Name", // Wajib: untuk grouping
    "lat": "-6.175392",         // Wajib: GPS latitude
    "lon": "106.827153"         // Wajib: GPS longitude
  }
]
```

**Aturan Validasi:**
1. JSON harus array valid
2. Semua field wajib (api, display, province, lat, lon)
3. Koordinat harus angka valid
   - Latitude: -90 hingga 90
   - Longitude: -180 hingga 180
4. Ukuran file maks: 1MB
5. Ukuran ArduinoJson maks: 320KB (dihitung)

**Menambahkan Kota Custom:**
```json
{
  "api": "lokasi_saya",
  "display": "Lokasi Custom Saya (Kota)",
  "province": "Provinsi Saya",
  "lat": "-6.123456",
  "lon": "106.789012"
}
```

**Testing:**
1. Upload via antarmuka web
2. Cek serial monitor untuk validasi
3. Dropdown harus auto-reload
4. Pilih kota custom → Simpan
5. Verifikasi akurasi waktu sholat

### Server NTP Custom

**Edit di kode:**
```cpp
const char *ntpServers[] = {
  "pool.ntp.org",        // Default: Global pool
  "time.google.com",     // Google time servers
  "time.windows.com"     // Microsoft time servers
};
```

**Tambahkan khusus Indonesia:**
```cpp
const char *ntpServers[] = {
  "id.pool.ntp.org",     // Pool Indonesia
  "time.bmkg.go.id",     // BMKG (jika tersedia)
  "pool.ntp.org"         // Fallback global
};
```

### Kecerahan Backlight Custom

**Edit di kode:**
```cpp
#define TFT_BL_BRIGHTNESS 180  // 0-255 (180 = ~70%)
```

**Opsi PWM:**
- 0: Mati (layar hitam)
- 128: ~50% kecerahan
- 180: ~70% kecerahan (default)
- 255: 100% kecerahan (maks)

### Kalibrasi Touch Custom

**Jika koordinat touch salah:**
```cpp
#define TS_MIN_X 370
#define TS_MAX_X 3700
#define TS_MIN_Y 470
#define TS_MAX_Y 3600
```

**Prosedur Kalibrasi:**
1. Aktifkan debug: Uncomment `Serial.printf()` di `my_touchpad_read()`
2. Touch 4 sudut, catat koordinat
3. Hitung nilai MIN/MAX
4. Update konstanta
5. Recompile & upload

### Ukuran Stack Custom

**Jika task crash (Stack Overflow):**
```cpp
#define UI_TASK_STACK_SIZE 12288    // Tingkatkan jika perlu
#define WEB_TASK_STACK_SIZE 5120    // Tingkatkan untuk request besar
```

**Monitor via:**
```cpp
printStackReport();  // Dipanggil setiap 2 menit di webTask
```

**Aturan:**
- Tingkatkan dengan increment 2048 (2KB)
- Jaga total < 200KB (batas RAM)
- Monitor selama berminggu-minggu untuk memastikan stabilitas

---

## 📚 API Eksternal

### Aladhan Prayer Times API

**Endpoint:**
```
http://api.aladhan.com/v1/timings/{date}
  ?latitude={lat}
  &longitude={lon}
  &method={method_id}
```

**ID Metode:**
- `0` - Shia Ithna-Ashari
- `1` - University of Islamic Sciences, Karachi
- `2` - Islamic Society of North America (ISNA)
- `3` - Muslim World League (MWL)
- `4` - Umm Al-Qura University, Makkah
- `5` - Egyptian General Authority of Survey (Default)
- `7` - Institute of Geophysics, University of Tehran
- `20` - Kementerian Agama Indonesia (Kemenag)

**Format Response:**
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

**Rate Limit:**
- Tidak ada limit resmi disebutkan
- Disarankan: Maks 1 request per menit
- Auto-update: Sekali per hari (tengah malam)

**Dokumentasi:**
https://aladhan.com/prayer-times-api

---

## 🤝 Kontribusi

Kontribusi sangat diterima! Silakan ikuti panduan berikut:

### Melaporkan Bug

1. Cek issue yang sudah ada terlebih dahulu
2. Berikan informasi detail:
   - Versi board ESP32
   - Versi Arduino IDE
   - Versi library
   - Output serial monitor
   - Langkah untuk mereproduksi

### Menyarankan Fitur

1. Cek apakah sudah diminta sebelumnya
2. Jelaskan use case dengan jelas
3. Berikan mockup jika terkait UI

### Pull Request

1. Fork repository
2. Buat feature branch
3. Ikuti style kode yang ada
4. Test secara menyeluruh
5. Update dokumentasi
6. Submit PR dengan deskripsi jelas
