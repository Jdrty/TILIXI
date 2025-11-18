#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "filesystem.h"
#include "ino_helper.h"

// SD card pin definitions (from WIRING.md)
#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

void printDirectory(File dir, int numTabs) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        
        for (uint8_t i = 0; i < numTabs; i++) {
            Serial.print("  ");
        }
        
        Serial.print(entry.name());
        if (entry.isDirectory()) {
            Serial.println("/");
            Serial.flush();
            printDirectory(entry, numTabs + 1);
        } else {
            // files have sizes, directories do not
            Serial.print("\t\t");
            Serial.print(entry.size(), DEC);
            Serial.println(" bytes");
            Serial.flush();
        }
        entry.close();
    }
}

void setup(void) {
    // initialize serial for debug output
    Serial.begin(115200);
    
    // Wait longer for serial monitor to connect
    delay(3000);
    Serial.flush();
    
    // Send startup message immediately
    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("=== MicroSD Card Contents Reader ===");
    Serial.println("========================================");
    Serial.println();
    Serial.flush();
    
    delay(500);
    
    Serial.println("Initializing SD card...");
    Serial.flush();
    
    // configure CS pin for SD card
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);  // CS high = deselected
    
    delay(200);  // allow SD card to stabilize
    
    // initialize SPI for SD card with custom pins
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    // initialize SD card
    Serial.print("Attempting SD.begin()... ");
    Serial.flush();
    
    if (!SD.begin(SD_CS)) {
        Serial.println("FAILED!");
        Serial.println();
        Serial.println("SD card initialization failed!");
        Serial.println("Please check:");
        Serial.println("  - SD card is inserted");
        Serial.println("  - SD card is properly formatted (FAT32)");
        Serial.println("  - Wiring is correct (see docs/WIRING.md)");
        Serial.flush();
        return;
    }
    
    Serial.println("SUCCESS!");
    Serial.flush();
    delay(100);
    
    // check card type
    uint8_t cardType = SD.cardType();
    const char* cardTypeStr = "Unknown";
    switch (cardType) {
        case CARD_NONE:
            cardTypeStr = "NONE";
            break;
        case CARD_MMC:
            cardTypeStr = "MMC";
            break;
        case CARD_SD:
            cardTypeStr = "SD";
            break;
        case CARD_SDHC:
            cardTypeStr = "SDHC";
            break;
        default:
            cardTypeStr = "Unknown";
            break;
    }
    
    Serial.print("Card type: ");
    Serial.println(cardTypeStr);
    Serial.flush();
    
    if (cardType == CARD_NONE) {
        Serial.println("ERROR: No SD card detected!");
        Serial.flush();
        return;
    }
    
    // get card size
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);  // size in MB
    Serial.print("Card size: ");
    Serial.print(cardSize);
    Serial.println(" MB");
    Serial.println();
    Serial.flush();
    
    // print root directory contents
    Serial.println("Root directory contents:");
    Serial.println("================================");
    Serial.flush();
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("ERROR: Failed to open root directory!");
        Serial.flush();
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("ERROR: Root is not a directory!");
        root.close();
        Serial.flush();
        return;
    }
    
    printDirectory(root, 0);
    root.close();
    
    Serial.println("================================");
    Serial.println();
    Serial.println("File read test complete.");
    Serial.flush();
}

void loop(void) {
    // just loop forever
    delay_ms(1000);
}

