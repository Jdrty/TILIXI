#include "terminal.h"
#include "keyboard_core.h"
#include "debug_helper.h"

// convert key_code to character (for terminal input)
static char key_to_char(key_code key, uint8_t modifiers) {
    char base_char = 0;
    
    // map key codes to characters
    if (key >= key_a && key <= key_z) {
        base_char = 'a' + (key - key_a);
        if (modifiers & mod_shift) {
            base_char = 'A' + (key - key_a);
        }
    } else if (key >= key_one && key <= key_zero) {
        const char *numbers = "1234567890";
        base_char = numbers[key - key_one];
        if (modifiers & mod_shift) {
            const char *shifted = "!@#$%^&*()";
            base_char = shifted[key - key_one];
        }
    } else {
        switch (key) {
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
            default: base_char = 0; break;
        }
    }
    
    return base_char;
}

// handle keyboard event for terminal input
void terminal_handle_key_event(key_event evt) {
    terminal_state *term = get_active_terminal();
    if (term == NULL || !term->active) {
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
        case key_right:
            // cursor movement - could be implemented later
            break;
        case key_tab:
            // tab completion - could be implemented later
            break;
        default: {
            // regular character input
            char c = key_to_char(evt.key, evt.modifiers);
            if (c != 0) {
                terminal_handle_key(term, c);
            }
            break;
        }
    }
}

