#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "keyboard_core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NANO_MAX_BUFFER 4096
#define NANO_PROMPT_MAX 32

typedef enum {
    NANO_PROMPT_NONE = 0,
    NANO_PROMPT_CMD,
    NANO_PROMPT_SAVE_CHOICE,
    NANO_PROMPT_CONFIRM
} nano_prompt_state_t;

typedef struct {
    int active;
    terminal_state *term;
    char path[256];
    char buffer[NANO_MAX_BUFFER];
    size_t length;
    size_t cursor;
    int dirty;
    nano_prompt_state_t prompt_state;
    char prompt_input[NANO_PROMPT_MAX];
    size_t prompt_len;
    int pending_save;
} nano_state_t;

static nano_state_t nano_state = {0};

int cmd_nano(terminal_state *term, int argc, char **argv);
int nano_is_active(void);
void nano_handle_key_event(key_event evt);

const builtin_cmd cmd_nano_def = {
    .name = "nano",
    .handler = cmd_nano,
    .help = "Edit files"
};

static void nano_write_line(terminal_state *term, int row, const char *text) {
    if (term == NULL || row < 0 || row >= terminal_rows) {
        return;
    }
    
    char *line = &term->buffer[row * terminal_cols];
    memset(line, ' ', terminal_cols);
    
    if (text != NULL) {
        size_t len = strlen(text);
        if (len > terminal_cols) {
            len = terminal_cols;
        }
        memcpy(line, text, len);
    }
}

static char nano_key_to_char(key_code key, uint8_t modifiers) {
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

static void nano_reset_prompt(void) {
    nano_state.prompt_state = NANO_PROMPT_NONE;
    nano_state.prompt_len = 0;
    nano_state.prompt_input[0] = '\0';
    nano_state.pending_save = 0;
}

static void nano_exit_to_shell(void) {
    terminal_state *term = nano_state.term;
    nano_state.active = 0;
    nano_state.term = NULL;
    nano_reset_prompt();
    
    if (term == NULL) {
        return;
    }
    
    terminal_clear(term);
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    terminal_write_string(term, "$ ");
    term->cursor_row = 0;
    term->cursor_col = 2;
}

static int nano_load_file(terminal_state *term, const char *path) {
    vfs_node_t *node = vfs_resolve_at(term ? term->cwd : NULL, path);
    if (node == NULL) {
        return SHELL_ERR;
    }
    
    vfs_file_t *file = vfs_open_node(node, VFS_O_READ);
    if (file == NULL) {
        vfs_node_release(node);
        return SHELL_ERR;
    }
    
    nano_state.length = 0;
    while (nano_state.length < NANO_MAX_BUFFER - 1) {
        size_t remaining = NANO_MAX_BUFFER - 1 - nano_state.length;
        ssize_t read_bytes = vfs_read(file, nano_state.buffer + nano_state.length, remaining);
        if (read_bytes <= 0) {
            break;
        }
        nano_state.length += (size_t)read_bytes;
    }
    nano_state.buffer[nano_state.length] = '\0';
    vfs_close(file);
    vfs_node_release(node);
    return SHELL_OK;
}

static int nano_save_file(void) {
    vfs_node_t *node = vfs_resolve_at(nano_state.term ? nano_state.term->cwd : NULL,
                                      nano_state.path);
    if (node == NULL) {
        return SHELL_ERR;
    }
    
    vfs_file_t *file = vfs_open_node(node, VFS_O_WRITE | VFS_O_TRUNC | VFS_O_CREATE);
    if (file == NULL) {
        vfs_node_release(node);
        return SHELL_ERR;
    }
    
    ssize_t written = vfs_write(file, nano_state.buffer, nano_state.length);
    vfs_close(file);
    vfs_node_release(node);
    return (written < 0 || (size_t)written != nano_state.length) ? SHELL_ERR : SHELL_OK;
}

static void nano_render(void) {
    terminal_state *term = nano_state.term;
    if (term == NULL) {
        return;
    }
    
    terminal_clear(term);
    
    char header[terminal_cols + 1];
    snprintf(header, sizeof(header), "  GNU nano  %s", nano_state.path);
    nano_write_line(term, 0, header);
    
    int edit_rows = terminal_rows - 3;
    size_t idx = 0;
    size_t cursor_idx = nano_state.cursor;
    int cursor_row = 1;
    int cursor_col = 0;
    
    int total_lines = 0;
    if (nano_state.length > 0) {
        total_lines = 1;
        for (size_t i = 0; i < nano_state.length; i++) {
            if (nano_state.buffer[i] == '\n') {
                total_lines++;
            }
        }
    }
    
    int digits = 1;
    int temp_lines = total_lines > 0 ? total_lines : 1;
    while (temp_lines >= 10) {
        digits++;
        temp_lines /= 10;
    }
    int prefix_width = digits + 1;
    if (prefix_width < 3) {
        prefix_width = 3;
    }
    if (prefix_width > terminal_cols - 1) {
        prefix_width = terminal_cols - 1;
    }
    
    int cursor_line = 0;
    int cursor_col_text = 0;
    for (size_t i = 0; i < cursor_idx && i < nano_state.length; i++) {
        if (nano_state.buffer[i] == '\n') {
            cursor_line++;
            cursor_col_text = 0;
        } else {
            cursor_col_text++;
        }
    }
    
    if (cursor_line < 0) {
        cursor_line = 0;
    }
    if (cursor_line >= edit_rows) {
        cursor_line = edit_rows - 1;
    }
    if (cursor_col_text < 0) {
        cursor_col_text = 0;
    }
    if (cursor_col_text > terminal_cols - prefix_width - 1) {
        cursor_col_text = terminal_cols - prefix_width - 1;
    }
    cursor_row = 1 + cursor_line;
    cursor_col = prefix_width + cursor_col_text;
    
    for (int row = 0; row < edit_rows; row++) {
        char line[terminal_cols + 1];
        memset(line, ' ', terminal_cols);
        line[terminal_cols] = '\0';
        
        if (row < total_lines && total_lines > 0) {
            char prefix[16];
            snprintf(prefix, sizeof(prefix), "%*d ", digits, row + 1);
            size_t prefix_len = strlen(prefix);
            if ((int)prefix_len > prefix_width) {
                prefix_len = prefix_width;
            }
            memcpy(line, prefix, prefix_len);
        } else {
            line[0] = '~';
        }
        
        int col = 0;
        while (idx < nano_state.length) {
            char c = nano_state.buffer[idx];
            if (c == '\n') {
                idx++;
                break;
            }
            if (prefix_width + col >= terminal_cols) {
                break;
            }
            line[prefix_width + col] = c;
            col++;
            idx++;
        }
        
        nano_write_line(term, 1 + row, line);
    }
    
    char status[terminal_cols + 1];
    const char *dirty = nano_state.dirty ? "Modified" : "Saved";
    snprintf(status, sizeof(status), "File: %s -- %s", nano_state.path, dirty);
    nano_write_line(term, terminal_rows - 2, status);
    
    char footer[terminal_cols + 1];
    if (nano_state.prompt_state == NANO_PROMPT_CMD) {
        snprintf(footer, sizeof(footer), "Command: %s", nano_state.prompt_input);
    } else if (nano_state.prompt_state == NANO_PROMPT_SAVE_CHOICE) {
        snprintf(footer, sizeof(footer), "Save changes? (y/n)");
    } else if (nano_state.prompt_state == NANO_PROMPT_CONFIRM) {
        snprintf(footer, sizeof(footer), "Press Enter to confirm");
    } else {
        snprintf(footer, sizeof(footer), "^P Command");
    }
    nano_write_line(term, terminal_rows - 1, footer);
    
    term->cursor_row = cursor_row;
    term->cursor_col = cursor_col;
}

int nano_is_active(void) {
    return nano_state.active;
}

static void nano_insert_char(char c) {
    if (nano_state.length >= NANO_MAX_BUFFER - 1) {
        return;
    }
    memmove(nano_state.buffer + nano_state.cursor + 1,
            nano_state.buffer + nano_state.cursor,
            nano_state.length - nano_state.cursor);
    nano_state.buffer[nano_state.cursor] = c;
    nano_state.cursor++;
    nano_state.length++;
    nano_state.buffer[nano_state.length] = '\0';
    nano_state.dirty = 1;
}

static void nano_backspace(void) {
    if (nano_state.cursor == 0 || nano_state.length == 0) {
        return;
    }
    memmove(nano_state.buffer + nano_state.cursor - 1,
            nano_state.buffer + nano_state.cursor,
            nano_state.length - nano_state.cursor);
    nano_state.cursor--;
    nano_state.length--;
    nano_state.buffer[nano_state.length] = '\0';
    nano_state.dirty = 1;
}

void nano_handle_key_event(key_event evt) {
    if (!nano_state.active || nano_state.term == NULL) {
        return;
    }
    
    if ((evt.modifiers & mod_ctrl) && evt.key == key_x) {
        if (!nano_state.dirty) {
            nano_exit_to_shell();
            return;
        }
        nano_state.prompt_state = NANO_PROMPT_SAVE_CHOICE;
        nano_render();
        return;
    }
    
    if ((evt.modifiers & mod_ctrl) && evt.key == key_p) {
        if (nano_state.prompt_state == NANO_PROMPT_NONE) {
            nano_state.prompt_state = NANO_PROMPT_CMD;
            nano_state.prompt_len = 0;
            nano_state.prompt_input[0] = '\0';
        } else {
            nano_reset_prompt();
        }
        nano_render();
        return;
    }
    
    if (nano_state.prompt_state == NANO_PROMPT_CMD) {
        if (evt.key == key_backspace) {
            if (nano_state.prompt_len > 0) {
                nano_state.prompt_len--;
                nano_state.prompt_input[nano_state.prompt_len] = '\0';
            }
        } else if (evt.key == key_enter) {
            if (nano_state.prompt_len == 1 && nano_state.prompt_input[0] == 'x') {
                nano_state.prompt_state = NANO_PROMPT_SAVE_CHOICE;
            } else {
                nano_reset_prompt();
            }
        } else {
            char c = nano_key_to_char(evt.key, evt.modifiers);
            if (c != 0 && nano_state.prompt_len < NANO_PROMPT_MAX - 1) {
                if (nano_state.prompt_len == 0 && c == 'x') {
                    nano_state.prompt_state = NANO_PROMPT_SAVE_CHOICE;
                } else {
                    nano_state.prompt_input[nano_state.prompt_len++] = c;
                    nano_state.prompt_input[nano_state.prompt_len] = '\0';
                }
            }
        }
        nano_render();
        return;
    }
    
    if (nano_state.prompt_state == NANO_PROMPT_SAVE_CHOICE) {
        char c = nano_key_to_char(evt.key, evt.modifiers);
        if (c == 'y' || c == 'Y') {
            nano_state.pending_save = 1;
            nano_state.prompt_state = NANO_PROMPT_CONFIRM;
        } else if (c == 'n' || c == 'N') {
            nano_state.pending_save = 0;
            nano_state.prompt_state = NANO_PROMPT_CONFIRM;
        }
        nano_render();
        return;
    }
    
    if (nano_state.prompt_state == NANO_PROMPT_CONFIRM) {
        if (evt.key == key_enter) {
            if (nano_state.pending_save) {
                if (nano_save_file() == SHELL_OK) {
                    nano_state.dirty = 0;
                    nano_exit_to_shell();
                    return;
                }
            } else {
                nano_exit_to_shell();
                return;
            }
            nano_reset_prompt();
            nano_render();
        }
        return;
    }
    
    if (evt.key == key_enter) {
        nano_insert_char('\n');
    } else if (evt.key == key_backspace) {
        nano_backspace();
    } else {
        char c = nano_key_to_char(evt.key, evt.modifiers);
        if (c != 0 && !(evt.modifiers & mod_ctrl)) {
            nano_insert_char(c);
        }
    }
    
    nano_render();
}

int cmd_nano(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "nano: missing file operand");
        return SHELL_EINVAL;
    }
    
    if (argc > 2) {
        shell_error(term, "nano: too many arguments");
        return SHELL_EINVAL;
    }
    
    const char *path = argv[1];
    if (path == NULL) {
        return SHELL_ERR;
    }
    
    vfs_node_t *node = vfs_resolve_at(term->cwd, path);
    if (node == NULL) {
        shell_error(term, "nano: %s: no such file or directory", path);
        return SHELL_ENOENT;
    }
    
    if (node->type != VFS_NODE_FILE) {
        shell_error(term, "nano: %s: not a file", path);
        vfs_node_release(node);
        return SHELL_EINVAL;
    }
    vfs_node_release(node);
    
    memset(&nano_state, 0, sizeof(nano_state));
    nano_state.active = 1;
    nano_state.term = term;
    strncpy(nano_state.path, path, sizeof(nano_state.path) - 1);
    nano_state.path[sizeof(nano_state.path) - 1] = '\0';
    nano_state.cursor = 0;
    nano_state.dirty = 0;
    nano_state.prompt_state = NANO_PROMPT_NONE;
    
    if (nano_load_file(term, nano_state.path) != SHELL_OK) {
        nano_state.active = 0;
        shell_error(term, "nano: %s: unable to read file", path);
        return SHELL_ERR;
    }
    
    nano_render();
    return SHELL_OK;
}

