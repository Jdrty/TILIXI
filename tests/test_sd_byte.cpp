#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "boot_splash.h"
#include "ino_helper.h"
#include "debug_helper.h"

#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

#define TFT_CS    5
#define TFT_SCK   12
#define TFT_MISO  13
#define TFT_MOSI  11

#define TEST_FILENAME "/test_byte.bin"
#define TEST_SIZE 256

static int run_byte_test(void) {
    Serial.println("\nByte-level SD Card Test");
    Serial.flush();
    
    if (SD.cardType() == CARD_NONE) {
        Serial.println("ERROR: SD card not available");
        Serial.flush();
        return -1;
    }
    
    Serial.print("Card type: ");
    uint8_t card_type = SD.cardType();
    switch (card_type) {
        case CARD_MMC: Serial.println("MMC"); break;
        case CARD_SD: Serial.println("SD"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default: Serial.println("Unknown"); break;
    }
    Serial.flush();
    
    if (SD.exists(TEST_FILENAME)) {
        SD.remove(TEST_FILENAME);
    }
    
    Serial.println("\nTest 1: Write byte pattern");
    Serial.flush();
    
    File test_file = SD.open(TEST_FILENAME, FILE_WRITE);
    if (!test_file) {
        Serial.println("ERROR: Failed to open file for writing");
        Serial.flush();
        return -1;
    }
    
    for (uint16_t i = 0; i < TEST_SIZE; i++) {
        uint8_t byte_val = (uint8_t)(i & 0xFF);
        if (test_file.write(byte_val) != 1) {
            Serial.print("ERROR: Write failed at offset ");
            Serial.println(i);
            Serial.flush();
            test_file.close();
            return -1;
        }
    }
    test_file.close();
    Serial.println("PASS: Wrote 256 bytes");
    Serial.flush();
    delay(100);
    
    Serial.println("\nTest 2: Read and verify byte pattern");
    Serial.flush();
    
    test_file = SD.open(TEST_FILENAME, FILE_READ);
    if (!test_file) {
        Serial.println("ERROR: Failed to open file for reading");
        Serial.flush();
        return -1;
    }
    
    uint8_t read_byte;
    for (uint16_t i = 0; i < TEST_SIZE; i++) {
        if (test_file.available() == 0) {
            Serial.print("ERROR: Unexpected EOF at offset ");
            Serial.println(i);
            Serial.flush();
            test_file.close();
            return -1;
        }
        
        read_byte = test_file.read();
        uint8_t expected = (uint8_t)(i & 0xFF);
        
        if (read_byte != expected) {
            Serial.print("ERROR: Mismatch at offset ");
            Serial.print(i);
            Serial.print(": expected 0x");
            Serial.print(expected, HEX);
            Serial.print(", got 0x");
            Serial.println(read_byte, HEX);
            Serial.flush();
            test_file.close();
            return -1;
        }
    }
    test_file.close();
    Serial.println("PASS: Verified 256 bytes");
    Serial.flush();
    delay(100);
    
    Serial.println("\nTest 3: Random byte access");
    Serial.flush();
    
    test_file = SD.open(TEST_FILENAME, FILE_WRITE);
    if (!test_file) {
        Serial.println("ERROR: Failed to reopen file for writing");
        Serial.flush();
        return -1;
    }
    
    uint16_t test_offsets[] = {0, 1, 127, 128, 255};
    uint8_t test_values[] = {0xAA, 0x55, 0xF0, 0x0F, 0xFF};
    
    for (uint8_t i = 0; i < 5; i++) {
        if (!test_file.seek(test_offsets[i])) {
            Serial.print("ERROR: Seek failed to offset ");
            Serial.println(test_offsets[i]);
            Serial.flush();
            test_file.close();
            return -1;
        }
        
        if (test_file.write(test_values[i]) != 1) {
            Serial.print("ERROR: Write failed at offset ");
            Serial.println(test_offsets[i]);
            Serial.flush();
            test_file.close();
            return -1;
        }
    }
    test_file.close();
    Serial.println("PASS: Random byte writes");
    Serial.flush();
    delay(100);
    
    Serial.println("\nTest 4: Verify random byte writes");
    Serial.flush();
    
    test_file = SD.open(TEST_FILENAME, FILE_READ);
    if (!test_file) {
        Serial.println("ERROR: Failed to open file for verification");
        Serial.flush();
        return -1;
    }
    
    for (uint8_t i = 0; i < 5; i++) {
        if (!test_file.seek(test_offsets[i])) {
            Serial.print("ERROR: Seek failed to offset ");
            Serial.println(test_offsets[i]);
            Serial.flush();
            test_file.close();
            return -1;
        }
        
        read_byte = test_file.read();
        if (read_byte != test_values[i]) {
            Serial.print("ERROR: Verification failed at offset ");
            Serial.print(test_offsets[i]);
            Serial.print(": expected 0x");
            Serial.print(test_values[i], HEX);
            Serial.print(", got 0x");
            Serial.println(read_byte, HEX);
            Serial.flush();
            test_file.close();
            return -1;
        }
    }
    test_file.close();
    Serial.println("PASS: Random byte verification");
    Serial.flush();
    delay(100);
    
    SD.remove(TEST_FILENAME);
    
    Serial.println("\n=== All Tests Passed ===");
    Serial.flush();
    
    return 0;
}

void setup(void) {
    Serial.begin(115200);
    delay_ms(1000);
    
    boot_init();
    
    Serial.println("\nSD Card Byte Test");
    Serial.println("Initializing SD card...");
    Serial.flush();
    
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(200);
    
    digitalWrite(TFT_CS, HIGH);
    SPI.end();
    delay(50);
    
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("ERROR: SD card initialization failed");
        Serial.flush();
        return;
    }
    
    uint8_t card_type = SD.cardType();
    if (card_type == CARD_NONE) {
        Serial.println("ERROR: No SD card detected");
        Serial.flush();
        return;
    }
    
    Serial.println("SD card mounted successfully");
    Serial.flush();
    delay(500);
    
    int test_result = run_byte_test();
    
    if (test_result == 0) {
        Serial.println("\nTEST SUITE: PASSED");
    } else {
        Serial.println("\nTEST SUITE: FAILED");
    }
    Serial.flush();
}

void loop(void) {
    delay(1000);
}

