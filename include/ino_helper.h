#pragma once

// Arduino/ESP32 compatibility helper
// this file provides Arduino-style functions and ESP32-specific helpers
// for easier porting between PC and hardware platforms

#ifdef ARDUINO
    // Arduino/ESP32 includes
    // Arduino.h provides Serial, no need for HardwareSerial.h
    #include <Arduino.h>
    
    // uses Serial for debug output (only if not already defined by debug_helper.h)
    #if !defined(DEBUG_PRINT) && !defined(DEBUG)
        #define DEBUG_PRINT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
    #endif
    
    // timing functions
    #define delay_ms(ms) delay(ms)
    #define get_time_ms() millis()
    
    // pin definitions (can be overridden)
    #ifndef LED_BUILTIN
        #define LED_BUILTIN 2
    #endif
    
#else   // simply to run code with hardware dependancies without hardware
    // PC/Linux compatibility stubs
    #include <stdio.h>
    #include <stdint.h>
    #include <stdarg.h>
    #include <unistd.h>
    #include <time.h>
    #include <sys/time.h>
    
    // debug output to stdout
    #ifndef DEBUG_PRINT
        #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
    
    // timing stubs
    #define delay_ms(ms) usleep((ms) * 1000)
    
    static inline uint32_t get_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
    
    // pin definitions (no-op on PC)
    #define LED_BUILTIN 0
    #define digitalWrite(pin, val) ((void)0)
    #define digitalRead(pin) (0)
    #define pinMode(pin, mode) ((void)0)
    
    // serial stubs for PC
    static inline void serial_begin(uint32_t baud) { (void)baud; }
    static inline void serial_print(const char* str) { printf("%s", str); }
    static inline void serial_println(const char* str) { printf("%s\n", str); }
    static inline void serial_printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    
    // dummy Serial object for PC
    #define Serial (*(struct { \
        void (*begin)(uint32_t); \
        void (*print)(const char*); \
        void (*println)(const char*); \
        void (*printf)(const char*, ...); \
    }*)&(struct { \
        void (*begin)(uint32_t); \
        void (*print)(const char*); \
        void (*println)(const char*); \
        void (*printf)(const char*, ...); \
    }){serial_begin, serial_print, serial_println, serial_printf})
    
#endif

// ESP32-S3 specific helpers
#if (defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV)) && defined(ARDUINO)
    // ESP32 logging
    #ifndef ESP_LOG_TAG
        #define ESP_LOG_TAG "TILIXI"
    #endif
    #define ESP_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
    #define ESP_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define ESP_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
    // stubs for non-ESP32 builds
    #define ESP_DEBUG(fmt, ...) ((void)0)
    #define ESP_INFO(fmt, ...) ((void)0)
    #define ESP_ERROR(fmt, ...) ((void)0)
#endif

// platform detection macros
#ifndef ARDUINO
    #define PLATFORM_PC 1
#else
    #define PLATFORM_ESP32 1
#endif

