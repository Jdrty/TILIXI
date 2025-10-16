#include "keymap_pc.h"

key_code keymap_from_raw(int raw) {
    switch (raw) {
        case SDLK_a: return key_a;
        case SDLK_b: return key_b;
        case SDLK_c: return key_c;
        case SDLK_d: return key_d;
        case SDLK_e: return key_e;
        case SDLK_f: return key_f;
        case SDLK_g: return key_g;
        case SDLK_h: return key_h;
        case SDLK_i: return key_i;
        case SDLK_j: return key_j;
        case SDLK_k: return key_k;
        case SDLK_l: return key_l;
        case SDLK_m: return key_m;
        case SDLK_n: return key_n;
        case SDLK_o: return key_o;
        case SDLK_p: return key_p;
        case SDLK_q: return key_q;
        case SDLK_r: return key_r;
        case SDLK_s: return key_s;
        case SDLK_t: return key_tee;
        case SDLK_u: return key_u;
        case SDLK_v: return key_v;
        case SDLK_w: return key_w;
        case SDLK_x: return key_x;
        case SDLK_y: return key_y;
        case SDLK_z: return key_z;

        case SDLK_1: return key_one;
        case SDLK_2: return key_two;
        case SDLK_3: return key_three;
        case SDLK_4: return key_four;
        case SDLK_5: return key_five;
        case SDLK_6: return key_six;
        case SDLK_7: return key_seven;
        case SDLK_8: return key_eight;
        case SDLK_9: return key_nine;
        case SDLK_0: return key_zero;

        case SDLK_SPACE: return key_space;
        case SDLK_RETURN: return key_enter;
        case SDLK_BACKSPACE: return key_backspace;
        case SDLK_TAB: return key_tab;
        case SDLK_ESCAPE: return key_esc;

        case SDLK_LEFT: return key_left;
        case SDLK_RIGHT: return key_right;
        case SDLK_UP: return key_up;
        case SDLK_DOWN: return key_down;

        case SDLK_MINUS: return key_dash;
        case SDLK_EQUALS: return key_equals;
        case SDLK_LEFTBRACKET: return key_openbracket;
        case SDLK_RIGHTBRACKET: return key_closebracket;
        case SDLK_BACKSLASH: return key_backslash;
        case SDLK_SEMICOLON: return key_colon;
        case SDLK_QUOTE: return key_quote;
        case SDLK_COMMA: return key_comma;
        case SDLK_PERIOD: return key_period;
        case SDLK_SLASH: return key_slash;
        case SDLK_BACKQUOTE: return key_tilde;

        default: return key_none; // unknown keys
    }
}
