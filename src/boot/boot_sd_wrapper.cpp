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

int boot_sd_get_username(char *out_name, size_t out_len) {
    if (out_name == NULL || out_len == 0) {
        return 0;
    }
    out_name[0] = '\0';
    
    boot_sd_switch_to_sd_spi();
    File f = SD.open("/etc/passwd", FILE_READ);
    if (!f) {
        boot_sd_restore_tft_spi();
        return 0;
    }
    
    char buf[128];
    size_t read_len = f.readBytesUntil('\n', buf, sizeof(buf) - 1);
    f.close();
    boot_sd_restore_tft_spi();
    if (read_len == 0) {
        return 0;
    }
    buf[read_len] = '\0';
    char *colon = strchr(buf, ':');
    if (colon != NULL) {
        *colon = '\0';
    }
    if (buf[0] == '\0') {
        return 0;
    }
    strncpy(out_name, buf, out_len - 1);
    out_name[out_len - 1] = '\0';
    return 1;
}

static int has_rgb565_ext(const char *name) {
    size_t len = strlen(name);
    const char *ext = ".rgb565";
    size_t ext_len = strlen(ext);
    if (len < ext_len) {
        return 0;
    }
    return strcmp(name + len - ext_len, ext) == 0;
}

static const char *basename_ptr(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int boot_sd_find_bootlogo(const char *username, char *out_path, size_t out_len) {
    if (username == NULL || username[0] == '\0' || out_path == NULL || out_len == 0) {
        return 0;
    }
    out_path[0] = '\0';
    
    char dir_path[128];
    snprintf(dir_path, sizeof(dir_path), "/home/%s/.config/boot", username);
    
    boot_sd_switch_to_sd_spi();
    File dir = SD.open(dir_path);
    if (!dir || !dir.isDirectory()) {
        if (dir) {
            dir.close();
        }
        boot_sd_restore_tft_spi();
        return 0;
    }
    
    char best_name[64] = {0};
    char best_path[256] = {0};
    
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        const char *name = entry.name();
        const char *base = basename_ptr(name);
        if (has_rgb565_ext(base)) {
            if (best_name[0] == '\0' || strcmp(base, best_name) < 0) {
                strncpy(best_name, base, sizeof(best_name) - 1);
                best_name[sizeof(best_name) - 1] = '\0';
                if (name[0] == '/') {
                    strncpy(best_path, name, sizeof(best_path) - 1);
                    best_path[sizeof(best_path) - 1] = '\0';
                } else {
                    snprintf(best_path, sizeof(best_path), "%s/%s", dir_path, base);
                }
            }
        }
        entry.close();
    }
    dir.close();
    boot_sd_restore_tft_spi();
    
    if (best_path[0] == '\0') {
        return 0;
    }
    strncpy(out_path, best_path, out_len - 1);
    out_path[out_len - 1] = '\0';
    return 1;
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
int boot_sd_get_username(char *out_name, size_t out_len) { (void)out_name; (void)out_len; return 0; }
int boot_sd_find_bootlogo(const char *username, char *out_path, size_t out_len) { (void)username; (void)out_path; (void)out_len; return 0; }
}
#endif

