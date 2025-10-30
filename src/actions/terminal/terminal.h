#pragma once
#include <stdint.h>
#include <stdio.h>
#include "terminal_cmd.h"

// terminal char dimensions
#define terminal_rows 24
#define terminal_cols 80
#define terminal_buffer_size (terminal_rows * terminal_cols)

typedef struct {
    char buffer[terminal_buffer_size];  // whats being displayed
    char input_line[terminal_cols];
    uint8_t cursor_row;
    uint8_t cursor_col;
    uint8_t active;     // is terminal in use
    uint8_t input_pos;  // cursor position in input
} terminal_state;

void new_terminal(void);
void close_terminal(void);
