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
- Notifikasi visual dan buzzer (dapat diatur per waktu sholat)
- 8 metode kalkulasi (Kemenag, MWL, Egyptian, ISNA, dll)
- 500+ kota Indonesia dengan koordinat GPS akurat
- Edit koordinat manual dengan validasi real-time

### ⏰ Manajemen Waktu
- Sinkronisasi NTP otomatis setiap 1 jam
- Multiple NTP server (pool.ntp.org, time.google.com, time.windows.com)
- Backup RTC DS3231 (opsional, sangat disarankan)
- Dukungan zona waktu UTC-12 hingga UTC+14
- Sinkronisasi manual dari browser

### 🌐 Fitur Jaringan
- WiFi Mode Ganda (AP + STA bersamaan)
- Auto-reconnect cepat tanpa polling
- Pengaturan AP custom (SSID, Password, IP)
- Monitor koneksi real-time dengan RSSI

### 🖥️ Antarmuka
- UI touchscreen LVGL 9.2.0 @ 20 FPS
- Web responsif mobile-friendly
- Tampilan real-time (jam, tanggal, waktu sholat)
- Loading manager dengan progress

### 🔊 Kontrol Buzzer & Audio
- Toggle per-waktu sholat
- Kontrol volume 0-100%
- Output PWM (GPIO26)
- Putar audio adzan dari SD Card (opsional)

### 💾 Penyimpanan
- LittleFS storage untuk semua konfigurasi
- Auto-save setelah perubahan
- Factory reset dengan countdown safety

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

### RTC DS3231 (Opsional - Sangat Disarankan)
```
DS3231       ESP32
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21
SCL     →    GPIO 22
```

**Manfaat RTC:**
- Backup waktu saat mati lampu
- Akurasi ±2ppm dengan temperature compensation
- Baterai CR2032 untuk persistensi

### SD Card (Opsional - Untuk Audio Adzan)
```
SD Card      ESP32
CS      →    GPIO 5
MOSI    →    GPIO 23
MISO    →    GPIO 19
CLK     →    GPIO 18
```
### PCM102A (Harus ada untuk menghubungkan jack audio ke amplifier)
```
PCM102A          ESP32
I2S_BCLK    →    GPIO 25
I2S_LRC     →    GPIO 32
I2S_DOUT    →    GPIO 33
```
---

## 📦 Instalasi

### 1. Kebutuhan Software

| Komponen | Versi | Wajib |
|----------|-------|-------|
| ESP32 Board | v3.0.7 | ✅ |
| LVGL | 9.2.0 | ✅ |
| TFT_eSPI | 2.5.0+ | ✅ |
| XPT2046_Touchscreen | 1.4+ | ✅ |
| ArduinoJson | 6.21.0+ | ✅ |
| ESPAsyncWebServer | 1.2.3+ | ✅ |
| AsyncTCP | 1.1.1+ | ✅ |
| TimeLib | 1.6.1+ | ✅ |
| RTClib | 2.1.1+ | ✅ |

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Cari: "esp32"
Install: esp32 by Espressif Systems v3.0.7
```

### 3. Install Library

Via Library Manager (Sketch → Include Library → Manage Libraries), install semua library di tabel kebutuhan di atas.

### 4. Konfigurasi TFT_eSPI

Edit file `Arduino/libraries/TFT_eSPI/User_Setup_Select.h`:
- Comment semua setup default
- Uncomment setup yang sesuai dengan board ESP32-2432S024

### 5. Upload

```bash
# Clone repository
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia

# Upload filesystem (folder data/)
Tools → ESP32 Sketch Data Upload

# Upload program
Sketch → Upload (Ctrl+U)
```

**Pengaturan Board:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
```

---

## 🚀 Panduan Cepat

### Langkah 1: Boot Pertama
```
AP SSID: "JWS Indonesia"
Password: "12345678"
URL: http://192.168.4.1
```

### Langkah 2: Konfigurasi WiFi
1. Sambungkan ke AP "JWS Indonesia"
2. Buka browser → `http://192.168.4.1`
3. Tab **WIFI** → Masukkan SSID dan Password WiFi rumah
4. Klik **Simpan** → Tunggu koneksi (~15 detik)

### Langkah 3: Atur Lokasi & Metode
1. Tab **LOKASI** → Pilih kota dari dropdown
2. Verifikasi/edit koordinat (opsional)
3. Pilih metode kalkulasi (default: Egyptian)
4. Klik **Simpan**

### Langkah 4: Konfigurasi Zona Waktu
```
Default: UTC+7 (WIB)

Zona Waktu Indonesia:
- WIB:  +7  (Jawa, Sumatera, Kalimantan Barat)
- WITA: +8  (Kalimantan Tengah/Timur, Sulawesi, Bali, NTB, NTT)
- WIT:  +9  (Papua, Maluku)
```

Tab **WAKTU** → Klik ikon edit (🕐) → Input offset → Simpan

### Langkah 5: Konfigurasi Buzzer
Tab **JADWAL** → Toggle ON/OFF untuk setiap waktu → Atur slider volume

---

## 🌐 Antarmuka Web

### Tab BERANDA
- Status jaringan WiFi dan IP address
- Status NTP dan server yang digunakan
- Waktu dan tanggal real-time
- Uptime perangkat
- Tombol restart perangkat

### Tab WIFI
- Konfigurasi WiFi Router (SSID & Password)
- Konfigurasi Access Point (SSID & Password)
- Konfigurasi Jaringan AP (IP, Gateway, Subnet)

### Tab WAKTU
- Pembaruan manual dari browser
- Pembaruan otomatis via NTP (setiap 1 jam)
- Pengaturan zona waktu (UTC-12 hingga UTC+14)

### Tab LOKASI
- Pilih dari 500+ kota Indonesia
- Edit koordinat GPS manual
- Pilih metode kalkulasi (8 pilihan)
- Upload cities.json baru (dengan validasi)

### Tab JADWAL
- Tampilan waktu sholat (Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya)
- Toggle buzzer per-waktu sholat
- Kontrol volume (0-100%)
- Test buzzer

### Tab RESET
- Factory reset dengan countdown 60 detik
- Menghapus semua konfigurasi
- Kembali ke pengaturan default

---

## 🔍 Troubleshooting

### WiFi Tidak Connect

**Penyebab:**
- SSID/Password salah (case-sensitive)
- Router hanya 5GHz (ESP32 hanya support 2.4GHz)
- MAC filtering aktif di router
- Sinyal terlalu lemah

**Solusi:**
1. Cek SSID dan password (perhatikan huruf besar/kecil)
2. Pastikan router broadcast di 2.4GHz
3. Tambahkan MAC address ESP32 ke whitelist router
4. Dekatkan perangkat ke router
5. Factory reset → konfigurasi ulang

**Debug:**
- Serial monitor akan menampilkan nilai RSSI
- RSSI > -50 dBm: Sangat baik
- RSSI -50 hingga -70 dBm: Baik
- RSSI < -70 dBm: Lemah (terlalu jauh)

---

### Jadwal Sholat Tidak Akurat

**Penyebab:**
- Koordinat GPS tidak tepat
- Metode kalkulasi tidak sesuai daerah

**Solusi:**
1. Edit koordinat GPS manual:
   - Buka Google Maps
   - Klik lokasi rumah/masjid
   - Salin latitude & longitude
   - Paste ke antarmuka web

2. Ganti metode kalkulasi:
   - Coba: Kemenag Indonesia (Metode 20)
   - Atau: Egyptian General Authority (Metode 5)

3. Bandingkan dengan jadwal sholat masjid terdekat

---

### Jam Reset ke 01/01/2000

**Penyebab:**
- Belum ada sinkronisasi NTP
- RTC DS3231 tidak terpasang
- Baterai RTC habis

**Solusi:**
1. Pasang modul RTC DS3231:
   - Kabel: SDA→GPIO21, SCL→GPIO22
   - Masukkan baterai CR2032
   - Power cycle perangkat

2. Sambungkan ke WiFi:
   - Perangkat akan auto NTP sync
   - Waktu akan akurat dalam 10-30 detik

3. Cek status RTC di serial monitor:
   - "DS3231 detected" → OK
   - "DS3231 not found" → Modul tidak terpasang
   - "RTC HARDWARE FAILURE" → Modul rusak, ganti baru

---

### Waktu Sholat Tidak Update Tengah Malam

**Pemeriksaan:**
- WiFi harus terhubung (perlu internet)
- NTP harus tersinkronisasi
- Kota dan koordinat harus dipilih

**Debug Serial Monitor:**
Saat 00:00-00:05 harus muncul:
```
MIDNIGHT DETECTED - STARTING SEQUENCE
Triggering NTP Sync...
NTP SYNC COMPLETED
Updating Prayer Times...
```

**Solusi:**
1. Update manual via web (Tab LOKASI → Simpan)
2. Cek koneksi internet
3. Restart perangkat jika persisten

---

### Antarmuka Web Lambat/Timeout

**Penyebab:**
- WiFi sleep aktif (seharusnya disabled)
- Sinyal lemah
- Multiple client bersamaan
- Cache browser bermasalah

**Solusi:**
1. Cek status WiFi sleep di serial monitor (harus "DOUBLE DISABLED")
2. Pindah lebih dekat ke perangkat
3. Hapus cache browser (Ctrl+Shift+Delete)
4. Gunakan mode incognito/private
5. Coba browser berbeda
6. Restart perangkat

---

### Touch Tidak Responsif

**Solusi:**
1. Bersihkan layar sentuh dengan kain microfiber
2. Kalibrasi touch (edit nilai di source code jika perlu)
3. Test dengan sentuhan lebih kuat
4. Cek kabel touchscreen

---

### Buzzer Tidak Bunyi

**Pemeriksaan:**
- Toggle buzzer diaktifkan (Tab JADWAL)
- Volume tidak 0%
- Koneksi GPIO26

**Solusi:**
1. Test buzzer manual (Tab JADWAL → Test Buzzer)
2. Cek koneksi kabel GPIO26
3. Ganti buzzer jika rusak

---

### Display Flicker/Tearing

**Penyebab:**
- Power supply kurang (<2A)
- Kabel USB buruk (voltage drop)

**Solusi:**
1. Gunakan adaptor 5V 2A atau lebih
2. Kabel USB lebih pendek (<1 meter)
3. Tambahkan kapasitor 100-470µF di input power

---

### Audio Adzan Tidak Main

**Pemeriksaan:**
- SD Card terpasang dan terdeteksi
- File audio ada di folder `/adzan/`
- Format file: WAV 44.1kHz 16-bit
- Nama file sesuai: `subuh.wav`, `zuhur.wav`, dll

**Solusi:**
1. Cek serial monitor saat boot:
   - "I2S OK" → Audio system ready
   - "SD OK" → SD Card detected
2. Format SD Card (FAT32)
3. Buat folder `/adzan/` di root SD Card
4. Copy file WAV ke folder tersebut
5. Restart perangkat

---

## 🌐 API Endpoints

### GET `/api/data`
Data sistem real-time untuk integrasi IoT (Node-RED, Home Assistant, dll)

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
    ...
  },
  "location": {
    "city": "Jakarta",
    "latitude": "-6.175392",
    "longitude": "106.827153"
  },
  "device": {
    "wifiConnected": true,
    "ntpSynced": true,
    "freeHeap": 245632,
    "uptime": 3600
  }
}
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

### Rekomendasi
1. Ganti SSID dan password AP dengan yang kuat
2. Gunakan WiFi router dengan WPA2/WPA3
3. Jangan ekspos ke internet publik (hanya jaringan lokal)
4. Pertimbangkan VLAN IoT terpisah
5. Monitor akses fisik perangkat

---

## 📊 Arsitektur Sistem

### FreeRTOS Tasks

| Task | Core | Priority | Stack | Fungsi |
|------|------|----------|-------|--------|
| UI | 1 | 3 | 12KB | Rendering LVGL @ 20 FPS |
| WiFi | 0 | 2 | 3KB | Manajemen koneksi event-driven |
| NTP | 0 | 2 | 4KB | Sinkronisasi waktu |
| Web | 0 | 1 | 5KB | AsyncWebServer |
| Prayer | 0 | 1 | 16KB | Update jadwal sholat |
| Clock | 0 | 2 | 2KB | Increment waktu sistem |
| RTC Sync | 0 | 1 | 2KB | Sync RTC ↔ Sistem |
| Audio | 1 | 0 | 4KB | Putar audio adzan |

---

**GONIT - Global Network Identification Technology**
