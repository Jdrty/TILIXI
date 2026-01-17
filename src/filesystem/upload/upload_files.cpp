#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

#include "upload_payload.h"

#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

static bool ensure_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    if (SD.exists(path)) {
        return true;
    }
    return SD.mkdir(path);
}

static void ensure_parent_dirs(const char *path) {
    char temp[256];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) {
        return;
    }
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    for (size_t i = 1; i < len; i++) {
        if (temp[i] == '/') {
            temp[i] = '\0';
            if (strlen(temp) > 0) {
                ensure_dir(temp);
            }
            temp[i] = '/';
        }
    }
}

void setup(void) {
    Serial.begin(115200);
    delay(1000);

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(100);

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);

    if (!SD.begin(SD_CS)) {
        Serial.println("ERROR: SD card initialization failed");
        return;
    }

    ensure_dir("/upload");

    for (size_t i = 0; i < upload_file_count; i++) {
        char dest_path[256];
        const char *rel = upload_files[i].path;
        snprintf(dest_path, sizeof(dest_path), "/upload/%s", rel);
        ensure_parent_dirs(dest_path);

        if (SD.exists(dest_path)) {
            SD.remove(dest_path);
        }
        File f = SD.open(dest_path, FILE_WRITE);
        if (!f) {
            Serial.print("ERROR: failed to open ");
            Serial.println(dest_path);
            continue;
        }
        f.write(upload_files[i].data, upload_files[i].size);
        f.close();
    }

    Serial.println("Upload complete.");
}

void loop(void) {
    delay(1000);
}
