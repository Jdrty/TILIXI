#include <Arduino.h>
#include "ino_helper.h"
#include "keyboard_core.h"
#include "event_processor.h"
#include "terminal.h"

// TODO: configure keyboard input pins here

#include "platform/esp32/keyboard_esp.h"

static key_event last_key_event = {key_none, 0};

#ifdef KEYBOARD_SERIAL_DEBUG
#define KEYBOARD_DBG(...) Serial.printf(__VA_ARGS__)
#else
#define KEYBOARD_DBG(...) do { } while (0)
#endif

// convert ASCII char to key_code (similar to PC version)
static key_code key_from_char(char c) {
    // map lowercase letters to their corresponding key codes (QWERTY layout order)
    switch (c) {
        case 'a': return key_a;
        case 'b': return key_b;
        case 'c': return key_c;
        case 'd': return key_d;
        case 'e': return key_e;
        case 'f': return key_f;
        case 'g': return key_g;
        case 'h': return key_h;
        case 'i': return key_i;
        case 'j': return key_j;
        case 'k': return key_k;
        case 'l': return key_l;
        case 'm': return key_m;
        case 'n': return key_n;
        case 'o': return key_o;
        case 'p': return key_p;
        case 'q': return key_q;
        case 'r': return key_r;
        case 's': return key_s;
        case 't': return key_tee;
        case 'u': return key_u;
        case 'v': return key_v;
        case 'w': return key_w;
        case 'x': return key_x;
        case 'y': return key_y;
        case 'z': return key_z;
        
        // uppercase letters (shift handled separately, but map to same key)
        case 'A': return key_a;
        case 'B': return key_b;
        case 'C': return key_c;
        case 'D': return key_d;
        case 'E': return key_e;
        case 'F': return key_f;
        case 'G': return key_g;
        case 'H': return key_h;
        case 'I': return key_i;
        case 'J': return key_j;
        case 'K': return key_k;
        case 'L': return key_l;
        case 'M': return key_m;
        case 'N': return key_n;
        case 'O': return key_o;
        case 'P': return key_p;
        case 'Q': return key_q;
        case 'R': return key_r;
        case 'S': return key_s;
        case 'T': return key_tee;
        case 'U': return key_u;
        case 'V': return key_v;
        case 'W': return key_w;
        case 'X': return key_x;
        case 'Y': return key_y;
        case 'Z': return key_z;
        
        // number keys
        case '1': return key_one;
        case '2': return key_two;
        case '3': return key_three;
        case '4': return key_four;
        case '5': return key_five;
        case '6': return key_six;
        case '7': return key_seven;
        case '8': return key_eight;
        case '9': return key_nine;
        case '0': return key_zero;
        
        // special characters
        case ' ': return key_space;
        case '\n': return key_enter;
        case '\r': return key_enter;
        case '\t': return key_tab;
        case '\b': return key_backspace;
        case 127: return key_backspace;  // DEL key
        case 27: return key_esc;  // ESC key
        case '-': return key_dash;
        case '_': return key_dash;  // shift+dash
        case '=': return key_equals;
        case '+': return key_equals;  // shift+equals
        case '[': return key_openbracket;
        case '{': return key_openbracket;  // shift+openbracket
        case ']': return key_closebracket;
        case '}': return key_closebracket;  // shift+closebracket
        case '\\': return key_backslash;
        case '|': return key_backslash;  // shift+backslash
        case ';': return key_colon;
        case ':': return key_colon;  // shift+colon
        case '\'': return key_quote;
        case '"': return key_quote;  // shift+quote
        case ',': return key_comma;
        case '<': return key_comma;  // shift+comma
        case '.': return key_period;
        case '>': return key_period;  // shift+period
        case '/': return key_slash;
        case '?': return key_slash;  // shift+slash
        case '`': return key_tilde;
        case '~': return key_tilde;  // shift+tilde
        
        default: return key_none;
    }
}

// init ESP32 keyboard
void keyboard_esp_init(void) {
    // TODO: initialize GPIO pins, USB, I2C, etc.
    ESP_INFO("initializing ESP32 keyboard...");
    
    // serial is already initialized in setup(), but ensure it's ready for input
    // serial input will be read in keyboard_esp_scan()
    
    ESP_INFO("ESP32 keyboard initialized! :D");
    ESP_INFO("Serial input enabled - you can type in the serial monitor!");
}

static void keyboard_esp_handle_char(char c, uint8_t *escape_state,
                                     uint8_t *csi_mod, bool *needs_render) {
    // handle ANSI escape sequences (arrow keys) without printing escape bytes
    if (*escape_state == 1) {
        if (c == '[' || c == 'O') {
            *escape_state = 2;
            *csi_mod = 0;
        } else {
            *escape_state = 0;
        }
        return;
    }
    if (*escape_state == 2) {
        if ((c >= '0' && c <= '9') || c == ';') {
            *escape_state = 3;
            if (c == '5') {
                *csi_mod = mod_ctrl;
            }
            return;
        }
        *escape_state = 0;
        key_event evt;
        evt.modifiers = *csi_mod;
        evt.key = key_none;
        switch (c) {
            case 'A': evt.key = key_up; break;
            case 'B': evt.key = key_down; break;
            case 'C': evt.key = key_right; break;
            case 'D': evt.key = key_left; break;
            default: evt.key = key_none; break;
        }
        if (evt.key != key_none) {
            KEYBOARD_DBG("[KEYBOARD] arrow key: %c\n", c);
            process(evt);
            *needs_render = true;
            last_key_event = evt;
        }
        return;
    }
    if (*escape_state == 3) {
        if (c >= '0' && c <= '9') {
            if (c == '5') {
                *csi_mod = mod_ctrl;
            }
            return;
        }
        if (c == ';') {
            return;
        }
        *escape_state = 0;
        key_event evt;
        evt.modifiers = *csi_mod;
        evt.key = key_none;
        switch (c) {
            case 'A': evt.key = key_up; break;
            case 'B': evt.key = key_down; break;
            case 'C': evt.key = key_right; break;
            case 'D': evt.key = key_left; break;
            default: evt.key = key_none; break;
        }
        if (evt.key != key_none) {
            KEYBOARD_DBG("[KEYBOARD] arrow key (CSI): %c\n", c);
            process(evt);
            *needs_render = true;
            last_key_event = evt;
        }
        return;
    }
    if ((unsigned char)c == 27) {
        *escape_state = 1;
        return;
    }
    
    // debug: print raw character received
    KEYBOARD_DBG("[KEYBOARD] raw char: '%c' (ascii: %d, hex: 0x%02x)\n", 
                 (c >= 32 && c < 127) ? c : '?', (int)(unsigned char)c, (unsigned char)c);
    
    // handle CRLF sequence - only process CR (\r), ignore LF (\n)
    if (c == '\n') {
        KEYBOARD_DBG("[KEYBOARD] ignoring LF (0x0a) - only processing CR (0x0d) for Enter\n");
        return;  // ignore LF completely, exit immediately
    }
    
    key_event evt;
    evt.modifiers = 0;
    evt.key = key_none;
    
    // handle backspace and delete keys first (before control character check)
    if ((unsigned char)c == 8 || (unsigned char)c == 127) {
        evt.key = key_backspace;
        KEYBOARD_DBG("[KEYBOARD] detected backspace key (ascii=%d)\n", (unsigned char)c);
        process(evt);
        *needs_render = true;
        last_key_event = evt;
        return;
    }
    
    // handle control characters (Ctrl+key combinations)
    if ((unsigned char)c <= 26 && c != '\n' && c != '\r' && (unsigned char)c != 8 && c != '\t') {
        evt.modifiers |= mod_ctrl;
        c += 'a' - 1; // convert back to lowercase
        KEYBOARD_DBG("[KEYBOARD] detected ctrl character\n");
    }
    
    // handle shift modifier (uppercase letters)
    if (c >= 'A' && c <= 'Z') {
        evt.modifiers |= mod_shift;
        c = c - 'A' + 'a';  // convert to lowercase
        KEYBOARD_DBG("[KEYBOARD] detected shift (uppercase)\n");
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
    
    // debug: print converted key event
    KEYBOARD_DBG("[KEYBOARD] converted: key_code=%d, modifiers=0x%02x, char='%c'\n", 
                 evt.key, evt.modifiers, c);
    
    if (evt.key != key_none) {
        KEYBOARD_DBG("[KEYBOARD] processing event...\n");
        process(evt);
        *needs_render = true;
        last_key_event = evt;
    } else {
        KEYBOARD_DBG("[KEYBOARD] key is key_none, ignoring\n");
    }
}

// this scans the keyboard and processes events
// reads from Serial (serial monitor) and converts to key events
void keyboard_esp_scan(void) {
    static uint8_t escape_state = 0; // 0=none, 1=got ESC, 2=got ESC+[ or ESC+O, 3=CSI params
    static uint8_t csi_mod = 0;
    bool needs_render = false;
    
    // read and process all available bytes to avoid 1-byte-per-scan bottleneck
    while (Serial.available() > 0) {
        char c = Serial.read();
        keyboard_esp_handle_char(c, &escape_state, &csi_mod, &needs_render);
    }

    if (needs_render) {
        terminal_state *term = get_active_terminal();
        if (term != NULL && term->active) {
            KEYBOARD_DBG("[KEYBOARD] rendering terminal window...\n");
            terminal_render_window(term);
        } else {
            KEYBOARD_DBG("[KEYBOARD] terminal not active, skipping render (term=%p, active=%d)\n",
                         term, term ? term->active : 0);
        }
    }
}

