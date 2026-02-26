# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan antarmuka web dan backup RTC

[![Version](https://img.shields.io/badge/version-2.3-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/)

---

## 📸 Preview

<div align="center">
<img width="150" src="https://github.com/user-attachments/assets/734da9e8-efeb-4bae-b0e4-07221976d3ab" />
<img width="150" src="https://github.com/user-attachments/assets/e769b542-6f93-498c-8e5e-7e797fb66ed1" />
<img width="150" src="https://github.com/user-attachments/assets/8a1992b6-167f-4c7f-b73a-44a8e97e4fd6" />
<img width="150" src="https://github.com/user-attachments/assets/006afab9-db2d-48d8-924a-3e8af70637c2" />
<img width="150" src="https://github.com/user-attachments/assets/817a99ad-9c23-4302-963e-377add17cea0" />
<img width="150" src="https://github.com/user-attachments/assets/0fe80198-d629-43bb-b748-19a1aafea05e" />
</div>

---

## ✨ Fitur Utama

### 🕌 Waktu Sholat
- **Jadwal Otomatis** via Aladhan API dengan 8 metode kalkulasi
- **500+ Kota Indonesia** dengan koordinat GPS akurat
- **Notifikasi Visual** - LCD blink 1 menit saat waktu sholat masuk
- **Notifikasi Audio** - Buzzer atau DFPlayer Mini (MP3)
- **Touch Adzan** - Tap waktu sholat saat blink untuk play audio adzan (timeout 10 menit)
- **Toggle Individual** - Aktifkan/nonaktifkan buzzer per-waktu sholat
- **Auto-Update** - Tengah malam (00:00-00:05) otomatis update jadwal

### ⏰ Manajemen Waktu
- **NTP Auto-Sync** setiap 1 jam dengan multiple fallback server
- **Zona Waktu** - Dukungan UTC-12 hingga UTC+14 (WIB/WITA/WIT)
- **RTC Backup** - DS3231 opsional untuk persistensi waktu
- **Manual Sync** - Sync dari browser jika diperlukan

### 🌐 Fitur Jaringan
- **Dual WiFi Mode** - AP + STA bersamaan (bisa diakses via router dan AP)
- **Auto-Reconnect** - Event-driven tanpa polling
- **Custom AP** - Konfigurasi SSID, Password, IP, Gateway, Subnet
- **WiFi Sleep Disabled** - Performa maksimal, response time cepat

### 🖥️ Antarmuka
- **Touchscreen UI** - LVGL 9.2.0 @ 320x240 resolusi
- **Web Interface** - Responsive design, mobile-friendly
- **Real-time Display** - Jam, tanggal, kota, waktu sholat
- **Countdown Safety** - Visual progress bar untuk restart/reset (60 detik)

### 🔊 Audio & Buzzer
- **Buzzer PWM** - Volume 0-100%, test mode dengan auto-timeout
- **DFPlayer Mini** (opsional) - Play MP3 adzan dari SD Card
  - Volume 0-30 independen dari buzzer
  - Format: `0001.mp3` hingga `0005.mp3` (Subuh, Zuhur, Ashar, Maghrib, Isya)
  - Touch aktif 10 menit setelah waktu sholat
- **Speaker Passive** - 3-5W (4-8Ω) atau line-out ke amplifier

### ⏱️ Alarm
- **Alarm Sekali** - Atur jam dan menit via picker di web interface
- **Switch ON/OFF** - Aktifkan atau nonaktifkan tanpa menghapus waktu yang sudah diset
- **Kedip Jam** - Saat alarm berbunyi, label jam di LCD berkedip (bukan tanggal)
- **Buzzer Alarm** - Bunyi sinkron dengan kedip, volume mengikuti setting buzzer
- **Tanpa Batas** - Alarm berbunyi terus tanpa auto-stop
- **Stop via Sentuh** - Sentuh LCD mana saja untuk mematikan alarm
- **Prioritas Tinggi** - Saat alarm aktif, notif shalat ditangguhkan sementara
- **Auto-Resume** - Setelah alarm dimatikan, notif shalat kembali normal
- **Persistent** - Waktu dan status alarm tersimpan di LittleFS, tidak hilang saat restart

### 💾 Penyimpanan
- **LittleFS** - Semua konfigurasi persistent
- **Auto-Save** - Simpan otomatis setelah perubahan
- **Upload Cities** - Web interface untuk update daftar kota (max 1MB)
- **Factory Reset** - Kembalikan ke default dengan safety countdown

---

## 🔧 Hardware

### Board: ESP32-2432S024
- **MCU:** ESP32 Dual-Core @ 240MHz
- **RAM:** 520KB SRAM
- **Flash:** 4MB (minimal)
- **Display:** ILI9341 2.4" TFT (320x240)
- **Touch:** XPT2046 Resistive
- **WiFi:** 802.11 b/g/n (2.4GHz)
- **Power:** 5V USB (minimal 2A)

### Komponen Opsional

#### RTC DS3231 (Sangat Disarankan)
**Wiring:**
```
DS3231 VCC → ESP32 3.3V
DS3231 GND → ESP32 GND
DS3231 SDA → ESP32 GPIO21
DS3231 SCL → ESP32 GPIO22
Baterai CR2032 → Slot baterai RTC
```

**Manfaat:**
- Backup waktu saat mati lampu
- Akurasi ±2ppm dengan temperature compensation
- Auto-sync 1 menit sekali (RTC → Sistem)
- Validasi hardware otomatis saat boot

**⚠️ Catatan:** Tanpa RTC, waktu reset ke 01/01/2000 setiap restart hingga NTP sync berhasil.

#### DFPlayer Mini + Speaker (Untuk Audio Adzan)

**Wiring:**
```
DFPlayer Mini     ESP32-2432S024
VCC          →    5V
GND          →    GND
TX           →    GPIO32 (ESP32 RX)
RX           →    GPIO25 (ESP32 TX)
SPK_1        →    Speaker + (Passive)
SPK_2        →    Speaker - (Passive)
DAC_R/L      →    Line-out ke Amplifier
```

**Setup Audio:**
1. Format SD Card (FAT32, max 32GB)
2. **PENTING:** Jangan buat folder, file langsung di root
3. Copy file MP3 dengan nama exact:
   - `0001.mp3` = Adzan Subuh
   - `0002.mp3` = Adzan Zuhur
   - `0003.mp3` = Adzan Ashar
   - `0004.mp3` = Adzan Maghrib
   - `0005.mp3` = Adzan Isya
4. Masukkan SD Card ke DFPlayer
5. Sambungkan speaker passive (3-5W, 4-8Ω)

**⚠️ PENTING:**
- **Jangan gunakan speaker aktif** (amplifier built-in) - akan rusak
- **File naming harus exact:** 4 digit dengan leading zero
- **Folder harus root** - bukan `/mp3/` atau `/adzan/`
- **Format MP3:** 128-320 kbps, sample rate bebas

---

## 📦 Instalasi

### 1. Kebutuhan Software

| Komponen | Versi | Catatan |
|----------|-------|---------|
| ESP32 Board | v3.0.7 | ESP32 Core for Arduino |
| LVGL | 9.2.0 | Tidak kompatibel v8.x |
| TFT_eSPI | 2.5.0+ | Perlu konfigurasi manual |
| XPT2046_Touchscreen | 1.4+ | |
| ArduinoJson | 7.x | v6.x tidak kompatibel |
| ESPAsyncWebServer | 3.x | Untuk ESP32 v3.x |
| AsyncTCP | Latest | Dependency ESPAsync |
| TimeLib | 1.6.1+ | |
| RTClib | 2.1.1+ | Untuk DS3231 |
| DFRobotDFPlayerMini | 1.0.6+ | Untuk audio adzan |

**⚠️ PENTING:** Gunakan ESP32 Core v3.0.7, bukan v2.x karena breaking changes di WiFi API.

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Cari: "esp32"
Install: esp32 by Espressif Systems v3.0.7
```

### 3. Install Library

Via Library Manager (Sketch → Include Library → Manage Libraries), install semua library di tabel di atas.

### 4. Konfigurasi TFT_eSPI

Edit `Arduino/libraries/TFT_eSPI/User_Setup.h` sesuai board ESP32-2432S024 Anda:
- Driver: ILI9341
- Resolution: 320x240
- Pins: MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, BL=27

### 5. Generate UI dengan EEZ Studio

**⚠️ PENTING:** File UI (`src/ui.c`, `src/ui.h`, dll) tidak included di repository.

**Cara Generate:**
1. Download EEZ Studio: https://github.com/eez-open/studio
2. Buka file proyek `.eez-project`
3. Build → Generate Code for LVGL
4. Copy folder `src/` ke folder sketch

### 6. Upload Filesystem (LittleFS)

**Install Plugin:**
1. Download: https://github.com/lorol/arduino-esp32littlefs-plugin/releases
2. Extract ke `Arduino/tools/`
3. Restart Arduino IDE

**Upload Data:**
1. Buat folder `data/` di root sketch
2. Copy files:
   - `index.html`
   - `css/foundation.min.css`
   - `cities.json`
3. Tools → ESP32 Sketch Data Upload

**⚠️ Upload filesystem dulu sebelum upload sketch!**

### 7. Upload Sketch

**Pengaturan Board:**
```
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (2MB APP / 2MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
```

---

## 🚀 Panduan Cepat

### Boot Pertama
```
AP SSID: "JWS-(id unik)"
Password: "12345678"
URL: http://192.168.100.1
```

### Setup WiFi
1. Sambungkan ke AP "JWS-(id unik)"
2. Buka browser → `http://192.168.100.1`
3. Tab **WIFI** → Input SSID dan Password WiFi rumah
4. Klik **Simpan** → Tunggu ~30 detik

### Setup Lokasi
1. Tab **LOKASI** → Pilih provinsi → Pilih kota
2. (Opsional) Edit koordinat GPS jika perlu
3. Pilih metode kalkulasi (default: Egyptian General Authority)
4. Klik **Simpan** → Jadwal otomatis update

### Setup Zona Waktu
```
Default: UTC+7 (WIB)

Indonesia:
- WIB:  +7  (Jawa, Sumatera, Kalimantan Barat)
- WITA: +8  (Kalimantan Tengah/Timur, Sulawesi, Bali, NTB, NTT)
- WIT:  +9  (Papua, Maluku)
```

Tab **WAKTU** → Klik ikon edit (🕐) → Input offset → Klik 💾

### Setup Buzzer & Audio
1. Tab **JADWAL** → Toggle ON/OFF per-waktu sholat
2. Atur slider volume (0-100%)
3. Klik **Test Buzzer** untuk tes
4. **Setup Audio** (jika ada DFPlayer):
   - Format SD Card (FAT32)
   - Copy `0001.mp3` s/d `0005.mp3` di root
   - Reboot perangkat
   - **Play:** Tap label waktu sholat saat blink

### Setup Alarm
1. Tab **JADWAL** → Scroll ke bagian **⏱ Atur Alarm**
2. Putar picker untuk set jam dan menit yang diinginkan
3. Klik **Simpan Alarm** → waktu tersimpan ke perangkat
4. Nyalakan switch **ON** di sebelah tombol simpan
5. Alarm aktif — saat waktunya tiba, jam di LCD akan berkedip dan buzzer berbunyi
6. **Matikan:** Sentuh LCD mana saja untuk menghentikan alarm

**Catatan:**
- Simpan waktu dan toggle ON/OFF adalah dua aksi terpisah
- Volume alarm mengikuti slider volume buzzer
- Alarm tersimpan permanen, tidak hilang saat restart
- Default waktu: `00:00` dengan status OFF

---

## 🌐 Web Interface

### Tab BERANDA
- Status koneksi WiFi dan IP
- Status NTP dan server yang digunakan
- Waktu dan tanggal real-time
- Uptime perangkat
- Tombol restart perangkat

### Tab WIFI
- **WiFi Router:** SSID dan Password
- **Access Point:** Custom SSID, Password, IP, Gateway, Subnet
- Validasi format otomatis
- Restart dengan countdown 60 detik

### Tab WAKTU
- **Manual Sync:** Sync dari browser
- **Auto NTP:** Sync setiap 1 jam, multiple fallback server
- **Zona Waktu:** Inline edit UTC-12 hingga UTC+14
- Auto-trigger NTP saat zona waktu berubah

### Tab LOKASI
- **Dropdown:** 500+ kota Indonesia, grouping per-provinsi
- **Edit Koordinat:** Manual input lat/lon dengan validasi
- **Metode:** 8 pilihan kalkulasi (Kemenag, MWL, Egyptian, ISNA, dll)
- **Upload:** Drag & drop cities.json (max 1MB, validasi otomatis)

### Tab JADWAL
- **Waktu Sholat:** Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya
- **Toggle Buzzer:** ON/OFF per-waktu
- **Volume:** Slider 0-100% dengan preview
- **Test Buzzer:** Play/stop manual (auto-timeout 30s)
- **Touch Adzan:** Tap label saat blink untuk play audio
- **Atur Alarm:** Picker jam:menit, tombol Simpan Alarm, switch ON/OFF

### Tab RESET
- **Factory Reset:** Countdown 60 detik dengan progress bar
- Hapus semua konfigurasi, kembalikan ke default
- Auto-redirect ke 192.168.100.1

---

## ⏱️ Alarm — Detail Perilaku

### Cara Kerja
1. Setiap detik sistem membandingkan waktu sekarang dengan `alarmTime`
2. Jika cocok dan alarm ON → `alarmState.isRinging = true`
3. Label jam (`time_now`) mulai berkedip setiap 500ms — **hanya jam, bukan tanggal**
4. Buzzer berbunyi sinkron dengan kedip (ON saat jam tampil, OFF saat jam hilang)
5. Alarm berjalan **tanpa batas waktu** sampai layar disentuh
6. Sentuh LCD mana saja → alarm berhenti, jam tampil normal kembali

### Prioritas vs Notif Shalat
| Kondisi | Notif Shalat | Alarm |
|---------|-------------|-------|
| Alarm OFF, jam sholat masuk | ✅ Berjalan normal | Diam |
| Alarm ON, belum waktunya | ✅ Berjalan normal | Diam |
| Alarm ON, waktunya tiba | ⏸️ Ditangguhkan | ✅ Berbunyi |
| Alarm dimatikan (sentuh LCD) | ✅ Kembali normal | Diam |

### Jika Alarm dan Waktu Sholat Bersamaan
Alarm mengambil prioritas — notif shalat di menit yang sama tidak akan muncul. Ini perilaku yang disengaja, bukan bug.

### Penyimpanan
Konfigurasi alarm disimpan di `/alarm_config.txt` di LittleFS:
```
06:00       ← alarmTime (HH:MM)
1           ← alarmEnabled (1=ON, 0=OFF)
```
File ini tidak terhapus saat restart, hanya terhapus saat Factory Reset.

---

## 🔍 Troubleshooting

### WiFi Tidak Connect

**Penyebab:**
- SSID/Password salah (case-sensitive)
- Router hanya 5GHz (ESP32 hanya 2.4GHz)
- MAC filtering aktif
- Sinyal terlalu lemah
- Channel WiFi >11 (coba 1-11)

**Solusi:**
1. Cek SSID dan password (huruf besar/kecil)
2. Pastikan router broadcast 2.4GHz
3. Tambahkan MAC ESP32 ke whitelist
4. Dekatkan ke router
5. Ganti channel router ke 1-11
6. Factory reset → konfigurasi ulang

**Serial Monitor:**
```
WiFi: Connected | RSSI: -45 dBm | IP: 192.168.1.100  → ✅ Baik
WiFi: Connected | RSSI: -75 dBm                      → ⚠️ Lemah
WiFi Disconnected | Reason Code: 15                  → ❌ Gagal
```

**RSSI Guide:**
- -50 dBm: Sangat baik
- -60 dBm: Baik
- -70 dBm: Cukup
- -80 dBm: Lemah (terlalu jauh)

---

### Jadwal Sholat Tidak Akurat

**Solusi:**

1. **Edit Koordinat GPS:**
   - Buka Google Maps → Klik lokasi → Salin koordinat
   - Tab LOKASI → Edit Koordinat → Paste
   - Klik **Simpan**

2. **Ganti Metode:**
   - Tab LOKASI → Dropdown Metode
   - Coba: **Kemenag** (Metode 20) - paling akurat Indonesia
   - Atau: **Egyptian** (Metode 5) - alternatif bagus
   - Klik **Simpan**

3. **Cek Zona Waktu:**
   - Tab WAKTU → Pastikan sesuai lokasi (WIB/WITA/WIT)

4. **Bandingkan:**
   - Download app Muslim Pro/Adzan
   - Bandingkan dengan jadwal masjid
   - Adjust koordinat jika perlu (±0.01° ≈ ±1km)

**Serial Monitor:**
```
Fetching prayer times...
Response code: 200              → ✅ OK
Prayer times updated successfully
City: Jakarta
Subuh: 04:07  Zuhur: 11:54
```

---

### Jam Reset ke 01/01/2000

**Penyebab:**
- NTP belum sync (WiFi belum connect)
- RTC tidak terpasang
- Baterai RTC habis
- RTC hardware failure

**Solusi:**

1. **Sambungkan WiFi:**
   - NTP akan sync otomatis dalam 10-30 detik

2. **Pasang RTC DS3231:**
   - Wiring: VCC→3.3V, GND→GND, SDA→GPIO21, SCL→GPIO22
   - Pasang baterai CR2032
   - Reboot perangkat

3. **RTC Rusak:**
```
Serial Monitor:
*** RTC HARDWARE FAILURE ***
>>> SOLUTION: BUY NEW DS3231 MODULE <
```
Ganti modul baru - tidak bisa diperbaiki.

4. **Temporary Fix:**
   - Tab WAKTU → Tombol "Perbarui Waktu"
   - Atau tunggu WiFi connect → NTP auto-sync

**⚠️ Tanpa RTC:** Waktu reset setiap restart/mati lampu.

---

### Waktu Sholat Tidak Update Tengah Malam

**Serial Monitor (00:00-00:05):**

**✅ NORMAL:**
```
MIDNIGHT DETECTED - STARTING SEQUENCE
Triggering NTP Sync...
NTP SYNC COMPLETED
Updating Prayer Times...
Prayer times updated successfully
```

**❌ GAGAL:**

1. **WiFi Tidak Connect:**
```
ERROR: NTP Task handle NULL
Skipping midnight update
```
**Fix:** Cek Tab BERANDA → Status WiFi

2. **Kota Belum Dipilih:**
```
WARNING: No city coordinates
Using existing prayer times
```
**Fix:** Tab LOKASI → Pilih kota → Simpan

3. **NTP Timeout:**
```
NTP SYNC TIMEOUT
Decision: Use existing prayer times
```
**Fix:** Akan retry di 00:01, 00:02, dst. Cek internet.

**Manual Update:**
- Tab LOKASI → Klik **Simpan** (force update)

---

### Alarm Tidak Berbunyi

**Pemeriksaan:**
1. Switch alarm sudah ON di Tab JADWAL
2. Waktu alarm sudah di-**Simpan** (bukan hanya diatur di picker)
3. Volume buzzer tidak 0%
4. Waktu sistem sudah benar (cek Tab BERANDA)

**Debug Serial Monitor:**
```
✅ NORMAL:
Konfigurasi alarm dimuat: 06:00 | ON
ALARM AKTIF: 06:00
Kedip jam + buzzer dimulai

❌ TIDAK BERBUNYI:
Konfigurasi alarm dimuat: 06:00 | OFF  → Switch belum ON
```

**Catatan:** Simpan Alarm dan switch ON/OFF adalah dua aksi terpisah. Pastikan keduanya dilakukan.

---

### Alarm Tidak Bisa Dimatikan

**Solusi:**
- Sentuh bagian mana saja di layar LCD dengan tekanan cukup (touch resistive)
- Bersihkan layar jika kotor
- Pastikan tidak ada objek di atas layar

---

### Alarm Mengganggu Notif Sholat

Ini perilaku normal jika alarm berbunyi di menit yang sama dengan waktu sholat — alarm selalu menang. Solusinya set alarm di waktu yang berbeda dari waktu sholat.

---

### Web Interface Lambat/Timeout

**Penyebab:**
- WiFi sleep aktif (bug)
- Sinyal lemah
- Multiple client (>3)
- Cache browser

**Solusi:**

1. **Cek WiFi Sleep (Serial Monitor saat boot):**
```
WiFi Sleep: DOUBLE DISABLED
  Arduino: WIFI_PS_NONE    → ✅ OK
  ESP-IDF: WIFI_PS_NONE    → ✅ OK
```

2. **Dekatkan ke Router:**
   - RSSI harus > -60 dBm

3. **Clear Cache:**
   - Chrome: Ctrl+Shift+Delete → Cache
   - Atau: Mode Incognito (Ctrl+Shift+N)

4. **Batasi Client:**
   - Max 3 client untuk performa optimal

5. **Restart:**
   - Tab BERANDA → Mulai Ulang Perangkat

**Memory Status (Serial Monitor tiap 2 menit):**
```
STACK USAGE ANALYSIS
Prayer    :  9887/16384 (60.3%) [Free:  6497] OPTIMAL → ✅ Sehat

MEMORY STATUS:
Current:  245632 bytes (239.88 KB)
Lowest:   243520 bytes (238.00 KB)
Memory status: Normal
```

**Jika ada LEAK:**
```
LEAK DETECTED: 1024 bytes lost
```
**Fix:** Restart, report ke developer.

---

### Touch Tidak Responsif

**Penyebab:**
- Layar kotor
- Tekanan kurang
- Konflik pin audio

**Solusi:**
1. Bersihkan layar microfiber
2. Sentuh lebih kuat (resistive touch)
3. Cek Serial Monitor:
```
Touch initialized    → ✅ OK
```

---

### Audio Adzan Tidak Play (DFPlayer)

**Serial Monitor saat boot:**
```
✅ NORMAL:
INITIALIZING DFPlayer Mini
UART2: TX=GPIO25, RX=GPIO32
DFPlayer initialized successfully!
Files on SD: 7

❌ GAGAL:
DFPlayer connection FAILED!
Check wiring
```

**Checklist:**

1. **Wiring (HARUS SILANG):**
   - DFPlayer TX → ESP32 GPIO32 (RX) ✅
   - DFPlayer RX → ESP32 GPIO25 (TX) ✅
   - VCC → 5V, GND → GND

2. **SD Card:**
   - Format: FAT32 (max 32GB)
   - Files di root: `0001.mp3` s/d `0005.mp3`
   - Naming: 4 digit dengan leading zero

3. **Speaker:**
   - Passive 3-5W (4-8Ω)
   - JANGAN speaker aktif!

4. **Cara Play:**
   - Tunggu waktu sholat → Label blink
   - Tap area label (misal tap "SUBUH")
   - Audio play otomatis

5. **Timeout:** 10 menit setelah waktu masuk

**Debug:**
```
✅ NORMAL:
TOUCH ADZAN: subuh
PLAYING ADZAN: subuh
Track: 1, File: /0001.mp3
Adzan playback completed

❌ ERROR:
WARNING: Audio system not available
Reason: SD Card not detected
```

---

### Buzzer Tidak Bunyi

**Pemeriksaan:**
- Toggle aktif di Tab JADWAL
- Volume tidak 0%
- Pin GPIO26 OK

**Test Manual:**
- Tab JADWAL → Test Buzzer
- Harus bunyi beep-beep 30 detik

**Debug Serial:**
```
PRAYER TIME ENTER: SUBUH
Starting to blink for 1 minute...
```

**Solusi:**
1. Test buzzer di Tab JADWAL
2. Cek wiring: Buzzer(+)→GPIO26, (-)→GND
3. Ganti buzzer jika rusak

---

### Display Flicker/Tearing

**Penyebab:**
- Power supply <2A
- Kabel USB jelek
- SPI speed tinggi

**Solusi:**
1. Gunakan adaptor 5V 2A minimum
2. Kabel USB pendek (<1m, kualitas bagus)
3. Tambah kapasitor 100-470µF di VIN-GND

---

### RTC Time Invalid

**Serial Monitor:**
```
*** RTC HARDWARE FAILURE ***
DS3231 chip is defective!
>>> SOLUTION: BUY NEW DS3231 MODULE <
```

**Tidak bisa diperbaiki - ganti modul baru!**

**Tanpa RTC:**
- Sistem tetap jalan
- NTP sync otomatis saat WiFi connect
- Waktu reset setiap restart

---

### Prayer Task Crash

**Serial Monitor:**
```
CRITICAL: PRAYER TASK CRASHED
Action: Auto-restarting task...
Prayer Task restarted successfully
```

**Sistem auto-recovery - task restart otomatis!**

**Jika crash berulang:**
```
ERROR: Insufficient stack for HTTP request!
Stack available: 1800 bytes → ⚠️ Terlalu kecil
```

Increase stack size di kode atau report ke developer.

---

### Upload cities.json Gagal

**Error:**
- "Invalid filename" → Nama harus `cities.json`
- "File too large" → Max 1MB
- "Invalid JSON" → Format rusak

**Validasi:**
1. Nama: `cities.json` (lowercase, no space)
2. Size: Max 1MB
3. Format: Valid JSON array
4. Required fields: `api`, `display`, `province`

**Debug Serial:**
```
CITIES.JSON UPLOAD STARTED
Progress: 5120 bytes (5.0 KB)
Upload complete
Total size: 45632 bytes
File verified: 45632 bytes
JSON format looks valid
```

---

## 🛡️ Keamanan

### Kredensial Default
```
⚠️ GANTI SETELAH BOOT PERTAMA ⚠️

AP SSID:     JWS-(id unik)
AP Password: 12345678
AP IP:       192.168.100.1
```

### Rekomendasi
1. **Ganti SSID dan Password AP** segera
2. **Gunakan WPA2/WPA3** di router
3. **Jangan ekspos ke internet** - hanya akses lokal
4. **Monitor akses fisik** - Serial port bisa untuk debug/hack
5. **Backup konfigurasi** - Screenshot semua setting
6. **Update firmware** - Check repository berkala

---

## 📊 API Endpoints

### GET `/api/data`
Real-time data untuk integrasi IoT:
```json
{
  "time": "14:35:22",
  "date": "19/12/2024",
  "prayerTimes": {
    "subuh": "04:07",
    "zuhur": "11:54",
    ...
  },
  "location": {
    "city": "Jakarta",
    "latitude": "-6.2088",
    "longitude": "106.8456"
  },
  "device": {
    "wifiConnected": true,
    "ntpSynced": true,
    "uptime": 3600
  }
}
```

**Use Case:** Home Assistant, Node-RED, Mobile App

### GET `/api/countdown`
Status countdown restart/reset:
```json
{
  "active": true,
  "remaining": 45,
  "message": "Memulai Ulang Perangkat",
  "reason": "device_restart"
}
```

### POST Endpoints
- `/setwifi` - Set WiFi credentials
- `/setap` - Set AP config
- `/setcity` - Set lokasi dan koordinat
- `/synctime` - Manual time sync
- `/restart` - Restart perangkat (countdown 60s)
- `/reset` - Factory reset (countdown 60s)
- `/setalarmconfig` - Set waktu alarm (`alarmTime=HH:MM`)
- `/setbuzzertoggle` - Toggle alarm ON/OFF (`prayer=alarm&enabled=true/false`)

### GET Endpoints
- `/getalarmconfig` - Baca konfigurasi alarm saat ini
- `/getbuzzerconfig` - Baca semua konfigurasi buzzer termasuk alarm

---

## 🙏 Credits

**Developer:** GONIT - Global Network Identification Technology

**Libraries:**
- LVGL Team - https://lvgl.io/
- Espressif Systems - ESP32 Arduino Core
- Bodmer - TFT_eSPI
- Benoit Blanchon - ArduinoJson
- me-no-dev - ESPAsyncWebServer
- Adafruit - RTClib
- DFRobot - DFPlayerMini

**API:** Aladhan API - https://aladhan.com/

---

**© 2025 GONIT - Global Network Identification Technology**
