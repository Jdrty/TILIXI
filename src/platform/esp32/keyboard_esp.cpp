#include <Arduino.h>
#include "ino_helper.h"
#include "keyboard_core.h"
#include "event_processor.h"

// TODO: configure keyboard input pins here

#include "platform/esp32/keyboard_esp.h"

static key_event last_key_event = {key_none, 0};

// init ESP32 keyboard
void keyboard_esp_init(void) {
    // TODO: initialize GPIO pins, USB, I2C, etc.
    ESP_INFO("initializing ESP32 keyboard...");
    
    // TODO: set up keyboard hardware:
    
    ESP_INFO("ESP32 keyboard initialized! :D");
}

// this scans the keyboard and processes events
void keyboard_esp_scan(void) {
    // TODO: add keyboard scanning logic
    // placeholder
    key_event evt = {key_none, 0};
    
    // process the event if valid
    if (evt.key != key_none && (evt.key != last_key_event.key || evt.modifiers != last_key_event.modifiers)) {
        process(evt);
        last_key_event = evt;
    }
}

