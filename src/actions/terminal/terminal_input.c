// i know theres already a keyboard sketch which does the same thing
// but since this build only really uses the terminal for input its overkill
// ill migrate to the others if I add a desktop enviroment which would need it

#include "terminal.h"
#include "keyboard_core.h"
#include "debug_helper.h"
#include "firstboot.h"
#include "passwd.h"
#include "login.h"

extern int nano_is_active(void);
extern void nano_handle_key_event(key_event evt);

// convert key_code to character (for terminal input)
static char key_to_char(key_code key, uint8_t modifiers) {
    char base_char = 0;
    
    // map each key code individually to its character
    switch (key) {
        // letter keys - map each individually since they're not consecutive in enum
        case key_q: base_char = (modifiers & mod_shift) ? 'Q' : 'q'; break;
        case key_w: base_char = (modifiers & mod_shift) ? 'W' : 'w'; break;
        case key_e: base_char = (modifiers & mod_shift) ? 'E' : 'e'; break;
        case key_r: base_char = (modifiers & mod_shift) ? 'R' : 'r'; break;
        case key_tee: base_char = (modifiers & mod_shift) ? 'T' : 't'; break;
        case key_y: base_char = (modifiers & mod_shift) ? 'Y' : 'y'; break;
        case key_u: base_char = (modifiers & mod_shift) ? 'U' : 'u'; break;
        case key_i: base_char = (modifiers & mod_shift) ? 'I' : 'i'; break;
        case key_o: base_char = (modifiers & mod_shift) ? 'O' : 'o'; break;
        case key_p: base_char = (modifiers & mod_shift) ? 'P' : 'p'; break;
        case key_a: base_char = (modifiers & mod_shift) ? 'A' : 'a'; break;
        case key_s: base_char = (modifiers & mod_shift) ? 'S' : 's'; break;
        case key_d: base_char = (modifiers & mod_shift) ? 'D' : 'd'; break;
        case key_f: base_char = (modifiers & mod_shift) ? 'F' : 'f'; break;
        case key_g: base_char = (modifiers & mod_shift) ? 'G' : 'g'; break;
        case key_h: base_char = (modifiers & mod_shift) ? 'H' : 'h'; break;
        case key_j: base_char = (modifiers & mod_shift) ? 'J' : 'j'; break;
        case key_k: base_char = (modifiers & mod_shift) ? 'K' : 'k'; break;
        case key_l: base_char = (modifiers & mod_shift) ? 'L' : 'l'; break;
        case key_z: base_char = (modifiers & mod_shift) ? 'Z' : 'z'; break;
        case key_x: base_char = (modifiers & mod_shift) ? 'X' : 'x'; break;
        case key_c: base_char = (modifiers & mod_shift) ? 'C' : 'c'; break;
        case key_v: base_char = (modifiers & mod_shift) ? 'V' : 'v'; break;
        case key_b: base_char = (modifiers & mod_shift) ? 'B' : 'b'; break;
        case key_n: base_char = (modifiers & mod_shift) ? 'N' : 'n'; break;
        case key_m: base_char = (modifiers & mod_shift) ? 'M' : 'm'; break;
        
        // number keys with shift modifiers
        case key_one: base_char = (modifiers & mod_shift) ? '!' : '1'; break;
        case key_two: base_char = (modifiers & mod_shift) ? '@' : '2'; break;
        case key_three: base_char = (modifiers & mod_shift) ? '#' : '3'; break;
        case key_four: base_char = (modifiers & mod_shift) ? '$' : '4'; break;
        case key_five: base_char = (modifiers & mod_shift) ? '%' : '5'; break;
        case key_six: base_char = (modifiers & mod_shift) ? '^' : '6'; break;
        case key_seven: base_char = (modifiers & mod_shift) ? '&' : '7'; break;
        case key_eight: base_char = (modifiers & mod_shift) ? '*' : '8'; break;
        case key_nine: base_char = (modifiers & mod_shift) ? '(' : '9'; break;
        case key_zero: base_char = (modifiers & mod_shift) ? ')' : '0'; break;
        
        // special character keys
        case key_space: base_char = ' '; break;
        case key_dash: base_char = (modifiers & mod_shift) ? '_' : '-'; break;
        case key_equals: base_char = (modifiers & mod_shift) ? '+' : '='; break;
        case key_openbracket: base_char = (modifiers & mod_shift) ? '{' : '['; break;
        case key_closebracket: base_char = (modifiers & mod_shift) ? '}' : ']'; break;
        case key_backslash: base_char = (modifiers & mod_shift) ? '|' : '\\'; break;
        case key_colon: base_char = (modifiers & mod_shift) ? ':' : ';'; break;
        case key_quote: base_char = (modifiers & mod_shift) ? '"' : '\''; break;
        case key_comma: base_char = (modifiers & mod_shift) ? '<' : ','; break;
        case key_period: base_char = (modifiers & mod_shift) ? '>' : '.'; break;
        case key_slash: base_char = (modifiers & mod_shift) ? '?' : '/'; break;
        case key_tilde: base_char = (modifiers & mod_shift) ? '~' : '`'; break;
        
        // keys that should not be printed (serial-significant keys)
        case key_enter:
        case key_backspace:
        case key_tab:
        case key_esc:
        case key_left:
        case key_right:
        case key_up:
        case key_down:
        case key_capslock:
        case key_none:
        default: base_char = 0; break;
    }
    
    return base_char;
}

// handle keyboard event for terminal input
void terminal_handle_key_event(key_event evt) {
    terminal_state *term = get_active_terminal();
    
#ifdef ARDUINO
    extern void Serial_printf(const char *fmt, ...);
    // use Serial.printf via extern or printf wrapper
    // for now, debug will be minimal since we can't easily use Serial in C
#endif
    
    if (term == NULL) {
#ifndef ARDUINO
        DEBUG_PRINT("[TERMINAL_INPUT] terminal_handle_key_event: no active terminal\n");
#endif
        return;
    }
    if (!term->active) {
#ifndef ARDUINO
        DEBUG_PRINT("[TERMINAL_INPUT] terminal_handle_key_event: terminal not active\n");
#endif
        return;
    }
    if (term->image_view_active) {
        return;
    }
    
    if (login_is_active()) {
        login_handle_key_event(evt);
        return;
    }

    if (firstboot_is_active()) {
        firstboot_handle_key_event(evt);
        return;
    }

    if (passwd_is_active()) {
        passwd_handle_key_event(evt);
        return;
    }
    
    if (nano_is_active()) {
        nano_handle_key_event(evt);
        return;
    }
    
    // handle special keys
    switch (evt.key) {
        case key_enter:
            terminal_handle_enter(term);
            break;
        case key_backspace:
            terminal_handle_backspace(term);
            break;
        case key_up:
            terminal_handle_arrow_up(term);
            break;
        case key_down:
            terminal_handle_arrow_down(term);
            break;
        case key_left:
            terminal_handle_arrow_left(term);
            break;
        case key_right:
            terminal_handle_arrow_right(term);
            break;
        case key_tab:
            // tab completion - could be implemented later
            break;
        default: {
            // regular character input
            char c = key_to_char(evt.key, evt.modifiers);
#ifndef ARDUINO
            DEBUG_PRINT("[TERMINAL_INPUT] key_to_char: key=%d, mods=0x%02x, char='%c' (0x%02x)\n",
                        evt.key, evt.modifiers, c ? c : '?', (unsigned char)c);
#endif
            if (c != 0) {
#ifndef ARDUINO
                DEBUG_PRINT("[TERMINAL_INPUT] calling terminal_handle_key with char='%c'\n", c);
#endif
                terminal_handle_key(term, c);
            } else {
#ifndef ARDUINO
                DEBUG_PRINT("[TERMINAL_INPUT] key_to_char returned 0, ignoring\n");
#endif
            }
            break;
        }
    }
}

