#pragma once
#include <stdint.h>
#include "terminal_cmd.h"
#include "debug_helper.h"
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// terminal char dimensions
#define max_windows 8
#define terminal_rows 24
#define terminal_cols 80
#define terminal_buffer_size (terminal_rows * terminal_cols)
#define max_input_history 16
#define max_pipe_commands 8

// pipe structure for command chaining
typedef struct {
    int read_fd;   // file descriptor for reading
    int write_fd;  // file descriptor for writing
    uint8_t active;
} terminal_pipe_t;

// command token structure
typedef struct {
    char *tokens[terminal_cols];  // array of token pointers
    uint8_t token_count;
    uint8_t has_pipe;  // 1 if command contains pipe
    uint8_t pipe_pos;  // position of pipe in tokens
} command_tokens_t;

typedef struct {
    char buffer[terminal_buffer_size];  // whats being displayed
    char input_line[terminal_cols];
    char history[max_input_history][terminal_cols];  // command history
    uint8_t history_count;
    uint8_t history_pos;  // current position in history
    uint8_t cursor_row;
    uint8_t cursor_col;
    uint8_t active;     // is terminal in use
    uint8_t input_pos;  // cursor position in input
    terminal_pipe_t pipes[max_pipe_commands];  // pipes for command chaining
} terminal_state;

// expose for testing
extern uint8_t window_count;
extern terminal_state terminals[max_windows];
extern uint8_t active_terminal;

// terminal lifecycle
void new_terminal(void);
void close_terminal(void);
void init_terminal_system(void);
terminal_state *get_active_terminal(void);

// terminal I/O
void terminal_write_char(terminal_state *term, char c);
void terminal_write_string(terminal_state *term, const char *str);
void terminal_write_line(terminal_state *term, const char *str);
void terminal_clear(terminal_state *term);
void terminal_newline(terminal_state *term);

// terminal input
void terminal_handle_key(terminal_state *term, char key);
void terminal_handle_backspace(terminal_state *term);
void terminal_handle_enter(terminal_state *term);
void terminal_handle_arrow_up(terminal_state *term);
void terminal_handle_arrow_down(terminal_state *term);

// command parsing and execution
command_tokens_t terminal_parse_command(const char *input);
void terminal_execute_command(terminal_state *term, command_tokens_t *tokens);
void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens);

// pipe management
terminal_pipe_t *terminal_create_pipe(void);
void terminal_close_pipe(terminal_pipe_t *pipe);
void terminal_wire_pipes(terminal_pipe_t *pipe1, terminal_pipe_t *pipe2);

// keyboard input handling
void terminal_handle_key_event(key_event evt);

#ifdef __cplusplus
}
#endif
