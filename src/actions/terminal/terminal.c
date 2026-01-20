// main terminal file - includes all terminal sub-modules
#include "terminal.h"
#include "terminal_cmd.h"
#include "builtins.h"
#include "debug_helper.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

terminal_state terminals[max_windows];
uint8_t active_terminal = 0;
uint8_t window_count = 0;
uint8_t selected_terminal = 0;  // last opened terminal (for splitting) - accessed by terminal_layout.c
static uint8_t terminal_zoom = 1;
static char history_path[256] = {0};
static uint16_t terminal_active_color = 0x0000;
static uint8_t terminal_active_color_loaded = 0;

void init_terminal_system(void) {
    window_count = 0;
    active_terminal = 0;
    selected_terminal = 0;
#ifdef ARDUINO
    extern void terminal_image_view_release(terminal_state *term);
#endif
    for (uint8_t i = 0; i < max_windows; i++) {
#ifdef ARDUINO
        terminal_image_view_release(&terminals[i]);
#endif
        terminals[i].active = 0;
        terminals[i].cursor_row = 0;
        terminals[i].cursor_col = 0;
        terminals[i].input_pos = 0;
        terminals[i].input_len = 0;
        terminals[i].autocomplete_suffix[0] = '\0';
        terminals[i].autocomplete_len = 0;
        terminals[i].autocomplete_base_len = 0;
        terminals[i].autocomplete_applied = 0;
        terminals[i].history_count = 0;
        terminals[i].history_pos = 0;
        terminals[i].cwd = NULL;
        terminals[i].pipe_input = NULL;
        terminals[i].pipe_input_len = 0;
        terminals[i].fastfetch_image_active = 0;
        terminals[i].fastfetch_image_path[0] = '\0';
        terminals[i].fastfetch_image_pixels = NULL;
        terminals[i].fastfetch_image_w = 0;
        terminals[i].fastfetch_image_h = 0;
        terminals[i].fastfetch_start_row = 0;
        terminals[i].fastfetch_line_count = 0;
        terminals[i].fastfetch_text_lines = 0;
        terminals[i].image_view_active = 0;
        terminals[i].image_view_path[0] = '\0';
        memset(terminals[i].buffer, ' ', terminal_buffer_size);
        memset(terminals[i].input_line, 0, terminal_cols);
        terminals[i].x = 0;
        terminals[i].y = 0;
        terminals[i].width = 0;
        terminals[i].height = 0;
        terminals[i].split_dir = SPLIT_NONE;
        for (uint8_t j = 0; j < max_input_history; j++) {
            memset(terminals[i].history[j], 0, terminal_cols);
        }
        for (uint8_t j = 0; j < max_pipe_commands; j++) {
            terminals[i].pipes[j].active = 0;
            terminals[i].pipes[j].read_fd = -1;
            terminals[i].pipes[j].write_fd = -1;
        }
    }
    DEBUG_PRINT("[TERMINAL] Terminal system initialized\n");
}

void new_terminal(void);
void close_terminal(void);

terminal_state *get_active_terminal(void) {
    if (window_count == 0) {
        return NULL;
    }
    return &terminals[active_terminal];
}

void terminal_write_char(terminal_state *term, char c);
void terminal_write_string(terminal_state *term, const char *str);
void terminal_write_line(terminal_state *term, const char *str);
void terminal_newline(terminal_state *term);
void terminal_clear(terminal_state *term);

static int read_username_from_passwd(char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }
    out[0] = '\0';
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
    if (colon) {
        *colon = '\0';
    }
    if (buf[0] == '\0') {
        return 0;
    }
    size_t copy_len = strnlen(buf, out_len - 1);
    memcpy(out, buf, copy_len);
    out[copy_len] = '\0';
    return 1;
}

static int terminal_history_get_path(char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }
    char username[64];
    if (!read_username_from_passwd(username, sizeof(username))) {
        strncpy(username, "user", sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
    snprintf(out, out_len, "/home/%s/.config/shell/history", username);
    return 1;
}

static void terminal_ensure_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return;
    }
    vfs_node_t *existing = vfs_resolve(path);
    if (existing != NULL) {
        vfs_node_release(existing);
        return;
    }
    char path_copy[256];
    size_t path_len = strnlen(path, sizeof(path_copy) - 1);
    memcpy(path_copy, path, path_len);
    path_copy[path_len] = '\0';
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash == NULL || last_slash == path_copy) {
        return;
    }
    *last_slash = '\0';
    const char *name = last_slash + 1;
    if (name[0] == '\0') {
        return;
    }
    vfs_node_t *parent = vfs_resolve(path_copy);
    if (parent == NULL) {
        return;
    }
    vfs_node_t *created = vfs_dir_create_node(parent, name, VFS_NODE_DIR);
    if (created != NULL) {
        vfs_node_release(created);
    }
    vfs_node_release(parent);
}

static void terminal_history_ensure_dirs(void) {
    char username[64];
    if (!read_username_from_passwd(username, sizeof(username))) {
        strncpy(username, "user", sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
    char config_path[256];
    char shell_path[256];
    snprintf(config_path, sizeof(config_path), "/home/%s/.config", username);
    snprintf(shell_path, sizeof(shell_path), "/home/%s/.config/shell", username);
    terminal_ensure_dir(config_path);
    terminal_ensure_dir(shell_path);
}

static void terminal_history_push(terminal_state *term, const char *line) {
    if (term == NULL || line == NULL) {
        return;
    }
    size_t copy_len = strlen(line);
    if (copy_len > terminal_cols - 1) {
        copy_len = terminal_cols - 1;
    }
    if (copy_len == 0) {
        return;
    }
    if (term->history_count < max_input_history) {
        memcpy(term->history[term->history_count], line, copy_len);
        term->history[term->history_count][copy_len] = '\0';
        term->history_count++;
    } else {
        for (uint8_t i = 0; i < max_input_history - 1; i++) {
            memcpy(term->history[i], term->history[i + 1], terminal_cols - 1);
            term->history[i][terminal_cols - 1] = '\0';
        }
        memcpy(term->history[max_input_history - 1], line, copy_len);
        term->history[max_input_history - 1][copy_len] = '\0';
    }
    term->history_pos = term->history_count;
}

static void terminal_history_append_persistent(const char *line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }
    if (history_path[0] == '\0') {
        if (!terminal_history_get_path(history_path, sizeof(history_path))) {
            return;
        }
    }
    terminal_history_ensure_dirs();
    vfs_file_t *file = vfs_open(history_path, VFS_O_WRITE | VFS_O_APPEND | VFS_O_CREATE);
    if (file == NULL) {
        return;
    }
    size_t len = strlen(line);
    vfs_write(file, line, len);
    vfs_write(file, "\n", 1);
    vfs_close(file);
}

static void terminal_history_load(terminal_state *term) {
    if (term == NULL) {
        return;
    }
    if (history_path[0] == '\0') {
        if (!terminal_history_get_path(history_path, sizeof(history_path))) {
            return;
        }
    }
    vfs_file_t *file = vfs_open(history_path, VFS_O_READ);
    if (file == NULL) {
        return;
    }
    char line[terminal_cols];
    size_t line_len = 0;
    char buf[128];
    ssize_t read_bytes = 0;
    while ((read_bytes = vfs_read(file, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < read_bytes; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    terminal_history_push(term, line);
                    line_len = 0;
                }
            } else {
                if (line_len < terminal_cols - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        terminal_history_push(term, line);
    }
    vfs_close(file);
}

static void terminal_update_autocomplete(terminal_state *term) {
    if (term == NULL) {
        return;
    }
    term->autocomplete_suffix[0] = '\0';
    term->autocomplete_len = 0;
    term->autocomplete_base_len = term->input_len;
    term->autocomplete_applied = 0;
    if (term->input_len == 0 || term->history_count == 0) {
        return;
    }
    for (int i = (int)term->history_count - 1; i >= 0; i--) {
        const char *cmd = term->history[i];
        if (cmd == NULL || cmd[0] == '\0') {
            continue;
        }
        size_t cmd_len = strlen(cmd);
        if (cmd_len <= term->input_len) {
            continue;
        }
        if (strncmp(cmd, term->input_line, term->input_len) == 0) {
            size_t suffix_len = cmd_len - term->input_len;
            if (suffix_len > terminal_cols - 1) {
                suffix_len = terminal_cols - 1;
            }
            memcpy(term->autocomplete_suffix, cmd + term->input_len, suffix_len);
            term->autocomplete_suffix[suffix_len] = '\0';
            term->autocomplete_len = (uint8_t)suffix_len;
            term->autocomplete_base_len = term->input_len;
            return;
        }
    }
}

void terminal_load_history(terminal_state *term) {
    terminal_history_load(term);
}

static int parse_rgb_component(const char *s, uint8_t *out) {
    if (s == NULL || out == NULL) {
        return 0;
    }
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    char *end = NULL;
    long val = strtol(s, &end, 10);
    if (end == s || val < 0 || val > 255) {
        return 0;
    }
    *out = (uint8_t)val;
    return 1;
}

static int parse_hex_color(const char *s, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == NULL || r == NULL || g == NULL || b == NULL) {
        return 0;
    }
    if (*s == '#') {
        s++;
    }
    if (strlen(s) < 6) {
        return 0;
    }
    char buf[3] = {0};
    buf[0] = s[0];
    buf[1] = s[1];
    *r = (uint8_t)strtoul(buf, NULL, 16);
    buf[0] = s[2];
    buf[1] = s[3];
    *g = (uint8_t)strtoul(buf, NULL, 16);
    buf[0] = s[4];
    buf[1] = s[5];
    *b = (uint8_t)strtoul(buf, NULL, 16);
    return 1;
}

static int parse_named_color(const char *value, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (value == NULL || r == NULL || g == NULL || b == NULL) {
        return 0;
    }
    char name[32];
    size_t len = 0;
    while (value[len] != '\0' && !isspace((unsigned char)value[len]) && len < sizeof(name) - 1) {
        name[len] = (char)tolower((unsigned char)value[len]);
        len++;
    }
    name[len] = '\0';
    
    if (strcmp(name, "black") == 0) { *r = 0; *g = 0; *b = 0; return 1; }
    if (strcmp(name, "white") == 0) { *r = 255; *g = 255; *b = 255; return 1; }
    if (strcmp(name, "red") == 0) { *r = 255; *g = 0; *b = 0; return 1; }
    if (strcmp(name, "green") == 0) { *r = 0; *g = 255; *b = 0; return 1; }
    if (strcmp(name, "blue") == 0) { *r = 0; *g = 0; *b = 255; return 1; }
    if (strcmp(name, "yellow") == 0) { *r = 255; *g = 255; *b = 0; return 1; }
    if (strcmp(name, "cyan") == 0) { *r = 0; *g = 255; *b = 255; return 1; }
    if (strcmp(name, "magenta") == 0) { *r = 255; *g = 0; *b = 255; return 1; }
    if (strcmp(name, "gray") == 0 || strcmp(name, "grey") == 0) { *r = 128; *g = 128; *b = 128; return 1; }
    if (strcmp(name, "lightgray") == 0 || strcmp(name, "lightgrey") == 0) { *r = 192; *g = 192; *b = 192; return 1; }
    if (strcmp(name, "darkgray") == 0 || strcmp(name, "darkgrey") == 0) { *r = 64; *g = 64; *b = 64; return 1; }
    if (strcmp(name, "orange") == 0) { *r = 255; *g = 165; *b = 0; return 1; }
    if (strcmp(name, "purple") == 0) { *r = 128; *g = 0; *b = 128; return 1; }
    if (strcmp(name, "pink") == 0) { *r = 255; *g = 192; *b = 203; return 1; }
    if (strcmp(name, "brown") == 0) { *r = 165; *g = 42; *b = 42; return 1; }
    return 0;
}

static int parse_color_value(const char *value, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (value == NULL || r == NULL || g == NULL || b == NULL) {
        return 0;
    }
    while (*value != '\0' && isspace((unsigned char)*value)) {
        value++;
    }
    if (*value == '#' || (isxdigit((unsigned char)value[0]) && isxdigit((unsigned char)value[1]) &&
                          isxdigit((unsigned char)value[2]) && isxdigit((unsigned char)value[3]) &&
                          isxdigit((unsigned char)value[4]) && isxdigit((unsigned char)value[5]))) {
        if (parse_hex_color(value, r, g, b)) {
            return 1;
        }
    }
    const char *comma1 = strchr(value, ',');
    if (comma1 != NULL) {
        const char *comma2 = strchr(comma1 + 1, ',');
        if (comma2 != NULL) {
            uint8_t rr = 0, gg = 0, bb = 0;
            if (parse_rgb_component(value, &rr) &&
                parse_rgb_component(comma1 + 1, &gg) &&
                parse_rgb_component(comma2 + 1, &bb)) {
                *r = rr;
                *g = gg;
                *b = bb;
                return 1;
            }
        }
    }
    return parse_named_color(value, r, g, b);
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    // swap red/blue to match panel ordering
    uint8_t rr = b;
    uint8_t bb = r;
    return (uint16_t)(((rr & 0xF8) << 8) | ((g & 0xFC) << 3) | (bb >> 3));
}

static void terminal_load_active_color(void) {
    if (terminal_active_color_loaded) {
        return;
    }
    terminal_active_color_loaded = 1;
    char username[64];
    if (!read_username_from_passwd(username, sizeof(username))) {
        return;
    }
    char path[256];
    snprintf(path, sizeof(path), "/home/%s/.config/TILIXI/TILIXI.conf", username);
    vfs_file_t *file = vfs_open(path, VFS_O_READ);
    if (file == NULL) {
        return;
    }
    char buf[256];
    ssize_t read_bytes = 0;
    char line[256];
    size_t line_len = 0;
    while ((read_bytes = vfs_read(file, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < read_bytes; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    const char *prefix = "color_active=";
                    size_t prefix_len = strlen(prefix);
                    if (strncmp(line, prefix, prefix_len) == 0) {
                        const char *value = line + prefix_len;
                        uint8_t r = 0, g = 0, b = 0;
                        if (parse_color_value(value, &r, &g, &b)) {
                            uint8_t inv_r = (uint8_t)(255 - r);
                            uint8_t inv_g = (uint8_t)(255 - g);
                            uint8_t inv_b = (uint8_t)(255 - b);
                            terminal_active_color = rgb888_to_rgb565(inv_r, inv_g, inv_b);
                            vfs_close(file);
                            return;
                        }
                    }
                    line_len = 0;
                }
            } else {
                if (line_len < sizeof(line) - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        const char *prefix = "color_active=";
        size_t prefix_len = strlen(prefix);
        if (strncmp(line, prefix, prefix_len) == 0) {
            const char *value = line + prefix_len;
            uint8_t r = 0, g = 0, b = 0;
            if (parse_color_value(value, &r, &g, &b)) {
                uint8_t inv_r = (uint8_t)(255 - r);
                uint8_t inv_g = (uint8_t)(255 - g);
                uint8_t inv_b = (uint8_t)(255 - b);
                terminal_active_color = rgb888_to_rgb565(inv_r, inv_g, inv_b);
            }
        }
    }
    vfs_close(file);
}

uint16_t terminal_get_active_color(void) {
    terminal_load_active_color();
    return terminal_active_color;
}

void terminal_reload_config(void) {
    terminal_active_color_loaded = 0;
    terminal_load_active_color();
#ifdef ARDUINO
    extern void terminal_render_all(void);
    terminal_render_all();
#endif
}

static void terminal_redraw_input_line(terminal_state *term) {
    if (term == NULL || !term->active) return;
    uint16_t line_start = term->cursor_row * terminal_cols;
    if (line_start >= terminal_buffer_size) return;
    for (uint16_t col = 0; col < terminal_cols; col++) {
        if (line_start + col < terminal_buffer_size) {
            term->buffer[line_start + col] = ' ';
        }
    }
    if (line_start + 0 < terminal_buffer_size) term->buffer[line_start + 0] = '$';
    if (line_start + 1 < terminal_buffer_size) term->buffer[line_start + 1] = ' ';
    for (uint16_t i = 0; i < term->input_len && (2 + i) < terminal_cols; i++) {
        uint16_t buf_pos = line_start + 2 + i;
        if (buf_pos < terminal_buffer_size) {
            term->buffer[buf_pos] = term->input_line[i];
        }
    }
    term->cursor_col = 2 + term->input_pos;
}

void terminal_handle_key(terminal_state *term, char key) {
    if (term == NULL || !term->active) return;
    
    if (term->input_len < terminal_cols - 1 && key >= 32 && key < 127) {
        if (term->input_pos < term->input_len) {
            memmove(&term->input_line[term->input_pos + 1],
                    &term->input_line[term->input_pos],
                    term->input_len - term->input_pos);
        }
        term->input_line[term->input_pos] = key;
        term->input_pos++;
        term->input_len++;
        term->input_line[term->input_len] = '\0';
        terminal_redraw_input_line(term);
        terminal_update_autocomplete(term);
    }
}

void terminal_handle_backspace(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    if (term->input_pos > 0 && term->input_len > 0) {
        term->input_pos--;
        if (term->input_pos < term->input_len - 1) {
            memmove(&term->input_line[term->input_pos],
                    &term->input_line[term->input_pos + 1],
                    term->input_len - term->input_pos - 1);
        }
        term->input_len--;
        term->input_line[term->input_len] = '\0';
        terminal_redraw_input_line(term);
        terminal_update_autocomplete(term);
    }
}

void terminal_handle_enter(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    // parse command first to check if it's empty
    extern void terminal_parse_command(const char *input, command_tokens_t *out_tokens);
    extern void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens);
    extern void terminal_execute_command(terminal_state *term, command_tokens_t *tokens);
    
    command_tokens_t tokens;
    terminal_parse_command(term->input_line, &tokens);
    
    // add to history
    // theres probably a more unified way to do this that looks a lot better
    // butttttttt im too lazy and dumb...
    if (term->input_len > 0) {
        terminal_history_push(term, term->input_line);
        terminal_history_append_persistent(term->input_line);
        for (uint8_t i = 0; i < max_windows; i++) {
            if (&terminals[i] != term && terminals[i].active) {
                terminal_history_push(&terminals[i], term->input_line);
            }
        }
    }
    
    // move to next line (off the input line) - always do this
    terminal_newline(term);

    // clear input line now so renders during command execution don't duplicate it
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    term->input_len = 0;
    term->autocomplete_suffix[0] = '\0';
    term->autocomplete_len = 0;
    term->autocomplete_base_len = 0;
    term->autocomplete_applied = 0;
    
    // execute command if there is one
    if (tokens.token_count > 0) {
        if (tokens.has_pipe) {
            terminal_execute_pipeline(term, &tokens);
        } else {
            terminal_execute_command(term, &tokens);
        }
    }
    
    // if a fullscreen app is active (e.g. nano), don't draw the shell prompt
    extern int nano_is_active(void);
    extern int passwd_is_active(void);
    if (nano_is_active() || passwd_is_active()) {
        return;
    }
    
    // show prompt
    // commands should have added their own newline if they output something
    // if we're not at the start of a new line (cursor_col > 0), add a newline
    if (term->cursor_col > 0) {
        terminal_newline(term);
    }
    terminal_write_string(term, "$ ");
    term->cursor_col = 2;  // cursor is after "$ "
}

void terminal_handle_arrow_up(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    if (term->history_pos > 0) {
        term->history_pos--;
        strncpy(term->input_line, term->history[term->history_pos], terminal_cols - 1);
        term->input_line[terminal_cols - 1] = '\0';
        term->input_len = strlen(term->input_line);
        term->input_pos = term->input_len;
        terminal_redraw_input_line(term);
        terminal_update_autocomplete(term);
    }
}

void terminal_handle_arrow_down(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    if (term->history_pos < term->history_count) {
        term->history_pos++;
        if (term->history_pos < term->history_count) {
            strncpy(term->input_line, term->history[term->history_pos], terminal_cols - 1);
            term->input_line[terminal_cols - 1] = '\0';
        } else {
            memset(term->input_line, 0, terminal_cols);
        }
        term->input_len = strlen(term->input_line);
        term->input_pos = term->input_len;
        terminal_redraw_input_line(term);
        terminal_update_autocomplete(term);
    }
}

void terminal_handle_arrow_left(terminal_state *term) {
    if (term == NULL || !term->active) return;
    if (term->autocomplete_applied &&
        term->input_pos == term->input_len &&
        term->input_len == term->autocomplete_base_len + term->autocomplete_len) {
        term->input_len = term->autocomplete_base_len;
        term->input_pos = term->input_len;
        term->input_line[term->input_len] = '\0';
        term->autocomplete_applied = 0;
        terminal_redraw_input_line(term);
        terminal_update_autocomplete(term);
        return;
    }
    if (term->input_pos > 0) {
        term->input_pos--;
        term->cursor_col = 2 + term->input_pos;
    }
}

void terminal_handle_arrow_right(terminal_state *term) {
    if (term == NULL || !term->active) return;
    if (term->input_pos == term->input_len &&
        term->autocomplete_len > 0 &&
        !term->autocomplete_applied) {
        size_t avail = terminal_cols - 1 - term->input_len;
        size_t append_len = term->autocomplete_len;
        if (append_len > avail) {
            append_len = avail;
        }
        if (append_len > 0) {
            memcpy(&term->input_line[term->input_len], term->autocomplete_suffix, append_len);
            term->input_len += (uint8_t)append_len;
            term->input_line[term->input_len] = '\0';
            term->input_pos = term->input_len;
            term->autocomplete_len = (uint8_t)append_len;
            term->autocomplete_applied = 1;
            terminal_redraw_input_line(term);
            return;
        }
    }
    if (term->input_pos < term->input_len) {
        term->input_pos++;
        term->cursor_col = 2 + term->input_pos;
    }
}

uint8_t terminal_get_zoom(void) {
    return terminal_zoom;
}

static void terminal_set_zoom(uint8_t zoom) {
    if (zoom < 1) zoom = 1;
    if (zoom > 3) zoom = 3;
    if (terminal_zoom == zoom) {
        return;
    }
    terminal_zoom = zoom;
#ifdef ARDUINO
    extern void terminal_render_all(void);
    extern void fastfetch_rescale_image(terminal_state *term);
    for (uint8_t i = 0; i < max_windows; i++) {
        if (terminals[i].active && terminals[i].fastfetch_image_active) {
            fastfetch_rescale_image(&terminals[i]);
        }
    }
    terminal_render_all();
#endif
}

void terminal_zoom_in(void) {
    terminal_set_zoom(terminal_zoom + 1);
}

void terminal_zoom_out(void) {
    if (terminal_zoom > 1) {
        terminal_set_zoom(terminal_zoom - 1);
    }
}

extern void terminal_parse_command(const char *input, command_tokens_t *out_tokens);
extern void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens);

// command execution
void terminal_execute_command(terminal_state *term, command_tokens_t *tokens) {
    if (term == NULL || tokens == NULL || tokens->token_count == 0) {
        return;
    }
    
    const char *cmd_name = tokens->tokens[0];
    char **argv = (char **)tokens->tokens;
    int argc = tokens->token_count;
    
    // find and execute command using builtin system
    builtin_cmd *cmd = builtins_find(cmd_name);
    if (cmd != NULL) {
        int result = cmd->handler(term, argc, argv);
        if (result != 0) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Command failed with code %d\n", result);
            terminal_write_string(term, error_msg);
        }
    } else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "damocles: unknown command: %s\n", cmd_name);
        terminal_write_string(term, error_msg);
    }
}

#ifdef ARDUINO
    extern void terminal_render_all(void);
    extern void terminal_render_window(terminal_state *term);
#endif
