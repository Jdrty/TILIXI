#include <Arduino.h>
#include <SPI.h>

#define SD_FAT_TYPE 3
#define ENABLE_DEDICATED_SPI 1
#include "SdFat.h"
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

#define SECTOR_SIZE 512
#define TEST_SECTOR_COUNT 4
#define MAX_SAVED_SECTORS 10

SdFs sd;   // frick my unabstracted chud life; they depricated SdSpiCard

typedef struct {
    uint32_t lba;
    uint8_t data[SECTOR_SIZE];
    uint8_t saved;
} saved_sector_t;

static saved_sector_t saved_sectors[MAX_SAVED_SECTORS];
static uint8_t saved_count = 0;

static void switch_to_sd_spi(void) {
    digitalWrite(TFT_CS, HIGH);
    SPI.end();
    delay(50);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(50);
}

// this looks very wrong but its bc of weird signatures, dont blame me blame the library
static int read_block(uint32_t lba, uint8_t *buf) {
    SdSpiCard* card = static_cast<SdSpiCard*>(sd.card());
    if (!card) return -1;

    if (!card->readStart(lba)) return -1;
    if (!card->readData(buf)) {
        card->readStop();
        return -1;
    }
    if (!card->readStop()) return -1;

    return 0;
}

static int write_block(uint32_t lba, const uint8_t *buf) {
    SdSpiCard* card = static_cast<SdSpiCard*>(sd.card());
    if (!card) return -1;

    if (!card->writeStart(lba)) return -1;
    if (!card->writeData(buf)) {
        card->writeStop();
        return -1;
    }
    if (!card->writeStop()) return -1;

    return 0;
}

static int save_sector(uint32_t lba) {
    if (saved_count >= MAX_SAVED_SECTORS) {
        return -1;
    }
    
    for (uint8_t i = 0; i < saved_count; i++) {
        if (saved_sectors[i].lba == lba) {
            return 0;
        }
    }
    
    saved_sector_t *saved = &saved_sectors[saved_count];
    if (read_block(lba, saved->data) != 0) {
        return -1;
    }
    
    saved->lba = lba;
    saved->saved = 1;
    saved_count++;
    
    return 0;
}

static int restore_sectors(void) {
    Serial.println("\nCleaning up test sectors...");
    Serial.flush();
    
    for (uint8_t i = 0; i < saved_count; i++) {
        if (saved_sectors[i].saved) {
            if (write_block(saved_sectors[i].lba, saved_sectors[i].data) != 0) {
                Serial.print("WARNING: Failed to restore sector ");
                Serial.println(saved_sectors[i].lba);
                Serial.flush();
            }
        }
    }
    
    Serial.println("Cleanup complete");
    Serial.flush();
    
    return 0;
}

static void print_hex_dump(const uint8_t *buf, size_t len, size_t offset) {
    for (size_t i = 0; i < len; i += 16) {
        Serial.print("0x");
        Serial.print(offset + i, HEX);
        Serial.print(": ");
        
        for (size_t j = 0; j < 16 && (i + j) < len; j++) {
            if (buf[i + j] < 0x10) Serial.print("0");
            Serial.print(buf[i + j], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

static int test_mbr_bpb(void) {
    uint8_t sector_buf[SECTOR_SIZE];
    
    Serial.println("\nTest 1: Read MBR/BPB (sector 0)");
    Serial.flush();
    
    if (read_block(0, sector_buf) != 0) {
        Serial.println("ERROR: Failed to read sector 0");
        Serial.flush();
        return -1;
    }
    
    Serial.println("PASS: Read sector 0 (MBR/BPB)");
    Serial.print("First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        if (sector_buf[i] < 0x10) Serial.print("0");
        Serial.print(sector_buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.flush();
    
    if (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA) {
        Serial.println("PASS: Valid MBR signature (0x55 0xAA)");
    } else {
        Serial.print("WARNING: Invalid MBR signature: 0x");
        Serial.print(sector_buf[510], HEX);
        Serial.print(" 0x");
        Serial.println(sector_buf[511], HEX);
    }
    Serial.flush();
    
    return 0;
}

static int test_block_alignment(void) {
    uint8_t write_buf[SECTOR_SIZE];
    uint8_t read_buf[SECTOR_SIZE];
    uint32_t test_lba = 1000;
    
    Serial.println("\nTest 2: Block alignment verification");
    Serial.flush();
    
    if (save_sector(test_lba) != 0) {
        Serial.println("WARNING: Failed to save original sector data");
        Serial.flush();
    }
    
    for (uint16_t i = 0; i < SECTOR_SIZE; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    if (write_block(test_lba, write_buf) != 0) {
        Serial.println("ERROR: Failed to write test block");
        Serial.flush();
        return -1;
    }
    
    delay(100);
    
    memset(read_buf, 0, SECTOR_SIZE);
    if (read_block(test_lba, read_buf) != 0) {
        Serial.println("ERROR: Failed to read test block");
        Serial.flush();
        return -1;
    }
    
    for (uint16_t i = 0; i < SECTOR_SIZE; i++) {
        if (read_buf[i] != write_buf[i]) {
            Serial.print("ERROR: Mismatch at offset ");
            Serial.print(i);
            Serial.print(": expected 0x");
            Serial.print(write_buf[i], HEX);
            Serial.print(", got 0x");
            Serial.println(read_buf[i], HEX);
            Serial.flush();
            return -1;
        }
    }
    
    Serial.println("PASS: Block alignment verified (512-byte sector)");
    Serial.flush();
    
    return 0;
}

static int test_lba_addressing(void) {
    uint8_t write_buf[SECTOR_SIZE];
    uint8_t read_buf[SECTOR_SIZE];
    uint32_t test_lbas[] = {1, 100, 1000, 10000};
    uint8_t test_count = sizeof(test_lbas) / sizeof(test_lbas[0]);
    
    Serial.println("\nTest 3: LBA addressing verification");
    Serial.flush();
    
    for (uint8_t i = 0; i < test_count; i++) {
        uint32_t lba = test_lbas[i];
        if (save_sector(lba) != 0) {
            Serial.print("WARNING: Failed to save original sector ");
            Serial.println(lba);
            Serial.flush();
        }
        
        for (uint16_t j = 0; j < SECTOR_SIZE; j++) {
            write_buf[j] = (uint8_t)((lba + j) & 0xFF);
        }
        
        Serial.print("Testing LBA ");
        Serial.print(lba);
        Serial.print("... ");
        Serial.flush();
        
        if (write_block(lba, write_buf) != 0) {
            Serial.println("ERROR: Write failed");
            Serial.flush();
            return -1;
        }
        
        delay(50);
        
        memset(read_buf, 0, SECTOR_SIZE);
        if (read_block(lba, read_buf) != 0) {
            Serial.println("ERROR: Read failed");
            Serial.flush();
            return -1;
        }
        
        int mismatch = 0;
        for (uint16_t j = 0; j < SECTOR_SIZE; j++) {
            if (read_buf[j] != write_buf[j]) {
                Serial.print("ERROR: Mismatch at offset ");
                Serial.println(j);
                Serial.flush();
                mismatch = 1;
                break;
            }
        }
        
        if (mismatch) {
            return -1;
        }
        
        Serial.println("PASS");
        Serial.flush();
    }
    
    Serial.println("PASS: LBA addressing verified");
    Serial.flush();
    
    return 0;
}

static int test_multiple_blocks(void) {
    uint8_t write_buf[SECTOR_SIZE * TEST_SECTOR_COUNT];
    uint8_t read_buf[SECTOR_SIZE * TEST_SECTOR_COUNT];
    uint32_t start_lba = 2000;
    
    Serial.println("\nTest 4: Multiple block read/write");
    Serial.flush();
    
    for (uint8_t i = 0; i < TEST_SECTOR_COUNT; i++) {
        uint32_t lba = start_lba + i;
        if (save_sector(lba) != 0) {
            Serial.print("WARNING: Failed to save original sector ");
            Serial.println(lba);
            Serial.flush();
        }
    }
    
    for (uint16_t i = 0; i < SECTOR_SIZE * TEST_SECTOR_COUNT; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    for (uint8_t i = 0; i < TEST_SECTOR_COUNT; i++) {
        uint32_t lba = start_lba + i;
        if (write_block(lba, write_buf + (i * SECTOR_SIZE)) != 0) {
            Serial.print("ERROR: Failed to write block at LBA ");
            Serial.println(lba);
            Serial.flush();
            return -1;
        }
    }
    
    delay(100);
    
    for (uint8_t i = 0; i < TEST_SECTOR_COUNT; i++) {
        uint32_t lba = start_lba + i;
        memset(read_buf + (i * SECTOR_SIZE), 0, SECTOR_SIZE);
        if (read_block(lba, read_buf + (i * SECTOR_SIZE)) != 0) {
            Serial.print("ERROR: Failed to read block at LBA ");
            Serial.println(lba);
            Serial.flush();
            return -1;
        }
    }
    
    for (uint16_t i = 0; i < SECTOR_SIZE * TEST_SECTOR_COUNT; i++) {
        if (read_buf[i] != write_buf[i]) {
            Serial.print("ERROR: Mismatch at offset ");
            Serial.println(i);
            Serial.flush();
            return -1;
        }
    }
    
    Serial.print("PASS: Multiple blocks verified (");
    Serial.print(TEST_SECTOR_COUNT);
    Serial.println(" sectors)");
    Serial.flush();
    
    return 0;
}

static int run_block_tests(void) {
    Serial.println("\nBlock-level SD Card Test");
    Serial.flush();
    
    if (!sd.card()) {
        Serial.println("ERROR: SD card not present");
        Serial.flush();
        return -1;
    }
    
    uint32_t card_size = sd.card()->sectorCount();
    Serial.print("Card sector count: ");
    Serial.println(card_size);
    Serial.print("Card size: ");
    Serial.print((uint64_t)card_size * 512 / (1024 * 1024));
    Serial.println(" MB");
    Serial.flush();
    
    int result = 0;
    
    result = test_mbr_bpb();
    if (result != 0) return result;
    delay(200);
    
    result = test_block_alignment();
    if (result != 0) return result;
    delay(200);
    
    result = test_lba_addressing();
    if (result != 0) return result;
    delay(200);
    
    result = test_multiple_blocks();
    if (result != 0) return result;
    
    Serial.println("\nAll Block Tests Passed");
    Serial.flush();
    
    return 0;
}

void setup(void) {
    Serial.begin(115200);
    delay_ms(1000);
    
    boot_init();
    
    Serial.println("\nSD Card Block Test");
    Serial.println("Initializing SD card...");
    Serial.flush();
    
    switch_to_sd_spi();
    
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(200);
    
    SdSpiConfig sd_config(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(25));
    if (!sd.begin(sd_config)) {
        Serial.println("ERROR: SD card initialization failed");
        Serial.flush();
        return;
    }
    
    Serial.println("SD card initialized successfully");
    Serial.flush();
    delay(500);
    
    int test_result = run_block_tests();
    
    restore_sectors();
    
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

