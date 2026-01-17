#include "firstboot.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_error.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define FIRSTBOOT_MAX_INPUT 32
#define FIRSTBOOT_MAX_PASS 64

typedef enum {
    FIRSTBOOT_USERNAME = 0,
    FIRSTBOOT_PASSWORD,
    FIRSTBOOT_PASSWORD_CONFIRM
} firstboot_step_t;

typedef struct {
    int active;
    terminal_state *term;
    firstboot_step_t step;
    char username[FIRSTBOOT_MAX_INPUT];
    char input[FIRSTBOOT_MAX_INPUT];
    size_t input_len;
    char password[FIRSTBOOT_MAX_PASS];
    char password_confirm[FIRSTBOOT_MAX_PASS];
    size_t password_len;
    size_t password_confirm_len;
    int prompt_row;
    int prompt_col;
} firstboot_state_t;

static firstboot_state_t firstboot_state = {0};

static void firstboot_clear_input(void) {
    firstboot_state.input_len = 0;
    firstboot_state.input[0] = '\0';
}

static void firstboot_set_prompt(const char *text) {
    terminal_state *term = firstboot_state.term;
    terminal_write_string(term, text);
    firstboot_state.prompt_row = term->cursor_row;
    firstboot_state.prompt_col = term->cursor_col;
}

static void firstboot_show_screen(void) {
    terminal_state *term = firstboot_state.term;
    terminal_clear(term);
    terminal_write_line(term, "First boot setup");
    if (firstboot_state.step == FIRSTBOOT_USERNAME) {
        firstboot_set_prompt("Enter username: ");
    } else if (firstboot_state.step == FIRSTBOOT_PASSWORD) {
        firstboot_set_prompt("Enter password: ");
    } else {
        firstboot_set_prompt("Confirm password: ");
    }
    firstboot_clear_input();
}

static int username_is_valid(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; name[i] != '\0'; i++) {
        char c = name[i];
        if (c == '/' || c == ' ' || c == '\t' || c == '.') {
            return 0;
        }
    }
    return 1;
}

static uint32_t simple_hash(const char *data) {
    uint32_t hash = 2166136261u;
    while (*data) {
        hash ^= (uint8_t)(*data++);
        hash *= 16777619u;
    }
    return hash;
}

static int write_passwd_file(const char *username, const char *password) {
    char line[128];
    uint32_t hash = simple_hash(password);
    snprintf(line, sizeof(line), "%s:%08lx\n", username, (unsigned long)hash);
    
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

static int rename_home_dir(const char *username) {
    vfs_node_t *home = vfs_resolve("/home");
    if (home == NULL || home->type != VFS_NODE_DIR) {
        if (home != NULL) {
            vfs_node_release(home);
        }
        return 0;
    }
    
    vfs_node_t *users = vfs_resolve_at(home, "users");
    if (users != NULL && users->type == VFS_NODE_DIR) {
        vfs_node_release(users);
        int res = vfs_dir_rename_node(home, "users", home, username);
        vfs_node_release(home);
        return res == VFS_EOK;
    }
    if (users != NULL) {
        vfs_node_release(users);
    }
    
    vfs_node_t *user = vfs_resolve_at(home, "user");
    if (user != NULL && user->type == VFS_NODE_DIR) {
        vfs_node_release(user);
        int res = vfs_dir_rename_node(home, "user", home, username);
        vfs_node_release(home);
        return res == VFS_EOK;
    }
    if (user != NULL) {
        vfs_node_release(user);
    }
    
    vfs_node_t *created = vfs_dir_create_node(home, username, VFS_NODE_DIR);
    if (created != NULL) {
        vfs_node_release(created);
        vfs_node_release(home);
        return 1;
    }
    vfs_node_release(home);
    return 0;
}

static void firstboot_finish(void) {
    terminal_state *term = firstboot_state.term;
    firstboot_state.active = 0;
    firstboot_state.term = NULL;
    terminal_newline(term);
    terminal_write_line(term, "Setup complete.");
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    term->input_len = 0;
    terminal_write_string(term, "$ ");
    term->cursor_row = term->cursor_row;
    term->cursor_col = 2;
}

static int buffer_has_non_whitespace(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c != ' ' && c != '\n' && c != '\t' && c != '\r' && c != '\f' && c != '\v') {
            return 1;
        }
    }
    return 0;
}

void firstboot_begin_if_needed(terminal_state *term) {
    if (term == NULL) {
        return;
    }
    vfs_node_t *passwd = vfs_resolve("/etc/passwd");
    if (passwd != NULL) {
        if (passwd->type == VFS_NODE_FILE) {
            vfs_file_t *file = vfs_open_node(passwd, VFS_O_READ);
            vfs_node_release(passwd);
            if (file != NULL) {
                char buf[128];
                int has_content = 0;
                while (1) {
                    ssize_t read_bytes = vfs_read(file, buf, sizeof(buf));
                    if (read_bytes <= 0) {
                        break;
                    }
                    if (buffer_has_non_whitespace(buf, (size_t)read_bytes)) {
                        has_content = 1;
                        break;
                    }
                }
                vfs_close(file);
                if (has_content) {
                    return;
                }
            }
        } else {
            vfs_node_release(passwd);
        }
    }
    
    memset(&firstboot_state, 0, sizeof(firstboot_state));
    firstboot_state.active = 1;
    firstboot_state.term = term;
    firstboot_state.step = FIRSTBOOT_USERNAME;
    firstboot_show_screen();
}

int firstboot_is_active(void) {
    return firstboot_state.active;
}

static void firstboot_backspace(void) {
    terminal_state *term = firstboot_state.term;
    if (firstboot_state.input_len == 0) {
        return;
    }
    if (term->cursor_col == 0) {
        return;
    }
    firstboot_state.input_len--;
    firstboot_state.input[firstboot_state.input_len] = '\0';
    
    uint16_t pos = term->cursor_row * terminal_cols + (term->cursor_col - 1);
    if (pos < terminal_buffer_size) {
        term->buffer[pos] = ' ';
    }
    term->cursor_col--;
}

static void firstboot_append_char(char c, int mask) {
    terminal_state *term = firstboot_state.term;
    if (firstboot_state.input_len + 1 >= FIRSTBOOT_MAX_INPUT) {
        return;
    }
    firstboot_state.input[firstboot_state.input_len++] = c;
    firstboot_state.input[firstboot_state.input_len] = '\0';
    terminal_write_char(term, mask ? '*' : c);
}

static char firstboot_key_to_char(key_code key, uint8_t modifiers) {
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
static void firstboot_accept_input(void) {
    terminal_state *term = firstboot_state.term;
    terminal_newline(term);
    
    if (firstboot_state.step == FIRSTBOOT_USERNAME) {
        if (!username_is_valid(firstboot_state.input)) {
            terminal_write_line(term, "Invalid username. Use letters/numbers, no dots or slashes.");
            firstboot_set_prompt("Enter username: ");
            firstboot_clear_input();
            return;
        }
        strncpy(firstboot_state.username, firstboot_state.input, FIRSTBOOT_MAX_INPUT - 1);
        firstboot_state.username[FIRSTBOOT_MAX_INPUT - 1] = '\0';
        firstboot_state.step = FIRSTBOOT_PASSWORD;
        firstboot_set_prompt("Enter password: ");
        firstboot_clear_input();
        return;
    }
    
    if (firstboot_state.step == FIRSTBOOT_PASSWORD) {
        if (firstboot_state.input_len == 0) {
            terminal_write_line(term, "Password cannot be empty.");
            firstboot_set_prompt("Enter password: ");
            firstboot_clear_input();
            return;
        }
        strncpy(firstboot_state.password, firstboot_state.input, FIRSTBOOT_MAX_PASS - 1);
        firstboot_state.password[FIRSTBOOT_MAX_PASS - 1] = '\0';
        firstboot_state.password_len = strlen(firstboot_state.password);
        firstboot_state.step = FIRSTBOOT_PASSWORD_CONFIRM;
        firstboot_set_prompt("Confirm password: ");
        firstboot_clear_input();
        return;
    }
    
    if (firstboot_state.step == FIRSTBOOT_PASSWORD_CONFIRM) {
        strncpy(firstboot_state.password_confirm, firstboot_state.input, FIRSTBOOT_MAX_PASS - 1);
        firstboot_state.password_confirm[FIRSTBOOT_MAX_PASS - 1] = '\0';
        firstboot_state.password_confirm_len = strlen(firstboot_state.password_confirm);
        
        if (strcmp(firstboot_state.password, firstboot_state.password_confirm) != 0) {
            terminal_write_line(term, "Passwords do not match. Try again.");
            firstboot_state.step = FIRSTBOOT_PASSWORD;
            firstboot_set_prompt("Enter password: ");
            firstboot_clear_input();
            return;
        }
        
        if (!rename_home_dir(firstboot_state.username)) {
            terminal_write_line(term, "Failed to set home directory.");
            firstboot_finish();
            return;
        }
        
        if (!write_passwd_file(firstboot_state.username, firstboot_state.password)) {
            terminal_write_line(term, "Failed to write /etc/passwd.");
            firstboot_finish();
            return;
        }
        
        firstboot_finish();
    }
}

void firstboot_handle_key_event(key_event evt) {
    if (!firstboot_state.active || firstboot_state.term == NULL) {
        return;
    }
    
    if (evt.key == key_backspace) {
        firstboot_backspace();
        return;
    }
    if (evt.key == key_enter) {
        firstboot_accept_input();
        return;
    }
    
    if (evt.key == key_tab || evt.key == key_esc) {
        return;
    }
    
    char c = firstboot_key_to_char(evt.key, evt.modifiers);
    if (c == 0) {
        return;
    }
    
    int mask = (firstboot_state.step != FIRSTBOOT_USERNAME);
    firstboot_append_char(c, mask);
}

