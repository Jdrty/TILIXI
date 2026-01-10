#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include "platform/pc/keyboard_pc.h"
#include "event_processor.h"

// modifier masks (matching keyboard_core.h)
#define mod_ctrl  (1 << 1)
#define mod_shift (1 << 0)

// enable raw terminal input
static void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);  // disable line buffering and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// restore terminal to previous mode
static void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSANOW, orig);
}

// convert ASCII char to key_code
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

// "sdlread" replacement for terminal input
void sdlread(void) {
    struct termios orig;
    enable_raw_mode(&orig);

    fflush(stdout);

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n != 1) continue;

        key_event evt;
        evt.modifiers = 0;

        // handle control characters (Ctrl+key combinations)
        if ((unsigned char)c <= 26 && c != '\n' && c != '\r' && c != '\t' && (unsigned char)c != 8) {
            evt.modifiers |= mod_ctrl;
            c += 'a' - 1; // convert back to lowercase
        }

        // handle uppercase letters (shift detected)
        if (c >= 'A' && c <= 'Z') {
            evt.modifiers |= mod_shift;
            c = c - 'A' + 'a';  // convert to lowercase for key mapping
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

        DEBUG_PRINT("[DEBUG] Key pressed: key=%d, mods=%d\n", evt.key, evt.modifiers);
        fflush(stdout);

        process(evt);
    }

    disable_raw_mode(&orig);
}
