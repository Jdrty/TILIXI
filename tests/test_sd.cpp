#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

#define SD_CS    18
#define SD_SCK   17
#define SD_MOSI  15
#define SD_MISO  16

#define TEST_FILENAME "/test.txt"
#define TEST_CONTENT "test"

void setup(void) {
    // initialize serial for debug output
    Serial.begin(115200);
    
    // wait for serial monitor to connect
    delay(3000);
    Serial.flush();
    
    // send startup message
    Serial.println("\n\n");
    Serial.println("microSD Card Read/Write Test");
    Serial.println();
    Serial.flush();
    
    delay(500);
    
    Serial.println("initializing SD card...");
    Serial.flush();
    
    // configure CS pin for SD card
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);  // CS high = deselected
    
    delay(200);  // allow SD card to stabilize
    
    // initialize SPI for SD card with custom pins
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);
    
    // initialize SD card
    Serial.print("attempting SD.begin()... ");
    Serial.flush();
    
    if (!SD.begin(SD_CS)) {
        Serial.println("failed idiot (¬_¬ )");
        Serial.flush();
        return;
    }
    
    Serial.println("works");
    Serial.flush();
    delay(100);
    
    // check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("no sd card?  	(ᓀ ᓀ)");
        Serial.flush();
        return;
    }
    
    Serial.print("card type: ");
    switch (cardType) {
        case CARD_MMC:
            Serial.println("MMC");
            break;
        case CARD_SD:
            Serial.println("SD");
            break;
        case CARD_SDHC:
            Serial.println("SDHC");
            break;
        default:
            Serial.println("Unknown");
            break;
    }
    Serial.flush();
    
    // get card size
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);  // size in MB
    Serial.print("card size: ");
    Serial.print(cardSize);
    Serial.println(" MB");
    Serial.println();
    Serial.flush();
    
    // Test 1: Write to test.txt
    Serial.println("test 1: Writing to " TEST_FILENAME);
    Serial.flush();
    
    File testFile = SD.open(TEST_FILENAME, FILE_WRITE);
    if (!testFile) {
        Serial.println("can't open file for writing!! are you sure you formatted this");
        Serial.flush();
        return;
    }
    
    size_t bytesWritten = testFile.print(TEST_CONTENT);
    testFile.close();
    
    if (bytesWritten != strlen(TEST_CONTENT)) {
        Serial.print("write failed! expected ");
        Serial.print(strlen(TEST_CONTENT));
        Serial.print(" bytes, wrote ");
        Serial.println(bytesWritten);
        Serial.flush();
        return;
    }
    
    Serial.print("works, wrote ");
    Serial.print(bytesWritten);
    Serial.print(" bytes: \"");
    Serial.print(TEST_CONTENT);
    Serial.println("\"");
    Serial.flush();
    delay(100);
    
    // Test 2: Read from test.txt
    Serial.println();
    Serial.println("test 2: reading from " TEST_FILENAME);
    Serial.flush();
    
    testFile = SD.open(TEST_FILENAME, FILE_READ);
    if (!testFile) {
        Serial.println("failed to open file for reading! huh, howd this happen");
        Serial.flush();
        return;
    }
    
    // Read the file content
    String readContent = "";
    while (testFile.available()) {
        readContent += (char)testFile.read();
    }
    testFile.close();
    
    Serial.print("read content: \"");
    Serial.print(readContent);
    Serial.println("\"");
    Serial.flush();
    
    // Verify content matches
    if (readContent != TEST_CONTENT) {
        Serial.print("content mismatch! Expected \"");
        Serial.print(TEST_CONTENT);
        Serial.print("\", got \"");
        Serial.print(readContent);
        Serial.println("\"");
        Serial.flush();
        return;
    }
    
    Serial.println("works, content matches expected");
    Serial.flush();
    delay(100);
    
    // Test 3: Delete test.txt
    Serial.println();
    Serial.println("all done! deleting " TEST_FILENAME);
    Serial.flush();
    
    if (!SD.remove(TEST_FILENAME)) {
        Serial.println("ummmm, failed to delete file... howd we get this far..");
        Serial.flush();
        return;
    }
    
    Serial.println("file deleted");
    Serial.flush();
    delay(100);
    
    // Verify file is gone
    if (SD.exists(TEST_FILENAME)) {
        Serial.println("file still exists after deletion?? erm");
        Serial.flush();
        return;
    }
    
    Serial.println("file confirmed deleted!");
    Serial.flush();
    
    // Final summary
    Serial.println();
    Serial.println("ALL TESTS PASSED!!!!");
    Serial.flush();
}

void loop(void) {
    // just loop forever
    delay(1000);
}

