#include <Arduino.h>
#include "ino_helper.h"
#include "keyboard_core.h"
#include "event_processor.h"

// TODO: configure keyboard input pins here

#include "platform/esp32/keyboard_esp.h"

static key_event last_key_event = {key_none, 0};

// convert ASCII char to key_code (similar to PC version)
static key_code key_from_char(char c) {
    if (c >= 'a' && c <= 'z') return (key_code)(key_a + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return (key_code)(key_a + (c - 'A')); // shift handled separately
    if (c == '\n' || c == '\r') return key_enter;
    if (c == 27) return key_esc;
    if (c == ' ') return key_space;
    if (c == '\b' || c == 127) return key_backspace; // backspace
    if (c == '\t') return key_tab;
    
    // number keys
    if (c >= '1' && c <= '9') return (key_code)(key_one + (c - '1'));
    if (c == '0') return key_zero;
    
    // special characters
    if (c == '-') return key_dash;
    if (c == '=') return key_equals;
    if (c == '[') return key_openbracket;
    if (c == ']') return key_closebracket;
    if (c == '\\') return key_backslash;
    if (c == ';') return key_colon;
    if (c == '\'') return key_quote;
    if (c == ',') return key_comma;
    if (c == '.') return key_period;
    if (c == '/') return key_slash;
    if (c == '`') return key_tilde;
    
    return key_none;
}

// init ESP32 keyboard
void keyboard_esp_init(void) {
    // TODO: initialize GPIO pins, USB, I2C, etc.
    ESP_INFO("initializing ESP32 keyboard...");
    
    // Serial is already initialized in setup(), but ensure it's ready for input
    // Serial input will be read in keyboard_esp_scan()
    
    ESP_INFO("ESP32 keyboard initialized! :D");
    ESP_INFO("Serial input enabled - you can type in the serial monitor!");
}

// this scans the keyboard and processes events
// reads from Serial (serial monitor) and converts to key events
void keyboard_esp_scan(void) {
    // check if there's data available on Serial
    if (Serial.available() > 0) {
        char c = Serial.read();
        
        key_event evt;
        evt.modifiers = 0;
        
        // handle control characters (Ctrl+key combinations)
        // Control characters are sent as 1-26 for Ctrl+A through Ctrl+Z
        if ((unsigned char)c <= 26 && c != '\n' && c != '\r') {
            evt.modifiers |= mod_ctrl;
            c += 'a' - 1; // convert back to lowercase
        }
        
        // handle shift modifier (uppercase letters)
        if (c >= 'A' && c <= 'Z') {
            evt.modifiers |= mod_shift;
        }
        
        // handle shifted special characters
        if (c == '_') {
            evt.modifiers |= mod_shift;
            c = '-';
        } else if (c == '+') {
            evt.modifiers |= mod_shift;
            c = '=';
        } else if (c == '{') {
            evt.modifiers |= mod_shift;
            c = '[';
        } else if (c == '}') {
            evt.modifiers |= mod_shift;
            c = ']';
        } else if (c == '|') {
            evt.modifiers |= mod_shift;
            c = '\\';
        } else if (c == ':') {
            evt.modifiers |= mod_shift;
            c = ';';
        } else if (c == '"') {
            evt.modifiers |= mod_shift;
            c = '\'';
        } else if (c == '<') {
            evt.modifiers |= mod_shift;
            c = ',';
        } else if (c == '>') {
            evt.modifiers |= mod_shift;
            c = '.';
        } else if (c == '?') {
            evt.modifiers |= mod_shift;
            c = '/';
        } else if (c == '~') {
            evt.modifiers |= mod_shift;
            c = '`';
        } else if (c >= '!' && c <= ')') {
            // shifted numbers: !@#$%^&*()
            evt.modifiers |= mod_shift;
            const char shifted_chars[] = "!@#$%^&*()";
            const char normal_chars[] = "1234567890";
            for (int i = 0; i < 10; i++) {
                if (c == shifted_chars[i]) {
                    c = normal_chars[i];
                    break;
                }
            }
        }
        
        evt.key = key_from_char(c);
        
        // process the event if valid
        if (evt.key != key_none) {
            process(evt);
            last_key_event = evt;
        }
    }
}

