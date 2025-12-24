# 🕌 ESP32 Islamic Prayer Clock

> Jam digital otomatis untuk waktu sholat dengan web interface dan RTC backup

[![Version](https://img.shields.io/badge/version-2.1-blue)](https://github.com/gonit-dev/jws-indonesia) [![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)](https://lvgl.io/) [![Platform](https://img.shields.io/badge/platform-ESP32-red)](https://www.espressif.com/) [![License](https://img.shields.io/badge/license-MIT-yellow)](LICENSE)

---

## 📸 Preview

<div align="center">

| Home Screen | WiFi Settings | Time Sync |
|-------------|---------------|-----------|
| ![Home](https://github.com/user-attachments/assets/d0e64d2a-2a00-4a62-b92b-9aa36d95f4b8) | ![WiFi](https://github.com/user-attachments/assets/55d84d62-d986-460a-b2b2-3ac4d2b4aaf1) | ![Time](https://github.com/user-attachments/assets/b9f1db1c-83f2-4492-aef8-7b62997da9a9) |

| City Selection | Edit Coordinates | Prayer Times |
|----------------|------------------|--------------|
| ![City](https://github.com/user-attachments/assets/84408e8e-ef1b-4ee5-aa42-9c0f965bb031) | ![Coords](https://github.com/user-attachments/assets/6479dfd9-99b9-4034-a0d2-29453d6c46d9) | ![Prayer](https://github.com/user-attachments/assets/1f105d28-80a6-490c-a4a9-d7f8174a3c3e) |

</div>

---

## ✨ Fitur Utama

- 🕌 **Jadwal Sholat Otomatis** - Update otomatis tengah malam via Aladhan API
- ⏰ **NTP Time Sync** - Sinkronisasi jam otomatis setiap 1 jam
- 🌐 **Web Interface** - Konfigurasi lengkap via browser (responsive)
- 📍 **500+ Kota** - Database kota Indonesia dengan koordinat GPS
- 💾 **RTC DS3231** - Jam tetap akurat saat mati lampu (battery backup)
- 🖥️ **LVGL 9.2.0** - UI touchscreen smooth 20 FPS
- 🔄 **Event-Driven WiFi** - Auto-reconnect cepat tanpa polling
- 🕋 **8 Metode Kalkulasi** - Kemenag, MWL, Egyptian, ISNA, dll
- 🔊 **Buzzer Configurable** - Toggle & volume control per waktu sholat

---

## 🔧 Hardware Requirements

### Board: ESP32-2432S024
- MCU: ESP32 Dual-Core @ 240MHz
- Display: ILI9341 2.4" TFT (320x240)
- Touch: XPT2046 Resistive
- WiFi: 802.11 b/g/n (2.4GHz)
- Power: 5V USB (min 2A)

### RTC DS3231 (Opsional)
```
DS3231       ESP32
VCC     →    3.3V
GND     →    GND
SDA     →    GPIO 21
SCL     →    GPIO 22
```

---

## 📦 Instalasi

### 1. Requirements

| Komponen | Versi | Wajib |
|----------|-------|-------|
| ESP32 Board | v3.0.7 | ✅ |
| LVGL | 9.2.0 | ✅ |
| Arduino IDE | 2.x+ | - |

### 2. Install ESP32 Board

**Arduino IDE:**
```
File → Preferences → Additional Boards Manager URLs:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

Tools → Board → Boards Manager → Install: esp32 v3.0.7
```

### 3. Install Libraries

Via Library Manager (Sketch → Include Library → Manage Libraries):

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

### 4. Upload

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
Flash Size: 4MB (3MB APP / 1MB SPIFFS)
Partition Scheme: Default 4MB with spiffs
```

---

## 🚀 Quick Start

### Step 1: First Boot
```
📶 SSID: "JWS Indonesia"
🔐 Password: "12345678"
🌐 IP: http://192.168.4.1
```

### Step 2: Konfigurasi WiFi
1. Hubungkan ke WiFi **"JWS Indonesia"**
2. Buka browser → `http://192.168.4.1`
3. Tab **WIFI** → Masukkan SSID & Password WiFi rumah
4. Klik **Simpan** → Tunggu auto-connect (~15 detik)

### Step 3: Set Lokasi
1. Tab **LOKASI** → Pilih kota dari dropdown
2. *Opsional:* Edit koordinat GPS manual
3. Pilih metode kalkulasi (default: Egyptian)
4. Klik **Simpan** → Jadwal sholat auto-update

### Step 4: Timezone
```
Default: UTC+7 (WIB)
- WIB: +7  (Jawa, Sumatera)
- WITA: +8 (Kalimantan, Sulawesi)
- WIT: +9  (Papua, Maluku)
```

Tab **WAKTU** → Edit timezone → Klik **💾**

---

## 🌐 Web Interface

**Access:**
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ESP32>  (lihat serial monitor)
```

### Tab Overview

#### 🏠 BERANDA
- Status device (WiFi, IP, NTP, Uptime)
- Real-time clock display
- Manual restart button

#### 📡 WIFI
- **WiFi Router:** SSID & Password WiFi rumah
- **Access Point:** Custom SSID & Password AP
- **Network Config:** IP, Gateway, Subnet AP

#### ⏰ WAKTU
- **Manual Sync:** Sync waktu dari browser
- **Auto NTP:** Otomatis sync setiap 1 jam
- **Timezone:** Set UTC offset (-12 hingga +14)

#### 🕌 LOKASI
- **Pilih Lokasi:** 500+ kota Indonesia
- **Edit GPS:** Manual coordinates dengan reset default
- **Metode:** 8 pilihan (Kemenag, MWL, Egyptian, dll)
- **Upload JSON:** Upload cities.json baru (max 1MB)

#### 🙏 JADWAL
- Jadwal sholat lengkap (Imsak → Isya)
- Toggle buzzer per waktu sholat
- Volume control (0-100%)
- Auto-refresh tengah malam

#### ⚠️ RESET
- Factory reset (hapus semua data)
- Device auto-restart

---

## 🔍 Troubleshooting

### Compile Errors

**`ledcAttach() not declared`**
```
Solusi: Install ESP32 Board v3.0.7
```

**LVGL function errors**
```
Solusi: Install LVGL v9.2.0
```

### Upload Errors

**`Timed out waiting for packet header`**
```
Solusi:
1. Tekan & tahan tombol BOOT saat upload
2. Kurangi Upload Speed → 115200
3. Ganti kabel USB (data + power)
4. Install driver CH340/CP2102
```

### Runtime Issues

**WiFi tidak connect**
```
✅ Cek SSID/password (case-sensitive)
✅ Router harus 2.4GHz (bukan 5GHz)
✅ Restart device atau factory reset
```

**Jadwal sholat tidak akurat**
```
✅ Edit koordinat GPS manual (Google Maps)
✅ Ganti metode kalkulasi
✅ Koordinat akurat = waktu akurat
```

**Jam 01/01/2000 setelah mati lampu**
```
✅ Normal jika belum NTP sync
✅ Tunggu WiFi connect
✅ Install RTC DS3231 (battery backup)
```

---

## 🌐 API Endpoint

### GET `/api/data` - IoT Integration

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

## 📊 System Architecture

### FreeRTOS Tasks

| Task | Core | Priority | Stack | Function |
|------|------|----------|-------|----------|
| UI Task | 1 | High | 12KB | LVGL rendering @ 20 FPS |
| WiFi Task | 0 | High | 4KB | Event-driven connection |
| NTP Task | 0 | High | 6KB | Time sync fallback |
| Web Task | 0 | Low | 6KB | AsyncWebServer |
| Prayer Task | 0 | Low | 6KB | Midnight auto-update |
| Clock Task | 0 | High | 2KB | 1-second tick |
| RTC Sync | 0 | Low | 2KB | RTC ↔ System sync |

### Auto-Update System

**Midnight Prayer Update:**
```
00:00-00:05 → Trigger NTP Sync → Wait Complete → Update Prayer Times
```

**Hourly NTP Sync:**
```
Every 3600 seconds → Trigger NTP Sync → Update Display
```

---

## 📁 File Structure

```
jws-indonesia/
├── jws.ino                    # Main program
├── src/                       # EEZ Studio UI
│   ├── ui.h/cpp
│   ├── screens.h
│   ├── images.h
│   └── fonts.h
├── data/                      # LittleFS
│   ├── index.html             # Web interface
│   ├── assets/css/foundation.min.css
│   └── cities.json            # 500+ cities
└── README.md
```

**Runtime Files (Auto-created):**
```
/wifi_creds.txt       → WiFi credentials
/ap_creds.txt         → Access Point config
/prayer_times.txt     → Cached prayer times
/city_selection.txt   → City + GPS coordinates
/method_selection.txt → Calculation method
/timezone.txt         → UTC offset
/buzzer_config.txt    → Buzzer settings
```

---

## 🚀 Performance

### Optimizations

1. **WiFi Sleep DOUBLE-DISABLED**
   - Response: <10ms (vs 100-500ms)

2. **Event-Driven WiFi**
   - No polling → CPU idle

3. **Pre-allocated Buffers**
   - No malloc/free → super fast

4. **Browser Caching**
   - Load CSS once → instant reload

**Result:** Page load **200-500ms** (was 2-5 seconds!)

---

## 🔐 Security

**Default Credentials:**
```
AP SSID:     JWS Indonesia
AP Password: 12345678
```

**Recommendations:**
- ✅ Ganti AP password via web interface
- ✅ Gunakan strong WiFi password
- ✅ Akses hanya dari trusted network
- ❌ Jangan expose ke public internet

[Report Bug](https://github.com/gonit-dev/jws-indonesia/issues) · [Request Feature](https://github.com/gonit-dev/jws-indonesia/issues)

</div>
