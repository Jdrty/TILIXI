// this is just to connect C and C++ since SD library is C++ only

#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "boot_sequence.h"
#include "debug_helper.h"

// SD card pin definitions
#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

// TFT pin definitions (needed to restore SPI after SD init)
#define TFT_CS    5
#define TFT_SCK   12
#define TFT_MISO  13
#define TFT_MOSI  11

extern "C" {

// init SD card hardware and mount filesystem
int boot_sd_mount(void) {
    // configure CS pin for SD card
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);  // deselected
    
    delay(200);  // allow SD card to stabilize
    
    // ensure TFT CS is deselected first, redundant since it should already be high from boot_init
    digitalWrite(TFT_CS, HIGH);
    
    // end SPI to clean up TFT configuration before reinitializing for SD
    SPI.end();
    delay(50);
    
    // init SPI for SD card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    // init SD card
    if (!SD.begin(SD_CS)) {
        DEBUG_PRINT("[BOOT] SD.begin() failed\n");
        // restore SPI for TFT before returning
        SPI.end();
        SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
        return -1;  // SD card not present or initialization failed
    }
    
    // check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        DEBUG_PRINT("[BOOT] SD card type is CARD_NONE\n");
        // restore SPI for TFT before returning
        SPI.end();
        SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
        return -1;  // no SD card (T_T)
    }
    
    DEBUG_PRINT("[BOOT] SD card mounted successfully (type: %d)\n", cardType);
    
    // SPI is now configured for SD card pins
    // keep it this way during filesystem initialization. DONT TOUCH ヽ( `д´*)ノ
    
    return 0;
}

// restore SPI configuration for TFT display
// call this after filesystem initialization is complete
void boot_sd_restore_tft_spi(void) {
    // ensure SD CS is deselected
    digitalWrite(SD_CS, HIGH);
    
    // end SPI to clean up SD configuration
    SPI.end();
    delay(50);
    
    // restore SPI for TFT
    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    delay(50);
}

// switch SPI back to SD card configuration
// call this before SD operations if TFT SPI was restored
void boot_sd_switch_to_sd_spi(void) {
    // ensure TFT CS is deselected
    digitalWrite(TFT_CS, HIGH);
    
    // end SPI to clean up TFT configuration
    SPI.end();
    delay(50);
    
    // switch SPI to SD pins
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(50);
}

// check if SD card is available
int boot_sd_available(void) {
    // ensure SPI is configured for SD before SD operations
    boot_sd_switch_to_sd_spi();
    
    uint8_t cardType = SD.cardType();
    return (cardType != CARD_NONE) ? 0 : -1;
}

// check if a directory is empty
int boot_sd_is_directory_empty(const char *path) {
    // ensure SPI is configured for SD before SD operations
    boot_sd_switch_to_sd_spi();
    
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        return 1;  // treat as empty if can't open or not a directory
    }
    
    // check if directory has any entries
    File entry = dir.openNextFile();
    dir.close();
    
    if (entry) {
        entry.close();
        return 0;  // directory has at least one entry
    }
    
    return 1;  // directory is empty
}

// ensure a directory exists, create if it doesn't
int boot_sd_ensure_directory(const char *path) {
    // ensure SPI is configured for SD before SD operations
    boot_sd_switch_to_sd_spi();
    
    if (SD.exists(path)) {
        // check if it's actually a directory
        File f = SD.open(path);
        if (f && f.isDirectory()) {
            f.close();
            return 0;  // directory exists
        }
        f.close();
        return -1;
    }
    
    // create directory
    if (!SD.mkdir(path)) {
        DEBUG_PRINT("[BOOT] Failed to create directory: %s\n", path);
        return -1;
    }
    
    DEBUG_PRINT("[BOOT] Created directory: %s\n", path);
    return 0;
}

// ensure a file exists, create if it doesn't
int boot_sd_ensure_file(const char *path, const char *content) {
    // ensure SPI is configured for SD before SD operations
    boot_sd_switch_to_sd_spi();
    
    if (SD.exists(path)) {
        return 0;  // file already exists
    }
    
    // create file with content
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        DEBUG_PRINT("[BOOT] Failed to create file: %s\n", path);
        return -1;
    }
    
    if (content) {
        f.print(content);
    }
    f.close();
    
    DEBUG_PRINT("[BOOT] Created file: %s\n", path);
    return 0;
}

} // extern "C"

#else
// for pc build with no SD card support.
extern "C" {
int boot_sd_mount(void) { return 0; }
void boot_sd_restore_tft_spi(void) { }
void boot_sd_switch_to_sd_spi(void) { }
int boot_sd_available(void) { return 0; }
int boot_sd_is_directory_empty(const char *path) { (void)path; return 1; }
int boot_sd_ensure_directory(const char *path) { (void)path; return 0; }
int boot_sd_ensure_file(const char *path, const char *content) { (void)path; (void)content; return 0; }
}
#endif

