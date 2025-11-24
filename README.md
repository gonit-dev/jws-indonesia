# ESP32 Islamic Prayer Clock

Jam Waktu Sholat Digital berbasis ESP32-2432S024 dengan antarmuka touchscreen, sinkronisasi NTP otomatis, dan pemilihan kota manual via web interface.

![Version](https://img.shields.io/badge/version-2.0-blue)
![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)
![Platform](https://img.shields.io/badge/platform-ESP32-red)
![License](https://img.shields.io/badge/license-MIT-yellow)

## 📸 Screenshot
<img width="1366" height="2001" alt="FireShot Capture 090 - Islamic Prayer Clock Settings - 192 168 4 1" src="https://github.com/user-attachments/assets/2c69772f-4b74-494f-a232-7f3435539377" />
<video src="https://streamable.com/e/mbb13"></video>

## ✨ Fitur

- 🕌 **Waktu Sholat Otomatis** - 5 waktu sholat dengan update harian
- ⏰ **Jam Digital** - Sinkronisasi NTP dengan 5 server fallback
- 🌐 **Web Interface** - Konfigurasi via browser
- 📍 **Pemilihan Kota** - Pilih dari 500+ kota di Indonesia
- 🔄 **Auto Update Midnight** - Update otomatis waktu sholat jam 00:00-00:05
- ⏱️ **Hourly NTP Sync** - Sinkronisasi jam otomatis setiap 1 jam
- 🖥️ **Touchscreen UI** - Antarmuka LVGL 9.2.0
- 📱 **Access Point** - Mode AP untuk setup awal
- 💾 **LittleFS** - Penyimpanan persisten

## 🔧 Hardware

### Komponen Utama
- **ESP32-2432S024** (ESP32 + 2.4" TFT Touchscreen)
- **Display**: ILI9341 (320x240 pixels)
- **Touch**: XPT2046 (Resistive)
- **WiFi**: 802.11 b/g/n
- **Power**: 5V USB

### Pin Configuration
```cpp
#define TFT_BL      27  // Backlight PWM
#define TOUCH_CS    33  // Touch Chip Select
#define TOUCH_IRQ   36  // Touch Interrupt
#define TOUCH_MOSI  13  // Touch Data Out
#define TOUCH_MISO  12  // Touch Data In
#define TOUCH_CLK   14  // Touch Clock
```

## 📦 Instalasi

### ⚠️ Prerequisites (PENTING!)

```bash
# Arduino IDE atau PlatformIO
- Arduino IDE 2.x atau PlatformIO
- ESP32 Board Package v3.0.7 (WAJIB - JANGAN v2.x!)
- LVGL 9.2.0 (WAJIB - JANGAN v8.x!)
```

> **🚨 CRITICAL**: Project ini HANYA kompatibel dengan:
> - **ESP32 Board Package 3.0.7** (Tidak support v2.x atau v3.1.x+)
> - **LVGL 9.2.0** (API v9 berbeda dengan v8.x)

### Install ESP32 Board Package 3.0.7

#### Via Arduino IDE
```
1. File > Preferences
2. Additional Boards Manager URLs:
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
3. Tools > Board > Boards Manager
4. Cari "esp32"
5. Install "esp32 by Espressif Systems" versi 3.0.7
   ⚠️ JANGAN install versi lain!
```

#### Via PlatformIO
```ini
[env:esp32dev]
platform = espressif32@6.9.0  ; Setara ESP32 Core 3.0.7
board = esp32dev
framework = arduino
```

### Verifikasi Versi
```cpp
// Cek di serial monitor saat boot:
// ESP-IDF version: v5.1.4 = ESP32 Core 3.0.7 ✅
```

### Library Dependencies

#### Core Libraries (Wajib)

| Library | Versi | Fungsi | Link |
|---------|-------|--------|------|
| **LVGL** | **9.2.0** ⚠️ | GUI framework untuk touchscreen | [GitHub](https://github.com/lvgl/lvgl) |
| **TFT_eSPI** | 2.5.0+ | Driver display ILI9341 | [GitHub](https://github.com/Bodmer/TFT_eSPI) |
| **XPT2046_Touchscreen** | 1.4+ | Driver touch resistive | [GitHub](https://github.com/PaulStoffregen/XPT2046_Touchscreen) |
| **ArduinoJson** | 6.21.0+ | Parsing JSON dari API | [GitHub](https://github.com/bblanchon/ArduinoJson) |
| **ESPAsyncWebServer** | 1.2.3+ | Web server asinkron | [GitHub](https://github.com/me-no-dev/ESPAsyncWebServer) |
| **AsyncTCP** | 1.1.1+ | Dependency ESPAsyncWebServer | [GitHub](https://github.com/me-no-dev/AsyncTCP) |
| **TimeLib** | 1.6.1+ | Manajemen waktu & date | [GitHub](https://github.com/PaulStoffregen/Time) |
| **NTPClient** | 3.2.1+ | Sinkronisasi waktu via NTP | [GitHub](https://github.com/arduino-libraries/NTPClient) |

> **🚨 PENTING**: 
> - **LVGL harus versi 9.2.0** (bukan 8.x atau 9.0.x)
> - LVGL v9 API sangat berbeda dengan v8
> - Jika install versi lain, code tidak akan compile!

#### Built-in ESP32 Libraries (Otomatis Terinstall)
- `WiFi.h` - WiFi connectivity
- `WiFiClientSecure.h` - HTTPS client
- `HTTPClient.h` - HTTP requests
- `LittleFS.h` - File system
- `SPI.h` - SPI communication
- `esp_task_wdt.h` - Watchdog timer
- `esp_wifi.h` - WiFi low-level control

#### PlatformIO Configuration
```ini
[env:esp32dev]
platform = espressif32@6.9.0       ; ESP32 Core 3.0.7 (WAJIB!)
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    lvgl/lvgl@9.2.0                ; LVGL 9.2.0 (WAJIB!)
    bodmer/TFT_eSPI@^2.5.0
    paulstoffregen/XPT2046_Touchscreen@^1.4
    bblanchon/ArduinoJson@^6.21.0
    me-no-dev/ESPAsyncWebServer@^1.2.3
    me-no-dev/AsyncTCP@^1.1.1
    paulstoffregen/Time@^1.6.1
    arduino-libraries/NTPClient@^3.2.1

build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE      ; LVGL config
```

> **💡 TIP**: Gunakan versi exact (`@9.2.0`) bukan caret (`^9.2.0`) untuk LVGL agar tidak auto-update ke v9.3+

#### Arduino IDE Installation
```
1. Install ESP32 Board Package 3.0.7 (lihat bagian Prerequisites)

2. Buka Library Manager (Ctrl+Shift+I)

3. Install library berikut dengan versi EXACT:
   
   ⚠️ CRITICAL:
   - LVGL by kisvegabor → PILIH VERSI 9.2.0 (bukan latest!)
   
   ✅ Bebas versi:
   - TFT_eSPI by Bodmer → latest OK
   - XPT2046_Touchscreen by Paul Stoffregen → latest OK
   - ArduinoJson by Benoit Blanchon → v6.21+ OK
   - ESPAsyncWebServer by Me-No-Dev → latest OK
   - AsyncTCP by Me-No-Dev → latest OK
   - Time by Paul Stoffregen → latest OK
   - NTPClient by Arduino → latest OK
```

> **🚨 TROUBLESHOOTING**:
> - Jika LVGL error compile → Uninstall dan install ulang versi 9.2.0
> - Jika ESP32 error → Pastikan Board Manager menunjukkan "3.0.7"
> - Jika ledcAttach() error → Board Package bukan 3.0.7

### Penjelasan Library

#### 1️⃣ LVGL (Light and Versatile Graphics Library)
**Versi**: 9.2.0 (WAJIB - Tidak kompatibel dengan v8.x!)
**Fungsi**: Framework GUI untuk membuat interface touchscreen yang smooth
- Rendering grafis hardware-accelerated
- Widget system (button, label, slider, dll)
- Touch input handling
- Memory management otomatis
- **Digunakan untuk**: Tampilan jam, waktu sholat, animasi

> **⚠️ PERHATIAN LVGL v9**:
> - API v9 sangat berbeda dengan v8
> - Breaking changes: `lv_obj_t*` → object handling baru
> - Display & input driver API berubah total
> - Code v8 tidak akan compile di v9 (dan sebaliknya)

#### 2️⃣ TFT_eSPI
**Fungsi**: Driver universal untuk display TFT berbagai jenis
- Support ILI9341, ST7789, ST7735, dll
- Hardware SPI acceleration
- DMA support untuk rendering cepat
- **Digunakan untuk**: Kontrol display LCD 2.4"

#### 3️⃣ XPT2046_Touchscreen
**Fungsi**: Driver untuk touch controller resistive XPT2046
- Pressure detection (deteksi tekanan sentuh)
- Coordinate mapping & calibration
- IRQ-based touch detection
- **Digunakan untuk**: Input touchscreen

#### 4️⃣ ArduinoJson
**Fungsi**: Library parsing & serializing JSON
- Parser JSON yang efisien
- Low memory footprint
- Support streaming
- **Digunakan untuk**: Parse response API Aladhan

#### 5️⃣ ESPAsyncWebServer
**Fungsi**: Web server asinkron untuk ESP32
- Non-blocking operations
- Multiple concurrent connections
- WebSocket support
- Low memory usage
- **Digunakan untuk**: Web interface konfigurasi

#### 6️⃣ AsyncTCP
**Fungsi**: Library TCP asinkron (dependency ESPAsyncWebServer)
- Non-blocking TCP socket
- Event-driven architecture
- **Digunakan untuk**: Backend ESPAsyncWebServer

#### 7️⃣ TimeLib
**Fungsi**: Library manajemen waktu lengkap
- Unix timestamp conversion
- Date/time calculation
- Timezone support
- **Digunakan untuk**: Perhitungan waktu sistem

#### 8️⃣ NTPClient
**Fungsi**: Client untuk sinkronisasi waktu via NTP
- Multiple NTP server support
- Timezone offset
- Auto-retry mechanism
- **Digunakan untuk**: Sinkronisasi jam dari internet

### Langkah Instalasi

1. **Clone Repository**
```bash
git clone https://github.com/username/esp32-prayer-clock.git
cd esp32-prayer-clock
```

2. **Upload File System (LittleFS)**
```bash
# Upload file HTML, CSS, dan cities.json
# Arduino IDE: Tools > ESP32 Sketch Data Upload
# PlatformIO: pio run --target uploadfs
```

3. **Upload Code**
```bash
# Arduino IDE: Sketch > Upload
# PlatformIO: pio run --target upload
```

4. **Monitor Serial**
```bash
# Baud rate: 115200
pio device monitor -b 115200
```

## 🚀 Penggunaan

### Setup Awal

1. **Nyalakan Device**
   - Device akan membuat Access Point
   - SSID: `JWS ESP32`
   - Password: `12345678`

2. **Hubungkan ke AP**
   - Hubungkan HP/Laptop ke WiFi `JWS ESP32`
   - Buka browser: `http://192.168.4.1`

3. **Konfigurasi WiFi**
   - Masukkan SSID & Password WiFi rumah
   - Klik **Simpan & Restart**

4. **Pilih Kota**
   - Setelah terhubung WiFi
   - Buka web interface lagi
   - Pilih kota dari dropdown
   - Waktu sholat akan update otomatis

### Web Interface

#### URL Access
```
AP Mode:  http://192.168.4.1
STA Mode: http://<IP-ADDRESS>
```

#### Fitur Web Interface
- ✅ Device Status (WiFi, IP, NTP)
- ✅ WiFi Configuration
- ✅ City Selection (500+ kota)
- ✅ Prayer Times Display
- ✅ Manual Time Sync
- ✅ AP Credentials Change
- ✅ Factory Reset

## 📁 Struktur File

```
esp32-prayer-clock/
├── jws2.ino                    # Main program
├── src/
│   ├── ui.h                    # EEZ Studio UI header
│   ├── ui.cpp                  # EEZ Studio UI implementation
│   └── screens.h               # Screen definitions
├── data/                       # LittleFS files
│   ├── index.html              # Web interface
│   ├── assets/
│   │   └── css/
│   │       └── foundation.css  # CSS framework
│   └── cities.json             # 500+ kota Indonesia
├── README.md
├── LICENSE
└── platformio.ini              # PlatformIO config
```

## 🌐 API Endpoints

### Device Status
```http
GET /devicestatus
```
Response:
```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip": "192.168.1.100",
  "ntpSynced": true,
  "ntpServer": "pool.ntp.org",
  "currentTime": "14:30:25",
  "freeHeap": "180000"
}
```

### Get Prayer Times
```http
GET /getprayertimes
```
Response:
```json
{
  "subuh": "04:30",
  "dzuhur": "12:05",
  "ashar": "15:20",
  "maghrib": "18:10",
  "isya": "19:25"
}
```

### Get Cities List
```http
GET /getcities
```
Response:
```json
[
  {"api": "Jakarta", "display": "Jakarta"},
  {"api": "Bandung", "display": "Bandung"},
  {"api": "Surabaya", "display": "Surabaya"}
]
```

### Set City
```http
POST /setcity
Content-Type: application/x-www-form-urlencoded

city=Jakarta
```

### Set WiFi
```http
POST /setwifi
Content-Type: application/x-www-form-urlencoded

ssid=MyWiFi&password=MyPassword123
```

### Manual Time Sync
```http
POST /synctime
Content-Type: application/x-www-form-urlencoded

y=2025&m=11&d=23&h=14&i=30&s=0
```

### Factory Reset
```http
POST /reset
```

## 🏗️ Arsitektur

### FreeRTOS Tasks

| Task | Priority | Core | Stack | Fungsi |
|------|----------|------|-------|--------|
| UI Task | 3 | 1 | 16KB | LVGL rendering |
| WiFi Task | 2 | 0 | 8KB | WiFi management |
| NTP Task | 2 | 0 | 8KB | Time sync |
| Web Task | 1 | 0 | 16KB | Web server |
| Prayer Task | 1 | 0 | 8KB | Midnight auto-update (00:00) |
| Clock Task | 2 | 0 | 4KB | Clock tick |

### Semaphores & Mutexes
```cpp
displayMutex    // Proteksi operasi display
timeMutex       // Proteksi data waktu
wifiMutex       // Proteksi status WiFi
settingsMutex   // Proteksi konfigurasi
spiMutex        // Proteksi SPI bus
```

### Message Queue
```cpp
displayQueue    // Update display (10 items)
```

## ⏰ Auto-Update System

### 1️⃣ Midnight Prayer Times Update
```cpp
// prayerTask - Berjalan setiap 10 detik
// Cek: Jam 00:00 - 00:05 + WiFi connected + City selected
if (currentHour == 0 && currentMinute < 5 && !hasUpdatedToday) {
    getPrayerTimesByCity(selectedCity);
    hasUpdatedToday = true;
}
```
**Cara Kerja**:
- ✅ Task cek waktu setiap 10 detik
- ✅ Jika jam 00:00-00:05 dan belum update hari ini
- ✅ Auto-fetch prayer times dari API Aladhan
- ✅ Flag `hasUpdatedToday` reset saat ganti hari
- ✅ Hanya update jika WiFi connected dan city sudah dipilih

### 2️⃣ Hourly NTP Sync
```cpp
// clockTickTask - Counter setiap 1 detik
// Auto-sync setiap 3600 detik (1 jam)
if (autoSyncCounter >= 3600) {
    xTaskNotifyGive(ntpTaskHandle);  // Trigger NTP sync
    autoSyncCounter = 0;
}
```
**Cara Kerja**:
- ✅ Counter increment setiap detik
- ✅ Setiap 1 jam (3600 detik) trigger NTP sync
- ✅ Cegah clock drift
- ✅ Hanya jika WiFi connected

### 3️⃣ WiFi Auto-Update saat Connect
```cpp
// wifiTask - Saat WiFi baru connected
if (wifiState == WIFI_CONNECTED && !autoUpdateDone) {
    vTaskDelay(3000);  // Tunggu 3 detik
    getPrayerTimesByCity(selectedCity);
    autoUpdateDone = true;
}
```
**Cara Kerja**:
- ✅ Setelah WiFi connect, tunggu 3 detik
- ✅ Auto-update prayer times
- ✅ Flag reset jika WiFi disconnect
- ✅ Hanya jika city sudah dipilih

## ⚙️ Konfigurasi

### WiFi Power Management
```cpp
// Balanced mode - mencegah overheat
WiFi.setSleep(true);
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
```

### Backlight PWM
```cpp
#define TFT_BL_BRIGHTNESS 180  // 0-255 (70%)
```

### NTP Servers (Fallback)
```cpp
const char* ntpServers[] = {
    "pool.ntp.org",
    "id.pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com",
    "time.windows.com"
};
```

### Touch Calibration
```cpp
#define TS_MIN_X  370
#define TS_MAX_X  3700
#define TS_MIN_Y  470
#define TS_MAX_Y  3600
```

## 🔍 Troubleshooting

### Error Compile / Build Failed

#### `ledcAttach()` atau `ledcWrite()` not declared
```
❌ Penyebab: ESP32 Board Package bukan 3.0.7
✅ Solusi: 
   - Uninstall ESP32 board package
   - Install ulang versi 3.0.7 EXACT
   - Restart Arduino IDE / PlatformIO
```

#### LVGL function not found / undefined reference
```
❌ Penyebab: LVGL bukan versi 9.2.0
✅ Solusi:
   - Library Manager → Uninstall LVGL
   - Install LVGL versi 9.2.0 (pilih manual)
   - Clean build: Delete .pio folder (PlatformIO)
   - Rebuild project
```

#### `lv_obj_t` errors / deprecated warnings
```
❌ Penyebab: Menggunakan LVGL v8.x
✅ Solusi: Install LVGL 9.2.0 (API v9 berbeda total)
```

### Device Tidak Muncul AP
```
✅ Cek serial monitor (115200 baud)
✅ Pastikan power supply 5V cukup
✅ Coba factory reset via serial
```

### WiFi Tidak Konek
```
✅ Cek SSID & password benar
✅ Cek jarak ke router
✅ Restart device
✅ Cek router support 2.4GHz
```

### Waktu Sholat Tidak Update

**Cek Kondisi Auto-Update:**
```
✅ WiFi harus terhubung
✅ City harus sudah dipilih via web interface
✅ Tunggu hingga jam 00:00-00:05 (midnight update)
✅ Atau restart device (auto-update saat WiFi connect)
```

**Manual Update:**
```
✅ Buka web interface
✅ Pilih ulang city dari dropdown
✅ Prayer times akan update langsung
```

**Debug via Serial Monitor:**
```cpp
// Cek pesan berikut:
"🕌 Midnight prayer times update for: [CityName]"     // ✅ Update berjalan
"⚠️ No city selected"                                  // ❌ Pilih city dulu
"❌ WiFi not connected"                                // ❌ Cek WiFi
"🕌 Auto-updating prayer times for: [CityName]"       // ✅ Update saat connect
```

### Touchscreen Tidak Responsif
```
✅ Kalibrasi ulang (edit TS_MIN/MAX)
✅ Cek koneksi kabel touch
✅ Test dengan contoh XPT2046
```

### Device Overheat
```
✅ Mode sleep sudah aktif otomatis
✅ Kurangi brightness: TFT_BL_BRIGHTNESS
✅ Pastikan ventilasi baik
```

## 🛠️ Development

### Build dari Source
```bash
# PlatformIO
pio run

# Arduino IDE
# File > Open > jws2.ino > Upload
```

### Serial Debug
```cpp
// Enable debug di serial monitor
Serial.setDebugOutput(true);
```

### Custom Cities
Edit `data/cities.json`:
```json
[
  {
    "api": "Jakarta",
    "display": "Jakarta"
  },
  {
    "api": "Bandung",
    "display": "Bandung"
  }
]
```

### Cek Versi ESP32 & LVGL
```cpp
// Tambahkan di setup() untuk debug:
Serial.println(ESP.getSdkVersion());  // Harus: v5.1.4
Serial.println(LVGL_VERSION_INFO);     // Harus: 9.2.0

// Output yang benar:
// ESP-IDF: v5.1.4 (ESP32 Core 3.0.7) ✅
// LVGL: 9.2.0 ✅
```

## 📊 Memory Usage

```
Flash: ~50% (2MB used)
SRAM:  ~40% (200KB used)
PSRAM: Not used
```

## 🔐 Security

- ⚠️ Default AP password: `12345678`
- ⚠️ Ganti password AP via web interface
- ⚠️ WiFi credentials tersimpan plain text
- ⚠️ Web interface tidak ter-enkripsi (HTTP)

## 🤝 Kontribusi

Kontribusi sangat diterima! Silakan:

1. Fork repository
2. Buat branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add some AmazingFeature'`)
4. Push ke branch (`git push origin feature/AmazingFeature`)
5. Buat Pull Request

## 📝 TODO

- [ ] HTTPS support
- [ ] OTA (Over-The-Air) update
- [ ] Adhan sound/notification
- [ ] Multiple timezones
- [ ] Calendar (Hijriyah)
- [ ] Qibla direction
- [ ] Dark/Light theme
- [ ] Manual refresh button untuk prayer times
- [ ] Configurable auto-update time (selain midnight)

## 📄 Lisensi

MIT License - lihat file [LICENSE](LICENSE)

## 👨‍💻 Author
Bimo

## 🙏 Credits

- [LVGL](https://lvgl.io/) - GUI library
- [Aladhan API](https://aladhan.com/prayer-times-api) - Prayer times data
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Display driver
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Async web server

## ⭐ Support

Jika project ini bermanfaat, berikan ⭐ di GitHub!

---

Made with ❤️ for the Muslim Community
