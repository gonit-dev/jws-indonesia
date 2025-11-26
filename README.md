# 🕌 ESP32 Islamic Prayer Clock

> Jam Waktu Sholat Digital dengan Touchscreen, Auto-Update, dan Web Interface

![Version](https://img.shields.io/badge/version-2.0-blue)
![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)
![Platform](https://img.shields.io/badge/platform-ESP32-red)
![License](https://img.shields.io/badge/license-MIT-yellow)

## 📸 Preview

<img width="683" height="1000" alt="Web Interface" src="https://github.com/user-attachments/assets/2c69772f-4b74-494f-a232-7f3435539377" />

**Video Demo**: [Watch on Streamable](https://streamable.com/mbb13l)

## ✨ Fitur Utama

| Fitur | Deskripsi |
|-------|-----------|
| 🕌 **Auto Prayer Times** | Update otomatis setiap tengah malam (00:00-00:05) |
| ⏰ **NTP Sync** | Sinkronisasi jam otomatis setiap 1 jam dengan 5 server fallback |
| 🌐 **Web Interface** | Konfigurasi via browser - WiFi, City Selection, Manual Sync |
| 📍 **500+ Cities** | Pilih kota dari seluruh Indonesia |
| 💾 **RTC DS3231** | Jam tetap akurat meski mati lampu |
| 🖥️ **LVGL 9.2.0** | UI touchscreen yang smooth & responsive |
| 📱 **AP Mode** | Setup awal tanpa perlu WiFi eksternal |

## 🔧 Hardware

**Board**: ESP32-2432S024 (ESP32 + 2.4" TFT Touchscreen)
- **Display**: ILI9341 (320x240 pixels)
- **Touch**: XPT2046 (Resistive)
- **RTC**: DS3231 (optional - tetapi sangat direkomendasikan!)
- **WiFi**: 802.11 b/g/n
- **Power**: 5V USB

### Pin RTC DS3231 (Optional)
```
DS3231 ─────── ESP32-2432S024
VCC    ─────── 3.3V
GND    ─────── GND
SDA    ─────── GPIO 21
SCL    ─────── GPIO 22
```

> **💡 Mengapa perlu RTC?**
> - Tanpa RTC: Jam reset ke 00:00:00 setiap mati lampu
> - Dengan RTC: Jam tetap akurat meski mati lampu, NTP sync hanya untuk koreksi drift

## 📦 Instalasi

### ⚠️ Requirements (PENTING!)

```
✅ ESP32 Board Package: v3.0.7 (WAJIB - bukan v2.x!)
✅ LVGL: 9.2.0 (WAJIB - API v9 berbeda dengan v8.x)
✅ Arduino IDE 2.x atau PlatformIO
```

### Install ESP32 Board v3.0.7

**Arduino IDE**:
```
1. File > Preferences
2. Boards Manager URLs: 
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Tools > Board > Boards Manager
4. Cari "esp32" → Install versi 3.0.7
```

**PlatformIO**:
```ini
platform = espressif32@6.9.0  ; ESP32 Core 3.0.7
```

### Library Dependencies

| Library | Versi | Install via |
|---------|-------|-------------|
| **LVGL** | **9.2.0** ⚠️ | Library Manager (pilih manual!) |
| TFT_eSPI | 2.5.0+ | Library Manager |
| XPT2046_Touchscreen | 1.4+ | Library Manager |
| ArduinoJson | 6.21.0+ | Library Manager |
| ESPAsyncWebServer | 1.2.3+ | Library Manager |
| AsyncTCP | 1.1.1+ | Library Manager |
| TimeLib | 1.6.1+ | Library Manager |
| NTPClient | 3.2.1+ | Library Manager |
| RTClib | 2.1.1+ | Library Manager |

> **🚨 CRITICAL**: LVGL harus versi **9.2.0** (bukan latest/8.x/9.0.x!)

### Quick Start

```bash
# 1. Clone repository
git clone https://github.com/yourusername/esp32-prayer-clock.git
cd esp32-prayer-clock

# 2. Upload File System (LittleFS)
# Arduino IDE: Tools > ESP32 Sketch Data Upload
# PlatformIO: pio run --target uploadfs

# 3. Upload Code
# Arduino IDE: Sketch > Upload
# PlatformIO: pio run --target upload

# 4. Monitor Serial (115200 baud)
pio device monitor -b 115200
```

## 🚀 Setup Awal

### 1️⃣ First Boot
```
1. Device membuat Access Point
   SSID: "JWS ESP32"
   Password: "12345678"

2. Connect HP/Laptop ke AP tersebut
3. Buka browser: http://192.168.4.1
```

### 2️⃣ Konfigurasi WiFi
```
1. Masukkan SSID & Password WiFi rumah
2. Klik "Simpan & Restart"
3. Device akan restart dan connect ke WiFi
```

### 3️⃣ Pilih Kota
```
1. Buka web interface lagi (IP baru)
2. Pilih kota dari dropdown (500+ kota)
3. Prayer times auto-update!
```

## 🌐 Web Interface

### URL Access
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ESP32>  (cek serial monitor)
```

### Fitur Available
- ✅ Device Status (WiFi, IP, NTP, RTC)
- ✅ WiFi Configuration
- ✅ City Selection (dropdown)
- ✅ Prayer Times Display
- ✅ Manual Time Sync (browser time → ESP32)
- ✅ AP Credentials Change
- ✅ Factory Reset

## ⏰ Auto-Update System

### 1️⃣ Midnight Prayer Update
```cpp
// Setiap hari jam 00:00-00:05
// Auto-fetch prayer times dari Aladhan API
if (currentHour == 0 && currentMinute < 5 && !hasUpdatedToday) {
    getPrayerTimesByCity(selectedCity);
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

### 3️⃣ RTC Sync (New!)
```cpp
// Setiap 1 menit
// Sync system time ← RTC time (jika selisih > 2 detik)
// NTP sync → RTC (setiap NTP berhasil)
```

## 📊 System Architecture

### FreeRTOS Tasks (Multi-Core)

| Task | Core | Priority | Function |
|------|------|----------|----------|
| UI Task | 1 | 3 | LVGL rendering @ 20 FPS |
| WiFi Task | 0 | 2 | WiFi connection management |
| NTP Task | 0 | 2 | Time synchronization |
| Web Task | 0 | 1 | Web server (80 concurrent) |
| Prayer Task | 0 | 1 | Midnight auto-update |
| Clock Task | 0 | 2 | Clock tick + hourly NTP |
| RTC Sync | 0 | 1 | RTC ↔ System time sync |

### Memory Protection
```cpp
displayMutex    // Display operations
timeMutex       // Time data
wifiMutex       // WiFi status
settingsMutex   // Configuration
spiMutex        // SPI bus (display + touch)
```

## 🔍 Troubleshooting

### ❌ Error Compile

**`ledcAttach()` not declared**
```
Penyebab: ESP32 Board bukan v3.0.7
Solusi: Uninstall board → Install v3.0.7
```

**LVGL function errors**
```
Penyebab: LVGL bukan v9.2.0
Solusi: Library Manager → Uninstall → Install 9.2.0
```

### ❌ Runtime Issues

**AP tidak muncul**
- Cek serial monitor (115200 baud)
- Pastikan power supply 5V cukup (min 2A)

**WiFi tidak connect**
- Cek SSID/password benar
- Router harus support 2.4GHz (bukan 5GHz only)
- Restart device

**Prayer times tidak update**
- Cek WiFi connected (serial: "✅ WiFi Connected")
- Cek city sudah dipilih (web interface)
- Tunggu midnight (00:00) atau restart device

**Jam tidak akurat setelah mati lampu**
- Install RTC DS3231 module (lihat pinout di atas)
- RTC akan auto-detected saat boot

## 🔐 Security Notes

⚠️ **Default Credentials**
```
AP SSID: JWS ESP32
AP Password: 12345678
```

⚠️ **Recommendations**
- Ganti AP password via web interface
- WiFi credentials stored plain text (LittleFS)
- Web interface HTTP (not HTTPS)
- For home use only

## 📁 File Structure

```
esp32-prayer-clock/
├── jws.ino                 # Main program
├── src/
│   ├── ui.h                # EEZ Studio UI
│   ├── ui.cpp
│   └── screens.h
├── data/                   # LittleFS (upload ke ESP32)
│   ├── index.html          # Web interface
│   ├── assets/css/
│   │   └── foundation.css
│   └── cities.json         # 500+ cities
└── README.md
```

## 📝 API Reference

### GET /devicestatus
```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip": "192.168.1.100",
  "ntpSynced": true,
  "ntpServer": "pool.ntp.org",
  "currentTime": "14:30:25",
  "freeHeap": "180000",
  "rtcAvailable": true
}
```

### GET /getprayertimes
```json
{
  "subuh": "04:30",
  "dzuhur": "12:05",
  "ashar": "15:20",
  "maghrib": "18:10",
  "isya": "19:25"
}
```

### POST /setcity
```
city=Jakarta
```

### POST /setwifi
```
ssid=MyWiFi&password=MyPassword123
```

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open Pull Request

## 📋 TODO

- [ ] HTTPS support
- [ ] OTA updates
- [ ] Adhan sound/notification
- [ ] Hijri calendar
- [ ] Qibla direction
- [ ] Dark/Light theme
- [ ] Manual prayer times refresh button

## 📄 License

MIT License - see [LICENSE](LICENSE) file

## 🙏 Credits

- [LVGL](https://lvgl.io/) - GUI library
- [Aladhan API](https://aladhan.com/prayer-times-api) - Prayer times
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Display driver
- [RTClib](https://github.com/adafruit/RTClib) - RTC driver

## 💝 Support

If this project helps you, please give it a ⭐ on GitHub!

---

**Made with ❤️ for the Muslim Community**
