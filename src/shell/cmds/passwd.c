#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "passwd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PASSWD_MAX_INPUT 64

typedef enum {
    PASSWD_STEP_CURRENT = 0,
    PASSWD_STEP_NEW,
    PASSWD_STEP_CONFIRM
} passwd_step_t;

typedef struct {
    int active;
    terminal_state *term;
    passwd_step_t step;
    char input[PASSWD_MAX_INPUT];
    size_t input_len;
    char username[PASSWD_MAX_INPUT];
    uint32_t current_hash;
    char new_password[PASSWD_MAX_INPUT];
} passwd_state_t;

static passwd_state_t passwd_state = {0};

int cmd_passwd(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_passwd_def = {
    .name = "passwd",
    .handler = cmd_passwd,
    .help = "Change password"
};

static uint32_t simple_hash(const char *data) {
    uint32_t hash = 2166136261u;
    while (*data) {
        hash ^= (uint8_t)(*data++);
        hash *= 16777619u;
    }
    return hash;
}

static void passwd_clear_input(void) {
    passwd_state.input_len = 0;
    passwd_state.input[0] = '\0';
}

static void passwd_prompt(const char *text) {
    terminal_state *term = passwd_state.term;
    terminal_write_string(term, text);
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
    strncpy(out_user, buf, out_size - 1);
    out_user[out_size - 1] = '\0';
    const char *hash_str = colon + 1;
    if (strlen(hash_str) == 0) {
        return 0;
    }
    *out_hash = (uint32_t)strtoul(hash_str, NULL, 16);
    return 1;
}

static int write_passwd_entry(const char *user, const char *password) {
    char line[128];
    uint32_t hash = simple_hash(password);
    snprintf(line, sizeof(line), "%s:%08lx\n", user, (unsigned long)hash);
    vfs_file_t *file = vfs_open("/etc/passwd",
                                VFS_O_WRITE | VFS_O_TRUNC | VFS_O_CREATE);
    if (file == NULL) {
        return 0;
    }
    size_t len = strlen(line);
    ssize_t written = vfs_write(file, line, len);
    vfs_close(file);
    return (written == (ssize_t)len);
}

static void passwd_finish(void) {
    terminal_state *term = passwd_state.term;
    passwd_state.active = 0;
    passwd_state.term = NULL;
    terminal_newline(term);
    terminal_write_line(term, "Password updated.");
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    term->input_len = 0;
    terminal_write_string(term, "$ ");
    term->cursor_col = 2;
}

static void passwd_fail(const char *message) {
    terminal_state *term = passwd_state.term;
    passwd_state.active = 0;
    passwd_state.term = NULL;
    terminal_newline(term);
    terminal_write_line(term, message);
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    term->input_len = 0;
    terminal_write_string(term, "$ ");
    term->cursor_col = 2;
}

int passwd_is_active(void) {
    return passwd_state.active;
}

static char passwd_key_to_char(key_code key, uint8_t modifiers) {
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

void passwd_handle_key_event(key_event evt) {
    if (!passwd_state.active || passwd_state.term == NULL) {
        return;
    }
    
    if (evt.key == key_backspace) {
        if (passwd_state.input_len > 0) {
            passwd_state.input_len--;
            passwd_state.input[passwd_state.input_len] = '\0';
            terminal_state *term = passwd_state.term;
            if (term->cursor_col > 0) {
                uint16_t pos = term->cursor_row * terminal_cols + (term->cursor_col - 1);
                if (pos < terminal_buffer_size) {
                    term->buffer[pos] = ' ';
                }
                term->cursor_col--;
            }
        }
        return;
    }
    
    if (evt.key == key_enter) {
        terminal_state *term = passwd_state.term;
        terminal_newline(term);
        if (passwd_state.step == PASSWD_STEP_CURRENT) {
            uint32_t hash = simple_hash(passwd_state.input);
            if (hash != passwd_state.current_hash) {
                passwd_clear_input();
                passwd_prompt("Current password incorrect. Try again: ");
                return;
            }
            passwd_state.step = PASSWD_STEP_NEW;
            passwd_clear_input();
            passwd_prompt("Enter new password: ");
            return;
        }
        if (passwd_state.step == PASSWD_STEP_NEW) {
            if (passwd_state.input_len == 0) {
                passwd_prompt("Password cannot be empty. Enter new password: ");
                return;
            }
            strncpy(passwd_state.new_password, passwd_state.input, PASSWD_MAX_INPUT - 1);
            passwd_state.new_password[PASSWD_MAX_INPUT - 1] = '\0';
            passwd_state.step = PASSWD_STEP_CONFIRM;
            passwd_clear_input();
            passwd_prompt("Confirm new password: ");
            return;
        }
        if (passwd_state.step == PASSWD_STEP_CONFIRM) {
            if (strcmp(passwd_state.new_password, passwd_state.input) != 0) {
                passwd_state.step = PASSWD_STEP_NEW;
                passwd_clear_input();
                passwd_prompt("Passwords do not match. Enter new password: ");
                return;
            }
            if (!write_passwd_entry(passwd_state.username, passwd_state.new_password)) {
                passwd_fail("Failed to update /etc/passwd.");
                return;
            }
            passwd_finish();
            return;
        }
        return;
    }
    
    if (evt.key == key_tab || evt.key == key_esc) {
        return;
    }
    
    char c = passwd_key_to_char(evt.key, evt.modifiers);
    if (c == 0) {
        return;
    }
    if (passwd_state.input_len + 1 >= PASSWD_MAX_INPUT) {
        return;
    }
    passwd_state.input[passwd_state.input_len++] = c;
    passwd_state.input[passwd_state.input_len] = '\0';
    terminal_write_char(passwd_state.term, '*');
}

int cmd_passwd(terminal_state *term, int argc, char **argv) {
    (void)argv;
    if (term == NULL) {
        return SHELL_ERR;
    }
    if (argc != 1) {
        shell_error(term, "passwd: too many arguments");
        return SHELL_EINVAL;
    }
    
    char username[PASSWD_MAX_INPUT];
    uint32_t hash = 0;
    if (!read_passwd_entry(username, sizeof(username), &hash)) {
        shell_error(term, "passwd: /etc/passwd not found or invalid");
        return SHELL_ENOENT;
    }
    
    memset(&passwd_state, 0, sizeof(passwd_state));
    passwd_state.active = 1;
    passwd_state.term = term;
    passwd_state.step = PASSWD_STEP_CURRENT;
    passwd_state.current_hash = hash;
    strncpy(passwd_state.username, username, sizeof(passwd_state.username) - 1);
    passwd_state.username[sizeof(passwd_state.username) - 1] = '\0';
    
    terminal_clear(term);
    terminal_write_line(term, "Change password");
    passwd_prompt("Current password: ");
    passwd_clear_input();
    return SHELL_OK;
}
