# 🕌 ESP32 Islamic Prayer Clock

> Jam Waktu Sholat Digital dengan Touchscreen, Auto-Update, dan Web Interface

![Version](https://img.shields.io/badge/version-2.0-blue)
![LVGL](https://img.shields.io/badge/LVGL-9.2.0-green)
![Platform](https://img.shields.io/badge/platform-ESP32-red)
![License](https://img.shields.io/badge/license-MIT-yellow)

## 📸 Preview

<img width="683" height="1000" alt="Web Interface" src="https://github.com/user-attachments/assets/2c69772f-4b74-494f-a232-7f3435539377" />

**Video Demo**: [Watch on Tutorial](https://streamable.com/mbb13l)

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
git clone https://github.com/gonit-dev/jws-indonesia.git
cd jws-indonesia

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

# 📡 REST API - IoT Integration

## 🎯 Endpoint: `/api/data`

Endpoint ini memberikan **semua data lengkap** dalam satu request untuk integrasi dengan perangkat lain.

**URL Access:**
```
http://192.168.4.1/api/data        # Via Hotspot AP
http://192.168.1.100/api/data      # Via WiFi Router (ganti IP sesuai device)
```

### 📥 Response Format

```json
{
  "time": "14:30:45",
  "date": "26/11/2025",
  "day": "Wednesday",
  "timestamp": 1732612245,
  "prayerTimes": {
    "subuh": "04:30",
    "dzuhur": "12:05",
    "ashar": "15:20",
    "maghrib": "18:10",
    "isya": "19:25"
  },
  "location": {
    "city": "Jakarta",
    "cityId": "Jakarta"
  },
  "device": {
    "wifiConnected": true,
    "wifiSSID": "MyWiFi",
    "ip": "192.168.1.100",
    "apIP": "192.168.4.1",
    "ntpSynced": true,
    "freeHeap": 180000
  }
}
```

---

## 💻 Cara Mengakses API dari Berbagai Bahasa

### 🐍 Python

```python
import requests
import json

# Fetch data from ESP32
url = "http://192.168.1.100/api/data"
response = requests.get(url)

if response.status_code == 200:
    data = response.json()
    
    print(f"⏰ Current Time: {data['time']}")
    print(f"📅 Date: {data['date']} ({data['day']})")
    print(f"🕌 Prayer Times:")
    print(f"   Subuh: {data['prayerTimes']['subuh']}")
    print(f"   Dzuhur: {data['prayerTimes']['dzuhur']}")
    print(f"   Ashar: {data['prayerTimes']['ashar']}")
    print(f"   Maghrib: {data['prayerTimes']['maghrib']}")
    print(f"   Isya: {data['prayerTimes']['isya']}")
    print(f"📍 Location: {data['location']['city']}")
else:
    print(f"❌ Error: {response.status_code}")
```

**Advanced: Auto-refresh setiap 1 detik**
```python
import requests
import time
import os

def display_prayer_clock():
    url = "http://192.168.1.100/api/data"
    
    while True:
        try:
            response = requests.get(url, timeout=5)
            data = response.json()
            
            os.system('clear' if os.name == 'posix' else 'cls')
            print("=" * 50)
            print(f"🕌 PRAYER CLOCK - {data['location']['city']}")
            print("=" * 50)
            print(f"⏰ {data['time']}  📅 {data['date']}")
            print(f"📡 WiFi: {data['device']['wifiSSID']}")
            print("-" * 50)
            print(f"Subuh   : {data['prayerTimes']['subuh']}")
            print(f"Dzuhur  : {data['prayerTimes']['dzuhur']}")
            print(f"Ashar   : {data['prayerTimes']['ashar']}")
            print(f"Maghrib : {data['prayerTimes']['maghrib']}")
            print(f"Isya    : {data['prayerTimes']['isya']}")
            print("=" * 50)
            
            time.sleep(1)
        except Exception as e:
            print(f"❌ Connection error: {e}")
            time.sleep(5)

display_prayer_clock()
```

---

### 🟦 JavaScript (Node.js)

```javascript
const axios = require('axios');

async function getPrayerData() {
    try {
        const response = await axios.get('http://192.168.1.100/api/data');
        const data = response.data;
        
        console.log(`⏰ Current Time: ${data.time}`);
        console.log(`📅 Date: ${data.date} (${data.day})`);
        console.log(`🕌 Prayer Times:`);
        console.log(`   Subuh: ${data.prayerTimes.subuh}`);
        console.log(`   Dzuhur: ${data.prayerTimes.dzuhur}`);
        console.log(`   Ashar: ${data.prayerTimes.ashar}`);
        console.log(`   Maghrib: ${data.prayerTimes.maghrib}`);
        console.log(`   Isya: ${data.prayerTimes.isya}`);
        console.log(`📍 Location: ${data.location.city}`);
    } catch (error) {
        console.error('❌ Error:', error.message);
    }
}

getPrayerData();
```

**Auto-refresh:**
```javascript
setInterval(getPrayerData, 1000); // Refresh setiap 1 detik
```

---

### 🌐 JavaScript (Browser / HTML)

```html
<!DOCTYPE html>
<html>
<head>
    <title>Prayer Clock Dashboard</title>
    <style>
        body { font-family: Arial; padding: 20px; background: #1a1a1a; color: white; }
        .container { max-width: 600px; margin: 0 auto; }
        .time { font-size: 48px; font-weight: bold; text-align: center; }
        .prayer { padding: 10px; margin: 5px; background: #2a2a2a; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🕌 Prayer Clock</h1>
        <div class="time" id="time">--:--:--</div>
        <div id="date">--/--/----</div>
        <h3>Prayer Times - <span id="city">--</span></h3>
        <div id="prayers"></div>
    </div>

    <script>
        async function fetchData() {
            try {
                const response = await fetch('http://192.168.1.100/api/data');
                const data = await response.json();
                
                document.getElementById('time').textContent = data.time;
                document.getElementById('date').textContent = `${data.date} (${data.day})`;
                document.getElementById('city').textContent = data.location.city;
                
                const prayersHtml = `
                    <div class="prayer">Subuh: ${data.prayerTimes.subuh}</div>
                    <div class="prayer">Dzuhur: ${data.prayerTimes.dzuhur}</div>
                    <div class="prayer">Ashar: ${data.prayerTimes.ashar}</div>
                    <div class="prayer">Maghrib: ${data.prayerTimes.maghrib}</div>
                    <div class="prayer">Isya: ${data.prayerTimes.isya}</div>
                `;
                document.getElementById('prayers').innerHTML = prayersHtml;
            } catch (error) {
                console.error('Error:', error);
            }
        }
        
        // Refresh setiap 1 detik
        fetchData();
        setInterval(fetchData, 1000);
    </script>
</body>
</html>
```

---

### 🖥️ PHP

```php
<?php
$url = "http://192.168.1.100/api/data";
$response = file_get_contents($url);
$data = json_decode($response, true);

echo "⏰ Current Time: " . $data['time'] . "\n";
echo "📅 Date: " . $data['date'] . " (" . $data['day'] . ")\n";
echo "🕌 Prayer Times:\n";
echo "   Subuh: " . $data['prayerTimes']['subuh'] . "\n";
echo "   Dzuhur: " . $data['prayerTimes']['dzuhur'] . "\n";
echo "   Ashar: " . $data['prayerTimes']['ashar'] . "\n";
echo "   Maghrib: " . $data['prayerTimes']['maghrib'] . "\n";
echo "   Isya: " . $data['prayerTimes']['isya'] . "\n";
echo "📍 Location: " . $data['location']['city'] . "\n";
?>
```

**Web Dashboard (PHP + HTML):**
```php
<!DOCTYPE html>
<html>
<head>
    <title>Prayer Clock Dashboard</title>
    <meta http-equiv="refresh" content="5">
    <style>
        body { font-family: Arial; padding: 20px; background: #1a1a1a; color: white; }
        .time { font-size: 48px; text-align: center; }
        .prayer { padding: 15px; margin: 10px 0; background: #2a2a2a; border-radius: 8px; }
    </style>
</head>
<body>
    <?php
    $url = "http://192.168.1.100/api/data";
    $response = @file_get_contents($url);
    
    if ($response) {
        $data = json_decode($response, true);
        ?>
        <h1>🕌 Prayer Clock - <?php echo $data['location']['city']; ?></h1>
        <div class="time"><?php echo $data['time']; ?></div>
        <div style="text-align: center;"><?php echo $data['date'] . " (" . $data['day'] . ")"; ?></div>
        
        <h3>Prayer Times</h3>
        <div class="prayer">Subuh: <?php echo $data['prayerTimes']['subuh']; ?></div>
        <div class="prayer">Dzuhur: <?php echo $data['prayerTimes']['dzuhur']; ?></div>
        <div class="prayer">Ashar: <?php echo $data['prayerTimes']['ashar']; ?></div>
        <div class="prayer">Maghrib: <?php echo $data['prayerTimes']['maghrib']; ?></div>
        <div class="prayer">Isya: <?php echo $data['prayerTimes']['isya']; ?></div>
        <?php
    } else {
        echo "<p style='color: red;'>❌ Cannot connect to device</p>";
    }
    ?>
</body>
</html>
```

---

### 🔵 Go (Golang)

```go
package main

import (
    "encoding/json"
    "fmt"
    "io"
    "net/http"
)

type PrayerData struct {
    Time   string `json:"time"`
    Date   string `json:"date"`
    Day    string `json:"day"`
    PrayerTimes struct {
        Subuh   string `json:"subuh"`
        Dzuhur  string `json:"dzuhur"`
        Ashar   string `json:"ashar"`
        Maghrib string `json:"maghrib"`
        Isya    string `json:"isya"`
    } `json:"prayerTimes"`
    Location struct {
        City string `json:"city"`
    } `json:"location"`
}

func main() {
    url := "http://192.168.1.100/api/data"
    
    resp, err := http.Get(url)
    if err != nil {
        fmt.Println("❌ Error:", err)
        return
    }
    defer resp.Body.Close()
    
    body, _ := io.ReadAll(resp.Body)
    
    var data PrayerData
    json.Unmarshal(body, &data)
    
    fmt.Printf("⏰ Current Time: %s\n", data.Time)
    fmt.Printf("📅 Date: %s (%s)\n", data.Date, data.Day)
    fmt.Println("🕌 Prayer Times:")
    fmt.Printf("   Subuh: %s\n", data.PrayerTimes.Subuh)
    fmt.Printf("   Dzuhur: %s\n", data.PrayerTimes.Dzuhur)
    fmt.Printf("   Ashar: %s\n", data.PrayerTimes.Ashar)
    fmt.Printf("   Maghrib: %s\n", data.PrayerTimes.Maghrib)
    fmt.Printf("   Isya: %s\n", data.PrayerTimes.Isya)
    fmt.Printf("📍 Location: %s\n", data.Location.City)
}
```

---

### 🔵 C# (.NET)

```csharp
using System;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

class Program
{
    static async Task Main(string[] args)
    {
        var client = new HttpClient();
        var url = "http://192.168.1.100/api/data";
        
        try
        {
            var response = await client.GetStringAsync(url);
            var data = JsonSerializer.Deserialize<PrayerData>(response);
            
            Console.WriteLine($"⏰ Current Time: {data.time}");
            Console.WriteLine($"📅 Date: {data.date} ({data.day})");
            Console.WriteLine("🕌 Prayer Times:");
            Console.WriteLine($"   Subuh: {data.prayerTimes.subuh}");
            Console.WriteLine($"   Dzuhur: {data.prayerTimes.dzuhur}");
            Console.WriteLine($"   Ashar: {data.prayerTimes.ashar}");
            Console.WriteLine($"   Maghrib: {data.prayerTimes.maghrib}");
            Console.WriteLine($"   Isya: {data.prayerTimes.isya}");
            Console.WriteLine($"📍 Location: {data.location.city}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"❌ Error: {ex.Message}");
        }
    }
}

public class PrayerData
{
    public string time { get; set; }
    public string date { get; set; }
    public string day { get; set; }
    public PrayerTimes prayerTimes { get; set; }
    public Location location { get; set; }
}

public class PrayerTimes
{
    public string subuh { get; set; }
    public string dzuhur { get; set; }
    public string ashar { get; set; }
    public string maghrib { get; set; }
    public string isya { get; set; }
}

public class Location
{
    public string city { get; set; }
}
```

---

### 💡 cURL (Command Line)

```bash
# Simple GET request
curl http://192.168.1.100/api/data

# Pretty print dengan jq
curl -s http://192.168.1.100/api/data | jq '.'

# Extract specific data
curl -s http://192.168.1.100/api/data | jq '.prayerTimes'

# Get only Subuh time
curl -s http://192.168.1.100/api/data | jq -r '.prayerTimes.subuh'

# Save to file
curl -s http://192.168.1.100/api/data -o prayer_data.json

# Watch mode (update every 1 second)
watch -n 1 'curl -s http://192.168.1.100/api/data | jq "."'
```

**Advanced bash script:**
```bash
#!/bin/bash

URL="http://192.168.1.100/api/data"

while true; do
    clear
    DATA=$(curl -s $URL)
    
    TIME=$(echo $DATA | jq -r '.time')
    DATE=$(echo $DATA | jq -r '.date')
    CITY=$(echo $DATA | jq -r '.location.city')
    SUBUH=$(echo $DATA | jq -r '.prayerTimes.subuh')
    DZUHUR=$(echo $DATA | jq -r '.prayerTimes.dzuhur')
    ASHAR=$(echo $DATA | jq -r '.prayerTimes.ashar')
    MAGHRIB=$(echo $DATA | jq -r '.prayerTimes.maghrib')
    ISYA=$(echo $DATA | jq -r '.prayerTimes.isya')
    
    echo "=================================="
    echo "🕌 PRAYER CLOCK - $CITY"
    echo "=================================="
    echo "⏰ $TIME  📅 $DATE"
    echo "----------------------------------"
    echo "Subuh   : $SUBUH"
    echo "Dzuhur  : $DZUHUR"
    echo "Ashar   : $ASHAR"
    echo "Maghrib : $MAGHRIB"
    echo "Isya    : $ISYA"
    echo "=================================="
    
    sleep 1
done
```

## 📊 Response Fields Description

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
| `location.city` | string | Selected city name (display) |
| `location.cityId` | string | City ID for API reference |
| `device.wifiConnected` | boolean | WiFi connection status |
| `device.wifiSSID` | string | Connected WiFi name |
| `device.ip` | string | Device IP address (STA mode) |
| `device.apIP` | string | Access Point IP (192.168.4.1) |
| `device.ntpSynced` | boolean | NTP sync status |
| `device.freeHeap` | number | Free RAM in bytes |

---

## ⚠️ Important Notes

1. **CORS**: API mendukung Cross-Origin requests
2. **Rate Limiting**: Tidak ada rate limit, tapi gunakan dengan bijak
3. **Authentication**: Tidak ada autentikasi (local network only)
4. **SSL/HTTPS**: Tidak support HTTPS (HTTP only)
5. **Caching**: Tidak ada caching, data selalu real-time
6. **Timeout**: Set timeout 5-10 detik untuk request yang aman

---

## 🆘 Troubleshooting

**Cannot connect to API:**
```bash
# Check IP address
ping 192.168.1.100

# Test connection
curl -v http://192.168.1.100/api/data

# Check if device is on same network
```

**Empty/Invalid JSON:**
- Pastikan WiFi terhubung
- Cek apakah city sudah dipilih
- Restart device jika perlu

**Timeout errors:**
- Tingkatkan timeout di code (misal: 10 detik)
- Cek kualitas sinyal WiFi
- Pastikan tidak ada firewall blocking

## 📋 TODO

- [ ] HTTP support
- [ ] OTA updates
- [ ] Adhan sound/notification
- [ ] Hijri calendar
- [ ] Qibla direction
- [ ] Dark/Light theme
- [ ] Manual prayer times refresh button

## 🙏 Credits

- [LVGL](https://lvgl.io/) - GUI library
- [Aladhan API](https://aladhan.com/prayer-times-api) - Prayer times
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Display driver
- [RTClib](https://github.com/adafruit/RTClib) - RTC driver

---

**Made with ❤️ for the GONIT Technology**
