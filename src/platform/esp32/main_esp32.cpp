#include <Arduino.h>
#include "ino_helper.h"
#include "boot_splash.h"
#include "boot_sequence.h"
#include "action_manager.h"
#include "event_processor.h"
#include "hotkey.h"
#include "platform/esp32/keyboard_esp.h"
#include "process.h"
#include "process_scheduler.h"
#include "process_script.h"
#include "terminal.h"
#include "terminal_cmd.h"

#ifdef PLATFORM_ESP32
    void setup(void) {
        // initialize serial for debug output
        Serial.begin(115200);
        delay_ms(1000);
        
        // initialize boot display (TFT) - show TILIXI text
        boot_init();
        
        // run boot sequence (handles all initialization)
        boot_sequence_run();
        
        // initialize keyboard (for serial input)
        keyboard_esp_init();
        
        // initialize actions (after boot sequence)
        init_actions();
        
        // register hotkeys
        register_key(mod_shift, key_a, "terminal");
        register_key(mod_shift, key_d, "close_terminal");
        // add more hotkeys as needed
    }
    
    void loop(void) {
        // freeRTOS handles preemptive scheduling automatically via interrupts
        // scheduler_tick() is a no-op on ESP32, but kept for API compatibility
        // this loop() runs in the default FreeRTOS task
        // other processes run as separate FreeRTOS tasks with preemptive scheduling
        // i'm so goated
        
        // scan for keyboard input from serial monitor
        keyboard_esp_scan();
        
        // small delay to prevent CPU spinning in the main loop
        // note: This doesn't affect process scheduling, FreeRTOS handles that
        delay_ms(10);
    }
    
#else
    // this file should only be compiled for ESP32 builds!!
    #error "main_esp32.c should only be used for ESP32 builds >:[ "
#endif

