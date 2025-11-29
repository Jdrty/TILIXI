#include <Arduino.h>
#include "ino_helper.h"
#include "boot_splash.h"
#include "action_manager.h"
#include "event_processor.h"
#include "hotkey.h"
#include "platform/esp32/keyboard_esp.h"
#include "process.h"
#include "process_scheduler.h"
#include "process_script.h"

#ifdef PLATFORM_ESP32
    void setup(void) {
        // initialize serial for debug output
        Serial.begin(115200);
        delay_ms(1000);
        
        // initialize boot display (TFT) - show TILIXI text
        boot_init();
        
        // initialize process system
        init_process_system();
        init_scheduler();
        init_script_system();
        
        // initialize terminal system
        init_terminal_system();
        init_terminal_commands();
        
        // initialize actions
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

