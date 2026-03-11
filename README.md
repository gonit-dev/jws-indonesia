# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan antarmuka web dan backup RTC

[![Version](https://img.shields.io/badge/version-2.3-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/)

---

## 📸 Preview

<div align="center">
<img width="250" src="https://github.com/user-attachments/assets/734da9e8-efeb-4bae-b0e4-07221976d3ab" />
<img width="250" src="https://github.com/user-attachments/assets/e769b542-6f93-498c-8e5e-7e797fb66ed1" />
<img width="250" src="https://github.com/user-attachments/assets/8a1992b6-167f-4c7f-b73a-44a8e97e4fd6" />
<img width="250" src="https://github.com/user-attachments/assets/006afab9-db2d-48d8-924a-3e8af70637c2" />
<img width="250" src="https://github.com/user-attachments/assets/8d868967-0f09-4dcf-a712-5e53ea7de482" />
<img width="250" src="https://github.com/user-attachments/assets/0fe80198-d629-43bb-b748-19a1aafea05e" />
</div>

---

## ✨ Fitur Utama

### 🕌 Waktu Sholat
- **Jadwal Otomatis** via Aladhan API dengan 8 metode kalkulasi
- **500+ Kota Indonesia** dengan koordinat GPS akurat
- **Tune/Offset Per Waktu** — sesuaikan maju/mundur tiap waktu sholat dalam menit
- **Notifikasi Visual** — LCD blink 1 menit saat waktu sholat masuk
- **Notifikasi Audio** — Buzzer atau DFPlayer Mini (MP3)
- **Touch Adzan** — Tap waktu sholat saat blink untuk play audio adzan (timeout 10 menit)
- **Toggle Individual** — Aktifkan/nonaktifkan buzzer per-waktu sholat
- **Auto-Update** — Tengah malam (00:00–00:05) otomatis update jadwal
- **Guard Data Kosong** — Notifikasi tidak jalan jika semua waktu sholat masih `00:00` (belum ada data dari API)

### ⏰ Manajemen Waktu
- **NTP Auto-Sync** setiap 1 jam dengan 3 fallback server (`pool.ntp.org`, `time.google.com`, `time.windows.com`)
- **Zona Waktu** — Dukungan UTC-12 hingga UTC+14 (WIB/WITA/WIT)
- **RTC Backup** — DS3231 opsional untuk persistensi waktu
- **Manual Sync** — Sync dari browser jika diperlukan

### 🌐 Fitur Jaringan
- **Dual WiFi Mode** — AP + STA bersamaan (bisa diakses via router dan AP)
- **Auto-Reconnect** — Event-driven tanpa polling, hingga 5 percobaan
- **WiFi Failed + Exponential Backoff** — Setelah 5x gagal, retry otomatis dengan jeda makin panjang (10s → 20s → 40s → 60s → 120s maks), retry selamanya sampai berhasil
- **Custom AP** — Konfigurasi SSID, Password, IP, Gateway, Subnet
- **WiFi Sleep Disabled** — Performa maksimal, response time cepat
- **Connection Type Detection** — Deteksi otomatis apakah client akses via AP atau router
- **Internet Check** — Cek koneksi internet setiap 30 detik via TCP ke `8.8.8.8:53`

### 🔴 RGB LED Status Indicator
- **Boot** — Merah kedip cepat selama proses booting berlangsung
- **WiFi Connecting** — Hijau kedip cepat (300ms) saat sedang menghubungkan ke router
- **WiFi Connected + Internet OK** — Hijau nyala solid
- **WiFi Connected + Internet Putus** — Hijau kedip lambat (500ms), seperti router saat LAN hilang
- **WiFi Failed** — Merah nyala saat gagal konek ke router (setelah 5x percobaan)
- **Restart/Reset Countdown** — Merah kedip saat countdown `device_restart` atau `factory_reset`

> Cek internet dilakukan setiap 30 detik via TCP connect ke `8.8.8.8:53` (DNS Google) — ringan, tidak ada data yang dikirim.

### 🖥️ Antarmuka
- **Touchscreen UI** — LVGL 9.2.0 @ 320x240 resolusi
- **Web Interface** — Responsive design, mobile-friendly
- **Real-time Display** — Jam, tanggal, kota, waktu sholat
- **Countdown Safety** — Visual progress bar untuk restart/reset (60 detik)

### 🔊 Audio & Buzzer
- **Buzzer PWM** — Volume 0–100%, test mode dengan auto-timeout
- **DFPlayer Mini** (opsional) — Play MP3 adzan dari SD Card
  - Volume 0–30 independen dari buzzer
  - Format: `0001.mp3` hingga `0005.mp3` (Subuh, Zuhur, Ashar, Maghrib, Isya)
  - Touch aktif 10 menit setelah waktu sholat
  - State adzan persistent — bertahan saat restart
- **Speaker Passive** — 3–5W (4–8Ω) atau line-out ke amplifier

### ⏱️ Alarm
- **Alarm Sekali** — Atur jam dan menit via picker di web interface
- **Switch ON/OFF** — Aktifkan atau nonaktifkan tanpa menghapus waktu yang sudah diset
- **Kedip Jam** — Saat alarm berbunyi, label jam di LCD berkedip (bukan tanggal)
- **Buzzer Alarm** — Bunyi sinkron dengan kedip, volume mengikuti setting buzzer
- **Tanpa Batas** — Alarm berbunyi terus tanpa auto-stop
- **Stop via Sentuh** — Sentuh LCD mana saja untuk mematikan alarm
- **Prioritas Tinggi** — Saat alarm aktif, notif shalat ditangguhkan sementara
- **Auto-Resume** — Setelah alarm dimatikan, notif shalat kembali normal
- **Persistent** — Waktu dan status alarm tersimpan di LittleFS, tidak hilang saat restart

### 💾 Penyimpanan
- **LittleFS** — Semua konfigurasi persistent
- **Auto-Create Default** — File konfigurasi default dibuat otomatis saat boot pertama
- **Auto-Save** — Simpan otomatis setelah perubahan
- **Upload Cities** — Web interface untuk update daftar kota (max 1MB)
- **Factory Reset** — Kembalikan ke default dengan safety countdown

### 🛡️ Stabilitas Sistem
- **Prayer Task Watchdog** — Monitor task sholat setiap 30 detik, auto-restart jika crash
- **Stack Monitoring** — Laporan penggunaan stack setiap 60 detik
- **Memory Monitoring** — Laporan heap setiap 30 detik, deteksi memory leak
- **Hardware Watchdog** — ESP32 WDT dengan timeout 100 detik

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

#### RGB LED (Indikator Status)
**Wiring:**
```
LED RGB Common Anode
Anoda (+) → 3.3V (melalui resistor)
R (Merah) → GPIO4
G (Hijau) → GPIO16
B (Biru)  → GPIO17
```

| Kondisi | Warna | Mode |
|---------|-------|------|
| Booting | Merah | Kedip cepat (300ms) |
| Boot selesai, WiFi tidak dikonfigurasi | — | Mati |
| Sedang connecting ke router | Hijau | Kedip cepat (300ms) |
| Terkoneksi ke router + internet OK | Hijau | Nyala solid |
| Terkoneksi ke router + internet putus | Hijau | Kedip lambat (500ms) |
| Gagal konek ke router (WIFI_FAILED) | Merah | Nyala |
| Countdown restart/factory reset | Merah | Kedip (500ms) |

> **Cek Internet:** Setiap 30 detik via TCP connect ke `8.8.8.8:53`. Jika WiFi putus, status internet otomatis direset ke false. Countdown `ap_restart` tidak mempengaruhi LED.

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
5. Sambungkan speaker passive (3–5W, 4–8Ω)

**⚠️ PENTING:**
- **Jangan gunakan speaker aktif** (amplifier built-in) — akan rusak
- **File naming harus exact:** 4 digit dengan leading zero
- **Folder harus root** — bukan `/mp3/` atau `/adzan/`
- **Format MP3:** 128–320 kbps, sample rate bebas

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

> **LED saat boot pertama:** Merah kedip → setelah setup selesai LED mati → hijau nyala saat WiFi berhasil konek ke router.

### Setup WiFi
1. Sambungkan ke AP "JWS-(id unik)"
2. Buka browser → `http://192.168.100.1`
3. Tab **WIFI** → Input SSID dan Password WiFi rumah
4. Klik **Simpan** → Tunggu ~30 detik

### Setup Lokasi
1. Tab **LOKASI** → Pilih provinsi → Pilih kota
2. (Opsional) Edit koordinat GPS jika perlu
3. (Opsional) Atur **Tune** per waktu sholat jika jadwal kurang akurat
4. Pilih metode kalkulasi (default: Egyptian General Authority)
5. Klik **Simpan** → Jadwal otomatis update

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
2. Atur slider volume (0–100%)
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
- **Status WiFi** — badge realtime: Terhubung / Menghubungkan... / Gagal / Tidak terkonfigurasi
- **Sinyal** — kekuatan sinyal dalam dBm + label kualitas (Sangat Baik / Baik / Cukup / Lemah)
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
- **Auto NTP:** Sync setiap 1 jam, 3 fallback server (`pool.ntp.org`, `time.google.com`, `time.windows.com`)
- **Zona Waktu:** Inline edit UTC-12 hingga UTC+14
- Auto-trigger NTP saat zona waktu berubah

### Tab LOKASI
- **Dropdown:** 500+ kota Indonesia, grouping per-provinsi
- **Edit Koordinat:** Manual input lat/lon dengan validasi
- **Tune Offset:** Sesuaikan waktu sholat maju/mundur per-waktu (satuan menit)
- **Metode:** 8 pilihan kalkulasi (Kemenag, MWL, Egyptian, ISNA, dll)
- **Upload:** Drag & drop cities.json (max 1MB, validasi otomatis)

### Tab JADWAL
- **Waktu Sholat:** Imsak, Subuh, Terbit, Zuhur, Ashar, Maghrib, Isya
- **Toggle Buzzer:** ON/OFF per-waktu
- **Volume:** Slider 0–100% dengan preview
- **Test Buzzer:** Play/stop manual (auto-timeout 30s)
- **Touch Adzan:** Tap label saat blink untuk play audio
- **Atur Alarm:** Picker jam:menit, tombol Simpan Alarm, switch ON/OFF

### Tab RESET
- **Factory Reset:** Countdown 60 detik dengan progress bar
- Hapus semua konfigurasi, kembalikan ke default
- Auto-redirect ke 192.168.100.1

---

## ⚙️ Konfigurasi Default (Auto-Create saat Boot)

Setiap boot, sistem otomatis mengecek dan membuat file konfigurasi jika belum ada. File yang sudah ada **tidak akan ditimpa**.

| File | Isi Default |
|------|-------------|
| `/ap_creds.txt` | SSID: `JWS-<MAC>`, Password: `12345678`, IP: `192.168.100.1` |
| `/timezone.txt` | `7` (UTC+7 / WIB) |
| `/buzzer_config.txt` | Semua OFF, volume 50 |
| `/alarm_config.txt` | `00:00`, disabled |
| `/method_selection.txt` | ID: 5, Egyptian General Authority of Survey |

File berikut **tidak dibuat otomatis** karena harus diisi/dipilih user:

| File | Alasan |
|------|--------|
| `/wifi_creds.txt` | Diisi user via web interface |
| `/city_selection.txt` | Harus dipilih user via web interface |
| `/prayer_times.txt` | Diisi otomatis setelah fetch API |
| `/adzan_state.txt` | Runtime state, dibuat/dihapus otomatis |

**Serial Monitor saat boot:**
```
========================================
CEK DEFAULT CONFIG FILES
========================================
[DIBUAT] /ap_creds.txt        ← Boot pertama
[ADA]   /timezone.txt - dilewati    ← Boot berikutnya
[ADA]   /buzzer_config.txt - dilewati
...
========================================
```

---

## ⏱ Alarm — Detail Perilaku

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

### Penyimpanan
```
/alarm_config.txt:
06:00       ← alarmTime (HH:MM)
1           ← alarmEnabled (1=ON, 0=OFF)
```

---

## 🕐 Tune / Offset Waktu Sholat

Setiap waktu sholat dapat disesuaikan maju atau mundur dalam satuan menit.

- Nilai **positif** → waktu dimajukan
- Nilai **negatif** → waktu dimundurkan
- Default semua = `0`

Tune dikirim ke Aladhan API sebagai parameter `tune` sehingga perhitungan langsung disesuaikan dari server. Nilai disimpan di `/city_selection.txt`.

**Contoh penggunaan:** Jika Subuh di daerah Anda selalu 2 menit lebih cepat dari hasil API, set tuneSubuh = `-2`.

---

## 🌐 WiFi & Koneksi Internet

### Mekanisme Reconnect

Saat WiFi terputus, sistem mencoba reconnect secara otomatis hingga **5 kali**. Jika semua gagal, state berubah ke `WIFI_FAILED` dan sistem masuk mode **exponential backoff retry**:

| Retry ke- | Jeda Tunggu |
|-----------|------------|
| 1 | 10 detik |
| 2 | 20 detik |
| 3 | 40 detik |
| 4 | 60 detik |
| 5+ | 120 detik (maksimal, retry selamanya) |

Saat WiFi hidup kembali, sistem otomatis terkoneksi dan semua counter direset. LED berubah dari merah nyala ke hijau solid (jika internet OK) atau hijau kedip (jika internet belum tersedia).

### Cek Internet
Setiap 30 detik sistem cek koneksi internet via **TCP connect ke `8.8.8.8:53`** (DNS Google). Tidak ada data yang dikirim — hanya handshake untuk verifikasi routing aktif. Jika WiFi putus, status internet otomatis direset ke false.

---

## 🛡️ Stabilitas Sistem

### Prayer Task Watchdog
Task sholat dipantau setiap 30 detik oleh task `PrayerWatchdog`. Jika crash terdeteksi, task di-restart otomatis beserta re-registrasi WDT hardware.

```
KRITIS: TUGAS SHALAT CRASH
Aksi: Memulai ulang tugas otomatis...
Tugas Shalat berhasil dimulai ulang
WDT: Re-registered
```

### Guard Data Waktu Sholat
Notifikasi sholat (LCD blink, buzzer, adzan) **tidak akan jalan** jika semua waktu sholat masih `00:00`. Kondisi ini terjadi saat:
- Setelah factory reset sebelum restart
- Boot pertama sebelum kota dipilih dan data di-fetch dari API
- Setelah restart sebelum internet tersedia

Notifikasi baru aktif setelah data berhasil diambil dari API dan minimal satu waktu sholat berbeda dari `00:00`.

### Stack & Memory Monitoring

**Stack Report (setiap 60 detik):**
```
UI        :  8192/12288 (66.7%) [Free:  4096] FIT
Web       :  2048/ 4096 (50.0%) [Free:  2048] OPTIMAL
Prayer    :  2500/ 4096 (61.0%) [Free:  1596] FIT
```

| Persentase | Status |
|-----------|--------|
| < 40% | BOROS |
| 40–60% | OPTIMAL |
| 60–75% | FIT |
| 75–90% | HIGH — monitor terus |
| 90–95% | DANGER — harus ditambah |
| > 95% | CRITICAL |

**Memory Report (setiap 30 detik):**
```
Sekarang:  245632 byte (239.88 KB)
Terendah:  243520 byte (238.00 KB)
Memory status: Normal
```

---

## 📊 API Endpoints Lengkap

### GET Endpoints

| Endpoint | Deskripsi |
|----------|-----------|
| `/` | Web interface utama |
| `/devicestatus` | Status lengkap perangkat |
| `/getwificonfig` | Konfigurasi WiFi & AP |
| `/gettimezone` | Offset timezone aktif |
| `/getcities` | Daftar kota dari `cities.json` |
| `/getcityinfo` | Info kota yang sedang dipilih |
| `/getmethod` | Metode kalkulasi aktif |
| `/getprayertimes` | Waktu sholat hari ini |
| `/getbuzzerconfig` | Konfigurasi buzzer + alarm |
| `/getalarmconfig` | Konfigurasi alarm saja |
| `/api/data` | Data real-time (IoT/Home Assistant) |
| `/api/countdown` | Status countdown restart/reset/AP restart |
| `/api/connection-type` | Tipe koneksi client (AP/STA) |

### POST Endpoints

| Endpoint | Parameter | Deskripsi |
|----------|-----------|-----------|
| `/restart` | — | Restart perangkat (countdown 60s) |
| `/reset` | — | Factory reset (countdown 60s) |
| `/setwifi` | `ssid`, `password` | Set kredensial router |
| `/setap` | Mode 1 (SSID/Pass): `ssid`, `password` | Set SSID dan password AP |
| `/setap` | Mode 2 (Network): `updateNetworkConfig=true`, `apIP`, `gateway`, `subnet` | Set IP, Gateway, Subnet AP |
| `/synctime` | — | Manual NTP sync |
| `/settimezone` | `offset` | Set UTC offset |
| `/setcity` | `city`, `cityName`, `lat`, `lon`, `tuneImsak`, `tuneSubuh`, `tuneTerbit`, `tuneZuhur`, `tuneAshar`, `tuneMaghrib`, `tuneIsya` | Set lokasi + tune per waktu |
| `/setmethod` | `methodId`, `methodName` | Set metode kalkulasi |
| `/setbuzzertoggle` | `prayer` (imsak/subuh/terbit/zuhur/ashar/maghrib/isya/alarm), `enabled` (true/false) | Toggle notif per-waktu atau alarm |
| `/setbuzzervolume` | `volume` (0–100) | Set volume buzzer |
| `/testbuzzer` | — | Test buzzer (auto-stop 30 detik) |
| `/stopbuzzer` | — | Stop test buzzer manual |
| `/setalarmconfig` | `alarmTime` (HH:MM) | Set waktu alarm |
| `/uploadcities` | file `cities.json` | Upload daftar kota (max 1MB) |

### Contoh Response `/api/data`
```json
{
  "time": "14:35:22",
  "date": "19/12/2024",
  "day": "Wednesday",
  "timestamp": 1734614122,
  "prayerTimes": {
    "imsak": "04:07",
    "subuh": "04:17",
    "terbit": "05:32",
    "zuhur": "11:54",
    "ashar": "15:12",
    "maghrib": "17:58",
    "isya": "19:08"
  },
  "location": {
    "city": "jakarta",
    "cityId": "jakarta",
    "displayName": "Jakarta",
    "latitude": "-6.2088",
    "longitude": "106.8456"
  },
  "device": {
    "wifiConnected": true,
    "wifiState": "connected",
    "rssi": -52,
    "apIP": "192.168.100.1",
    "ntpSynced": true,
    "ntpServer": "pool.ntp.org",
    "freeHeap": 245632,
    "uptime": 3600
  }
}
```

Nilai `wifiState` yang mungkin:

| wifiState | Keterangan |
|-----------|-----------|
| `idle` | WiFi belum dikonfigurasi |
| `connecting` | Sedang menghubungkan ke router |
| `connected` | Terhubung ke router |
| `failed` | Gagal konek (sedang exponential backoff retry) |

### Contoh Response `/devicestatus`
```json
{
  "connected": true,
  "wifiState": "connected",
  "ssid": "NamaWiFi",
  "ip": "192.168.1.100",
  "rssi": -52,
  "ntpSynced": true,
  "ntpServer": "pool.ntp.org",
  "currentTime": "14:35:22",
  "currentDate": "19/12/2024",
  "uptime": 3600,
  "freeHeap": "245632"
}
```

### Contoh Response `/api/countdown`
```json
{
  "active": true,
  "remaining": 45,
  "message": "Memulai Ulang Perangkat",
  "reason": "device_restart"
}
```

Nilai `reason` yang mungkin:

| reason | Keterangan |
|--------|-----------|
| `device_restart` | Restart perangkat penuh |
| `factory_reset` | Reset ke pengaturan pabrik |
| `ap_restart` | Restart Access Point saja |

---

## 🔍 Troubleshooting

### Internet Putus (WiFi Masih Konek)

**Gejala:** LED hijau kedip lambat, WiFi masih terhubung tapi jadwal sholat tidak update dan NTP gagal sync.

**Penyebab:**
- Kabel LAN router putus
- ISP down
- Router restart
- DNS bermasalah

**Cek Status (Serial Monitor):**
```
[Internet] Koneksi internet terputus (WiFi masih konek)  → ❌ Internet mati
[Internet] Koneksi internet tersedia                     → ✅ Internet OK
```

**Solusi:**
1. Cek kabel LAN ke router/modem
2. Restart modem/router
3. Tunggu — sistem cek ulang otomatis setiap 30 detik
4. Saat internet kembali, LED otomatis berubah ke hijau solid

---

### WiFi Tidak Connect / WIFI_FAILED

**Penyebab:**
- SSID/Password salah (case-sensitive)
- Router hanya 5GHz (ESP32 hanya 2.4GHz)
- MAC filtering aktif
- Sinyal terlalu lemah
- Channel WiFi >11

**Solusi:**
1. Cek SSID dan password (huruf besar/kecil)
2. Pastikan router broadcast 2.4GHz
3. Tambahkan MAC ESP32 ke whitelist
4. Dekatkan ke router
5. Ganti channel router ke 1–11
6. Factory reset → konfigurasi ulang

**Indikator LED:**
```
Hijau kedip cepat  → Sedang connecting (maks 5x percobaan)
Hijau solid        → Konek + internet OK
Hijau kedip lambat → Konek tapi internet putus
Merah nyala        → Gagal konek (WIFI_FAILED) — retry otomatis
LED mati           → Tidak ada konfigurasi WiFi
```

**Mekanisme Retry Otomatis saat WIFI_FAILED:**
```
Attempt 1–5  → Connecting langsung (WIFI_CONNECTING)
Gagal semua  → WIFI_FAILED, LED merah nyala
Retry #1     → Tunggu 10 detik → coba lagi
Retry #2     → Tunggu 20 detik → coba lagi
Retry #3     → Tunggu 40 detik → coba lagi
Retry #4     → Tunggu 60 detik → coba lagi
Retry #5+    → Tunggu 120 detik → coba lagi (selamanya)
Berhasil     → WIFI_CONNECTED, LED hijau solid/kedip
```

**Serial Monitor:**
```
WiFi: Connected | RSSI: -45 dBm | IP: 192.168.1.100  → ✅ Baik
WiFi: Connected | RSSI: -75 dBm                      → ⚠️ Lemah
WiFi Disconnected | Reason Code: 15                  → ❌ Gagal
Max reconnect attempts tercapai → WIFI_FAILED         → 🔴 Backoff aktif
WIFI_FAILED: Retry #1 (backoff 10s)                  → 🔄 Mencoba lagi
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
   - Tab LOKASI → Edit Koordinat → Paste → Simpan

2. **Gunakan Tune/Offset:**
   - Tab LOKASI → Atur nilai tune per-waktu sholat
   - Contoh: Subuh selalu terlambat 3 menit → tuneSubuh = `-3`

3. **Ganti Metode:**
   - Tab LOKASI → Dropdown Metode
   - Coba: **Kemenag** (Metode 20) — paling akurat Indonesia
   - Atau: **Egyptian** (Metode 5) — alternatif bagus

4. **Cek Zona Waktu:**
   - Tab WAKTU → Pastikan sesuai lokasi (WIB/WITA/WIT)

---

### Notifikasi Sholat Tidak Jalan Setelah Reset

**Gejala:** Setelah factory reset atau boot pertama, sudah jam sholat tapi tidak ada notifikasi.

**Penyebab Normal:** Sistem sengaja tidak menjalankan notifikasi jika semua waktu sholat masih `00:00` — ini adalah fitur keamanan untuk mencegah notifikasi palsu sebelum data tersedia.

**Solusi:**
1. Sambungkan WiFi → pilih kota → Simpan
2. Tunggu data jadwal di-fetch dari API
3. Notifikasi aktif otomatis setelah data tersedia

---

### Jam Reset ke 01/01/2000

**Penyebab:**
- NTP belum sync (WiFi belum connect)
- RTC tidak terpasang
- Baterai RTC habis
- RTC hardware failure

**Solusi:**
1. Sambungkan WiFi — NTP sync otomatis dalam 10–30 detik
2. Pasang RTC DS3231 + baterai CR2032
3. Tab WAKTU → Tombol "Perbarui Waktu" (temporary fix)

**RTC Rusak:**
```
*** RTC HARDWARE FAILURE ***
DS3231 chip is defective!
>>> SOLUTION: BUY NEW DS3231 MODULE <<<
```
Ganti modul baru — tidak bisa diperbaiki.

---

### Waktu Sholat Tidak Update Tengah Malam

**Serial Monitor (00:00–00:05):**

✅ **Normal:**
```
MIDNIGHT DETECTED - STARTING SEQUENCE
Triggering NTP Sync...
NTP SYNC COMPLETED
Updating Prayer Times...
Prayer times updated successfully
```

❌ **Gagal:**
- WiFi tidak connect → Cek Tab BERANDA
- Kota belum dipilih → Tab LOKASI → Pilih kota → Simpan
- NTP timeout → Akan retry di 00:01, 00:02, dst

**Manual Update:** Tab LOKASI → Klik **Simpan**

---

### Alarm Tidak Berbunyi

**Pemeriksaan:**
1. Switch alarm sudah ON di Tab JADWAL
2. Waktu alarm sudah di-**Simpan** (bukan hanya diatur di picker)
3. Volume buzzer tidak 0%
4. Waktu sistem sudah benar

**Debug:**
```
✅ Normal:
Konfigurasi alarm dimuat: 06:00 | ON
ALARM AKTIF: 06:00

❌ Tidak berbunyi:
Konfigurasi alarm dimuat: 06:00 | OFF  → Switch belum ON
```

---

### Audio Adzan Tidak Play (DFPlayer)

**Serial Monitor saat boot:**
```
✅ Normal:
INITIALIZING DFPlayer Mini
DFPlayer initialized successfully!
Files on SD: 5

❌ Gagal:
DFPlayer connection FAILED!
Check wiring
```

**Checklist:**
1. Wiring silang: DFPlayer TX → GPIO32, DFPlayer RX → GPIO25
2. SD Card format FAT32, file di root: `0001.mp3` s/d `0005.mp3`
3. Speaker passive 3–5W (4–8Ω) — JANGAN speaker aktif
4. Cara play: Tunggu waktu sholat → label blink → tap label

**Timeout:** Touch adzan aktif 10 menit setelah waktu masuk.

---

### Prayer Task Crash

```
KRITIS: TUGAS SHALAT CRASH
Aksi: Memulai ulang tugas otomatis...
Tugas Shalat berhasil dimulai ulang
```

**Sistem auto-recovery** — task restart otomatis. Jika crash berulang, laporkan ke developer.

---

### Web Interface Lambat/Timeout

**Solusi:**
1. Dekatkan ke router (RSSI > -60 dBm)
2. Clear cache browser (Ctrl+Shift+Delete)
3. Batasi max 3 client bersamaan
4. Tab BERANDA → Mulai Ulang Perangkat

---

### Display Flicker/Tearing

**Solusi:**
1. Gunakan adaptor 5V 2A minimum
2. Kabel USB pendek (<1m, kualitas bagus)
3. Tambah kapasitor 100–470µF di VIN-GND

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
1. **Ganti SSID dan Password AP** segera setelah setup
2. **Gunakan WPA2/WPA3** di router
3. **Jangan ekspos ke internet** — hanya akses lokal
4. **Monitor akses fisik** — Serial port bisa untuk debug
5. **Backup konfigurasi** — Screenshot semua setting
6. **Update firmware** — Check repository berkala

---

## 🙏 Credits

**Developer:** GONIT - Global Network Identification Technology

**Libraries:**
- LVGL Team — https://lvgl.io/
- Espressif Systems — ESP32 Arduino Core
- Bodmer — TFT_eSPI
- Benoit Blanchon — ArduinoJson
- me-no-dev — ESPAsyncWebServer
- Adafruit — RTClib
- DFRobot — DFPlayerMini

**API:** Aladhan API — https://aladhan.com/

---

**© 2025 GONIT - Global Network Identification Technology**
