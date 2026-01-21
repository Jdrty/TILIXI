#include "login.h"
#include "terminal.h"
#include "vfs.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOGIN_MAX_INPUT 64

typedef struct {
    int active;
    terminal_state *term;
    char username[LOGIN_MAX_INPUT];
    uint32_t hash;
    char input[LOGIN_MAX_INPUT];
    size_t input_len;
} login_state_t;

static login_state_t login_state = {0};

static uint32_t simple_hash(const char *data) {
    uint32_t hash = 2166136261u;
    while (*data) {
        hash ^= (uint8_t)(*data++);
        hash *= 16777619u;
    }
    return hash;
}

static int read_passwd_entry(char *out_user, size_t out_size, uint32_t *out_hash) {
    vfs_file_t *file = vfs_open("/etc/passwd", VFS_O_READ);
    if (file == NULL) {
        return 0;
    }
    char buf[128];
    ssize_t read_bytes = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (read_bytes <= 0) {
        return 0;
    }
    buf[read_bytes] = '\0';
    char *newline = strchr(buf, '\n');
    if (newline) {
        *newline = '\0';
    }
    char *colon = strchr(buf, ':');
    if (colon == NULL) {
        return 0;
    }
    *colon = '\0';
    size_t copy_len = strnlen(buf, out_size - 1);
    if (copy_len == 0) {
        return 0;
    }
    memcpy(out_user, buf, copy_len);
    out_user[copy_len] = '\0';
    const char *hash_str = colon + 1;
    if (hash_str[0] == '\0') {
        return 0;
    }
    *out_hash = (uint32_t)strtoul(hash_str, NULL, 16);
    return 1;
}

static void login_clear_input(void) {
    login_state.input_len = 0;
    login_state.input[0] = '\0';
}

static void login_show_screen(void) {
    terminal_state *term = login_state.term;
    terminal_clear(term);
    terminal_write_string(term, "Username: ");
    terminal_write_line(term, login_state.username);
    terminal_write_string(term, "password: ");
    login_clear_input();
#ifdef ARDUINO
    extern void terminal_render_window(terminal_state *term);
    terminal_render_window(term);
#endif
}

static void login_finish(void) {
    terminal_state *term = login_state.term;
    login_state.active = 0;
    login_state.term = NULL;
    terminal_clear(term);
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    term->input_len = 0;
    terminal_write_string(term, "$ ");
    term->cursor_col = 2;
}

int login_is_active(void) {
    return login_state.active;
}

static char login_key_to_char(key_code key, uint8_t modifiers) {
    char base_char = 0;
    switch (key) {
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
    return base_char;
}

void login_handle_key_event(key_event evt) {
    if (!login_state.active || login_state.term == NULL) {
        return;
    }
    terminal_state *term = login_state.term;

    if (evt.key == key_backspace) {
        if (login_state.input_len > 0 && term->cursor_col > 0) {
            login_state.input_len--;
            login_state.input[login_state.input_len] = '\0';
            uint16_t pos = term->cursor_row * terminal_cols + (term->cursor_col - 1);
            if (pos < terminal_buffer_size) {
                term->buffer[pos] = ' ';
            }
            term->cursor_col--;
        }
        return;
    }

    if (evt.key == key_enter) {
        terminal_newline(term);
        uint32_t hash = simple_hash(login_state.input);
        if (hash != login_state.hash) {
            terminal_write_line(term, "Incorrect password. Try again.");
            terminal_write_string(term, "password: ");
            login_clear_input();
            return;
        }
        login_finish();
        return;
    }

    if (evt.key == key_tab || evt.key == key_esc) {
        return;
    }

    char c = login_key_to_char(evt.key, evt.modifiers);
    if (c == 0) {
        return;
    }
    if (login_state.input_len + 1 >= LOGIN_MAX_INPUT) {
        return;
    }
    login_state.input[login_state.input_len++] = c;
    login_state.input[login_state.input_len] = '\0';
    terminal_write_char(term, '*');
}

void login_begin_if_needed(terminal_state *term) {
    if (term == NULL) {
        return;
    }
    char username[LOGIN_MAX_INPUT];
    uint32_t hash = 0;
    if (!read_passwd_entry(username, sizeof(username), &hash)) {
        return;
    }
    memset(&login_state, 0, sizeof(login_state));
    login_state.active = 1;
    login_state.term = term;
    login_state.hash = hash;
    size_t copy_len = strnlen(username, sizeof(login_state.username) - 1);
    memcpy(login_state.username, username, copy_len);
    login_state.username[copy_len] = '\0';
    login_show_screen();
}

