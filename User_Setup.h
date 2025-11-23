// User_Setup.h untuk ESP32-2432S028R (Cheap Yellow Display)
// Letakkan file ini di folder: Arduino/libraries/TFT_eSPI/User_Setup.h
// Atau gunakan User_Setup_Select.h untuk memilih konfigurasi ini

// PENTING: Pastikan hanya ada SATU driver yang aktif
#define ILI9341_DRIVER      // Driver untuk ILI9341 2.8" display
//#define ILI9341_2_DRIVER  // Coba ini jika yang atas tidak bekerja

// ESP32 Pins untuk ESP32-2432S028R
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin
#define TFT_DC   2   // Data Command control pin
#define TFT_RST  -1  // Reset pin (set -1 jika RST terhubung ke ESP32 EN)
// Atau coba: #define TFT_RST 22 jika -1 tidak bekerja

// Backlight Control
#define TFT_BL   21          // LED back-light
#define TFT_BACKLIGHT_ON HIGH  // Level untuk menyalakan backlight

// Kecepatan SPI - MULAI DENGAN KECEPATAN RENDAH
#define SPI_FREQUENCY  27000000  // 27MHz (lebih stabil)
// Jika masih tidak bekerja, coba: #define SPI_FREQUENCY  10000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Inversi warna (coba toggle jika warna aneh)
//#define TFT_INVERSION_ON
//#define TFT_INVERSION_OFF

// RGB order (uncomment jika warna BGR, bukan RGB)
//#define TFT_RGB_ORDER TFT_BGR

// Font support
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// Untuk debugging
#define TFT_WIDTH  240
#define TFT_HEIGHT 320