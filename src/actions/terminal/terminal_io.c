#include "terminal.h"
#include <string.h>

// terminal I/O functions
void terminal_write_char(terminal_state *term, char c) {
    if (term == NULL || !term->active) return;
    
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
