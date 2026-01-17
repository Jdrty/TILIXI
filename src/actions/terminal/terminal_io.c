#include "terminal.h"
#include <string.h>
#include <stdlib.h>

// terminal I/O functions
typedef struct {
    int active;
    char *buffer;
    size_t length;
    size_t capacity;
} terminal_capture_t;

static terminal_capture_t terminal_capture = {0};

void terminal_capture_start(void) {
    terminal_capture.active = 1;
    terminal_capture.length = 0;
    if (terminal_capture.buffer == NULL) {
        terminal_capture.capacity = 256;
        terminal_capture.buffer = (char*)malloc(terminal_capture.capacity);
        if (terminal_capture.buffer != NULL) {
            terminal_capture.buffer[0] = '\0';
        }
    } else {
        terminal_capture.buffer[0] = '\0';
    }
}

char *terminal_capture_stop(size_t *out_len) {
    terminal_capture.active = 0;
    if (out_len != NULL) {
        *out_len = terminal_capture.length;
    }
    if (terminal_capture.buffer == NULL) {
        return NULL;
    }
    
    char *result = (char*)malloc(terminal_capture.length + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, terminal_capture.buffer, terminal_capture.length);
    result[terminal_capture.length] = '\0';
    return result;
}

int terminal_capture_is_active(void) {
    return terminal_capture.active;
}

static void terminal_capture_append(char c) {
    if (terminal_capture.buffer == NULL) {
        return;
    }
    if (terminal_capture.length + 1 >= terminal_capture.capacity) {
        size_t new_capacity = terminal_capture.capacity * 2;
        char *new_buf = (char*)realloc(terminal_capture.buffer, new_capacity);
        if (new_buf == NULL) {
            return;
        }
        terminal_capture.buffer = new_buf;
        terminal_capture.capacity = new_capacity;
    }
    terminal_capture.buffer[terminal_capture.length++] = c;
    terminal_capture.buffer[terminal_capture.length] = '\0';
}
void terminal_write_char(terminal_state *term, char c) {
    if (term == NULL || !term->active) return;
    
    if (terminal_capture.active) {
        terminal_capture_append(c);
        return;
    }
    
    if (term->cursor_row >= terminal_rows) {
        // scroll buffer up
        memmove(term->buffer, term->buffer + terminal_cols, 
                (terminal_rows - 1) * terminal_cols);
        memset(term->buffer + (terminal_rows - 1) * terminal_cols, ' ', terminal_cols);
        term->cursor_row = terminal_rows - 1;
    }
    
    if (c == '\n') {
        term->cursor_col = 0;
        term->cursor_row++;
    } else {
        uint16_t pos = term->cursor_row * terminal_cols + term->cursor_col;
        if (pos < terminal_buffer_size) {
            term->buffer[pos] = c;
            term->cursor_col++;
            if (term->cursor_col >= terminal_cols) {
                term->cursor_col = 0;
                term->cursor_row++;
            }
        }
    }
}

void terminal_write_string(terminal_state *term, const char *str) {
    if (term == NULL || str == NULL) return;
    while (*str) {
        terminal_write_char(term, *str++);
    }
}

void terminal_write_line(terminal_state *term, const char *str) {
    terminal_write_string(term, str);
    terminal_newline(term);
}

void terminal_newline(terminal_state *term) {
    terminal_write_char(term, '\n');
}

void terminal_clear(terminal_state *term) {
    if (term == NULL) return;
    memset(term->buffer, ' ', terminal_buffer_size);
    term->cursor_row = 0;
    term->cursor_col = 0;
}



