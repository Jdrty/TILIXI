#include "shell_error.h"
#include "terminal.h"
#include <stdarg.h>
#include <stdio.h>

void shell_error(terminal_state *term, const char *fmt, ...) {
    if (term == NULL || fmt == NULL) {
        return;
    }
    
    char error_msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(error_msg, sizeof(error_msg), fmt, args);
    va_end(args);
    
    terminal_write_string(term, error_msg);
    size_t len = strlen(error_msg);
    if (len == 0 || error_msg[len - 1] != '\n') {
        terminal_newline(term);
    }
}

