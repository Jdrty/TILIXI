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
        
        // initialize actions (after boot sequence)
        init_actions();
        
        // register hotkeys
        register_key(mod_shift, key_a, "terminal");
        register_key(mod_shift, key_d, "close_terminal");
        // add more hotkeys as needed
    }
    
    void loop(void) {
        // run scheduler tick
        scheduler_tick();
        
        // small delay to prevent CPU spinning
        delay_ms(10);
    }
    
#else
    // this file should only be compiled for ESP32 builds!!
    #error "main_esp32.c should only be used for ESP32 builds >:[ "
#endif

