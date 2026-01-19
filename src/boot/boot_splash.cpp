#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include "boot_splash.h"
#include "ino_helper.h"
#include "boot_sequence.h"

// use ST7796S
// if not available, try ST7789 as fallback (reason for this is that I may be changing to a different board later in the project which is unsupported)
#ifdef ARDUINO
    #if __has_include(<Adafruit_ST7796S.h>)
        #include <Adafruit_ST7796S.h>
        #define USE_ST7796S
    #else
        // fallback to ST7789, works with ST7796S hardware
        #include <Adafruit_ST7789.h>
        #define USE_ST7789
        #pragma message "ST7796S not found, using ST7789 as fallback"
    #endif
#else
    #include <Adafruit_ST7789.h>
    #define USE_ST7789
#endif

// TFT pin definitions
#define TFT_CS    5
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_MISO  13
#define TFT_DC    21
#define TFT_RST   4  // reset connected to GPIO 4

// SD card pin definitions (from WIRING.md)
#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

// Logo file path
#define LOGO_PATH "/boot/logo.raw"

// display object (exported for boot_sequence.c)
#ifdef USE_ST7796S
    Adafruit_ST7796S tft = Adafruit_ST7796S(&SPI, TFT_CS, TFT_DC, TFT_RST);
#else
    Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
#endif

void boot_init(void) {
    // configure pins manually first
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    if (TFT_RST >= 0) {
        pinMode(TFT_RST, OUTPUT);
        // reset sequence: pull low then high
        digitalWrite(TFT_RST, LOW);
        delay(50);  // longer reset pulse
        digitalWrite(TFT_RST, HIGH);
        delay(50);  // wait after reset release
    }
    digitalWrite(TFT_CS, HIGH);  // CS high = deselected
    digitalWrite(TFT_DC, HIGH);
    
    // initialize SPI - ESP32-S3 SPI.begin(SCK, MISO, MOSI, SS)
    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    delay(800);  // delay for display power-up
    
    // init display
    #ifdef USE_ST7796S
        ESP_INFO("initializing ST7796S display...");
    #else
        ESP_INFO("initializing ST7789 display");
    #endif
    
    // init display - IMPORTANT: (320, 480) NOT (480, 320) per wiring notes
    tft.init(320, 480);

    delay(800);
    
    // rotate 180 degrees from current landscape orientation
    tft.setRotation(3);
    delay(100);
    
    // verify rotation worked - after rotation, dimensions swap
    ESP_INFO("Display after rotation: %dx%d", tft.width(), tft.height());
    
    tft.fillScreen(ST77XX_WHITE);   // this is extremely stupid, white is actually black
    delay(100);
    
    
    // Set text properties
    tft.setTextSize(4);
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);  // black text on white background
    tft.setTextWrap(false);
    
}

void boot_refresh(void) {
    // ensure reset pin stays HIGH to keep display active
    if (TFT_RST >= 0) {
        digitalWrite(TFT_RST, HIGH);
    }
    
    tft.setTextSize(4);
    tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    tft.setTextWrap(false);
    
}

void boot_tft_shutdown(void) {
#ifdef ARDUINO
    // blank the display before powering down
    tft.fillScreen(ST77XX_BLACK);
    delay(50);
    if (TFT_RST >= 0) {
        digitalWrite(TFT_RST, LOW);
    }
    digitalWrite(TFT_CS, HIGH);
#endif
}

int boot_draw_rgb565_scaled(const char *path, int16_t x, int16_t y,
                            int16_t width, int16_t height) {
#ifdef ARDUINO
    if (path == NULL || path[0] == '\0' || width <= 0 || height <= 0) {
        return 0;
    }
    
    const int16_t src_w = 480;
    const int16_t src_h = 320;
    const size_t expected = (size_t)src_w * (size_t)src_h * 2;
    
    boot_sd_switch_to_sd_spi();
    File logo = SD.open(path, FILE_READ);
    if (!logo) {
        boot_sd_restore_tft_spi();
        return 0;
    }
    if ((size_t)logo.size() < expected) {
        logo.close();
        boot_sd_restore_tft_spi();
        return 0;
    }
    
    size_t src_row_bytes = (size_t)src_w * 2;
    uint8_t *src_row = (uint8_t*)malloc(src_row_bytes);
    uint16_t *dest_row = (uint16_t*)malloc((size_t)width * 2);
    if (src_row == NULL || dest_row == NULL) {
        if (src_row) free(src_row);
        if (dest_row) free(dest_row);
        logo.close();
        boot_sd_restore_tft_spi();
        return 0;
    }
    
    for (int16_t dy = 0; dy < height; dy++) {
        int16_t sy = (int16_t)((dy * src_h) / height);
        size_t offset = (size_t)sy * src_row_bytes;
        boot_sd_switch_to_sd_spi();
        logo.seek(offset);
        size_t read_bytes = logo.read(src_row, src_row_bytes);
        boot_sd_restore_tft_spi();
        if (read_bytes != src_row_bytes) {
            break;
        }
        
        for (int16_t dx = 0; dx < width; dx++) {
            int16_t sx = (int16_t)((dx * src_w) / width);
            size_t idx = (size_t)sx * 2;
            uint16_t pix = (uint16_t)src_row[idx] | ((uint16_t)src_row[idx + 1] << 8);
            dest_row[dx] = pix;
        }
        tft.drawRGBBitmap(x, y + dy, dest_row, width, 1);
    }
    
    free(src_row);
    free(dest_row);
    logo.close();
    return 1;
#else
    (void)path;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return 0;
#endif
}

int boot_show_logo_from_sd(const char *path) {
#ifdef ARDUINO
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    
    int16_t width = tft.width();
    int16_t height = tft.height();
    if (width <= 0 || height <= 0) {
        return 0;
    }
    
    const int16_t max_chunk_lines = 20;
    int16_t chunk_lines = max_chunk_lines;
    if (chunk_lines > height) {
        chunk_lines = height;
    }
    size_t chunk_bytes = (size_t)width * (size_t)chunk_lines * 2;
    uint16_t *chunk = (uint16_t*)malloc(chunk_bytes);
    if (chunk == NULL) {
        return 0;
    }
    
    boot_sd_switch_to_sd_spi();
    File logo = SD.open(path, FILE_READ);
    if (!logo) {
        boot_sd_restore_tft_spi();
        free(chunk);
        return 0;
    }
    
    size_t expected = (size_t)width * (size_t)height * 2;
    if (logo.size() < expected) {
        logo.close();
        boot_sd_restore_tft_spi();
        free(chunk);
        return 0;
    }
    
    tft.fillScreen(ST77XX_WHITE);
    
    int16_t y = 0;
    while (y < height) {
        int16_t lines = chunk_lines;
        if (y + lines > height) {
            lines = height - y;
        }
        size_t bytes_to_read = (size_t)width * (size_t)lines * 2;
        
        boot_sd_switch_to_sd_spi();
        size_t read_bytes = logo.read((uint8_t*)chunk, bytes_to_read);
        boot_sd_restore_tft_spi();
        if (read_bytes != bytes_to_read) {
            break;
        }
        
        tft.drawRGBBitmap(0, y, chunk, width, lines);
        
        y += lines;
    }
    
    logo.close();
    free(chunk);
    return 1;
#else
    (void)path;
    return 0;
#endif
}

// C wrapper functions for boot_sequence.c to access TFT
extern "C" {
    // color constants for C code
    #define BOOT_COLOR_BLACK   0x0000
    #define BOOT_COLOR_WHITE   0xFFFF
    #define BOOT_COLOR_GREEN   0x07E0
    #define BOOT_COLOR_RED     0xF800
    
    void boot_tft_set_cursor(int16_t x, int16_t y) {
        tft.setCursor(x, y);
    }
    
    void boot_tft_set_text_size(uint8_t size) {
        tft.setTextSize(size);
    }
    
    void boot_tft_set_text_color(uint16_t color, uint16_t bg) {
        tft.setTextColor(color, bg);
    }
    
    void boot_tft_print(const char *str) {
        tft.print(str);
    }
    
    void boot_tft_fill_screen(uint16_t color) {
        tft.fillScreen(color);
    }
    
    void boot_tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        tft.fillRect(x, y, w, h, color);
    }
    
    void boot_tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        tft.drawRect(x, y, w, h, color);
    }

    void boot_tft_draw_rgb565(int16_t x, int16_t y, const uint16_t *data,
                              int16_t width, int16_t height) {
        if (data == NULL || width <= 0 || height <= 0) {
            return;
        }
        tft.drawRGBBitmap(x, y, data, width, height);
    }
    
    int16_t boot_tft_get_width(void) {
        return tft.width();
    }
    
    int16_t boot_tft_get_height(void) {
        return tft.height();
    }
}

