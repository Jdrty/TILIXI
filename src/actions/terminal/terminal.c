#include "terminal.h"
#include "terminal_cmd.h"
#include "process.h"
#include "process_script.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ARDUINO
    // ESP32: use simple file descriptors (stubs for now, one day, one day...)
    #define PIPE_READ  0
    #define PIPE_WRITE 1
#else
    // PC: use actual pipes
    #include <unistd.h>
    #include <sys/types.h>
    #include <fcntl.h>
#endif

terminal_state terminals[max_windows];
uint8_t active_terminal = 0;
uint8_t window_count = 0;

void init_terminal_system(void) {
    window_count = 0;
    active_terminal = 0;
    for (uint8_t i = 0; i < max_windows; i++) {
        terminals[i].active = 0;
        terminals[i].cursor_row = 0;
        terminals[i].cursor_col = 0;
        terminals[i].input_pos = 0;
        terminals[i].history_count = 0;
        terminals[i].history_pos = 0;
        memset(terminals[i].buffer, ' ', terminal_buffer_size);
        memset(terminals[i].input_line, 0, terminal_cols);
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

void new_terminal(void) {
    if (window_count >= max_windows) {
        DEBUG_PRINT("[LIMIT] Max terminal count\n");
        return;
    }
    
    // find free terminal slot
    for (uint8_t i = 0; i < max_windows; i++) {
        if (!terminals[i].active) {
            terminals[i].active = 1;
            terminals[i].cursor_row = 0;
            terminals[i].cursor_col = 0;
            terminals[i].input_pos = 0;
            terminals[i].history_count = 0;
            terminals[i].history_pos = 0;
            memset(terminals[i].buffer, ' ', terminal_buffer_size);
            memset(terminals[i].input_line, 0, terminal_cols);
            active_terminal = i;
            window_count++;
            terminal_write_line(&terminals[i], "TILIXI Terminal v1.0");
            terminal_write_string(&terminals[i], "$ ");
            DEBUG_PRINT("[ACTION] Terminal opened count is %d\n", window_count);
            return;
        }
    }
}

void close_terminal(void) {
    if (window_count == 0) {
        DEBUG_PRINT("[ERROR] No terminals to close\n");
        return;
    }
    
    if (terminals[active_terminal].active) {
        terminals[active_terminal].active = 0;
        window_count--;
        
        // find next active terminal
        for (uint8_t i = 0; i < max_windows; i++) {
            if (terminals[i].active) {
                active_terminal = i;
                break;
            }
        }
        DEBUG_PRINT("[ACTION] Terminal closed count is %d\n", window_count);
    }
}

terminal_state *get_active_terminal(void) {
    if (window_count == 0) {
        return NULL;
    }
    return &terminals[active_terminal];
}

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

// terminal input handling
void terminal_handle_key(terminal_state *term, char key) {
    if (term == NULL || !term->active) return;
    
    if (term->input_pos < terminal_cols - 1 && key >= 32 && key < 127) {
        term->input_line[term->input_pos++] = key;
        term->input_line[term->input_pos] = '\0';
        terminal_write_char(term, key);  // echo character
    }
}

void terminal_handle_backspace(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    if (term->input_pos > 0) {
        term->input_pos--;
        term->input_line[term->input_pos] = '\0';
        if (term->cursor_col > 0) {
            term->cursor_col--;
            uint16_t pos = term->cursor_row * terminal_cols + term->cursor_col;
            if (pos < terminal_buffer_size) {
                term->buffer[pos] = ' ';
            }
        }
        terminal_write_char(term, '\b');  // backspace
        terminal_write_char(term, ' ');    // erase
        terminal_write_char(term, '\b');  // backspace again
    }
}

void terminal_handle_enter(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    terminal_newline(term);
    
    // add to history
    // theres probably a more unified way to do this that looks a lot better
    // butttttttt im too lazy and dumb...
    if (term->input_pos > 0) {
        if (term->history_count < max_input_history) {
            strncpy(term->history[term->history_count], term->input_line, terminal_cols - 1);
            term->history[term->history_count][terminal_cols - 1] = '\0';
            term->history_count++;
        } else {
            // shift history up
            for (uint8_t i = 0; i < max_input_history - 1; i++) {
                strncpy(term->history[i], term->history[i + 1], terminal_cols - 1);
                term->history[i][terminal_cols - 1] = '\0';
            }
            strncpy(term->history[max_input_history - 1], term->input_line, terminal_cols - 1);
            term->history[max_input_history - 1][terminal_cols - 1] = '\0';
        }
        term->history_pos = term->history_count;
    }
    
    // parse and execute command
    command_tokens_t tokens = terminal_parse_command(term->input_line);
    if (tokens.token_count > 0) {
        if (tokens.has_pipe) {
            terminal_execute_pipeline(term, &tokens);
        } else {
            terminal_execute_command(term, &tokens);
        }
    }
    
    // clear input line
    memset(term->input_line, 0, terminal_cols);
    term->input_pos = 0;
    
    // show prompt
    terminal_write_string(term, "$ ");
}

void terminal_handle_arrow_up(terminal_state *term) {
    if (term == NULL || !term->active) return;
    
    if (term->history_pos > 0) {
        term->history_pos--;
        strncpy(term->input_line, term->history[term->history_pos], terminal_cols - 1);
        term->input_line[terminal_cols - 1] = '\0';
        term->input_pos = strlen(term->input_line);
        
        // clear current line and redraw
        terminal_write_string(term, "\r$ ");
        terminal_write_string(term, term->input_line);
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
        term->input_pos = strlen(term->input_line);
        
        // clear current line and redraw
        terminal_write_string(term, "\r$ ");
        terminal_write_string(term, term->input_line);
    }
}

// portable tokenizer helper, finds next token in string
// returns pointer to start of token, or NULL if no more tokens
// modifies the string by null-terminating tokens
static char *next_token(char *str, const char *delimiters, char **saveptr) {
    if (str == NULL) {
        str = *saveptr;
    }
    
    if (str == NULL) {
        return NULL;
    }
    
    // skip leading delimiters
    while (*str != '\0' && strchr(delimiters, *str) != NULL) {
        str++;
    }
    
    if (*str == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    
    char *token_start = str;
    
    // find end of token
    while (*str != '\0' && strchr(delimiters, *str) == NULL) {
        str++;
    }
    
    if (*str != '\0') {
        *str = '\0';
        str++;
    }
    
    *saveptr = str;
    return token_start;
}

// command parsing
command_tokens_t terminal_parse_command(const char *input) {
    command_tokens_t result = {0};
    uint8_t storage_idx = 0;
    
    if (input == NULL || strlen(input) == 0) {
        return result;
    }
    
    // use stack-allocated buffer instead of strdup() to avoid heap allocation
    // input is already limited to terminal_cols, so this is safe
    char input_copy[terminal_cols];
    size_t input_len = strlen(input);
    if (input_len >= terminal_cols) {
        input_len = terminal_cols - 1;
    }
    memcpy(input_copy, input, input_len);
    input_copy[input_len] = '\0';
    
    // portable tokenizer (replaces strtok_r)
    char *saveptr = input_copy;
    char *token = next_token(input_copy, " \t\n", &saveptr);
    
    while (token != NULL && result.token_count < terminal_cols && storage_idx < terminal_cols) {
        // check for pipe
        if (strcmp(token, "|") == 0) {
            result.has_pipe = 1;
            result.pipe_pos = result.token_count;
        } else {
            // copy token to per-instance storage (not static)
            size_t token_len = strlen(token);
            if (token_len >= terminal_cols) {
                token_len = terminal_cols - 1;
            }
            memcpy(result.token_storage[storage_idx], token, token_len);
            result.token_storage[storage_idx][token_len] = '\0';
            result.tokens[result.token_count] = result.token_storage[storage_idx];
            result.token_count++;
            storage_idx++;
        }
        token = next_token(NULL, " \t\n", &saveptr);
    }
    
    return result;
}

// command execution
void terminal_execute_command(terminal_state *term, command_tokens_t *tokens) {
    if (term == NULL || tokens == NULL || tokens->token_count == 0) {
        return;
    }
    
    const char *cmd_name = tokens->tokens[0];
    char **argv = (char **)tokens->tokens;
    int argc = tokens->token_count;
    
    // find and execute command
    term_cmd *cmd = find_cmd(cmd_name);
    if (cmd != NULL) {
        int result = cmd->handler(argc, argv);
        if (result != 0) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Command failed with code %d\n", result);
            terminal_write_string(term, error_msg);
        }
    } else {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Command not found: %s\n", cmd_name);
        terminal_write_string(term, error_msg);
    }
}

// pipeline execution
void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens) {
    if (term == NULL || tokens == NULL || !tokens->has_pipe) {
        return;
    }
    
    // split commands at pipe
    uint8_t cmd1_end = tokens->pipe_pos;
    uint8_t cmd2_start = tokens->pipe_pos + 1;
    
    if (cmd1_end == 0 || cmd2_start >= tokens->token_count) {
        terminal_write_string(term, "Invalid pipe syntax\n");
        return;
    }
    
    // create pipe
    terminal_pipe_t *pipe = terminal_create_pipe();
    if (pipe == NULL) {
        terminal_write_string(term, "Failed to create pipe\n");
        return;
    }
    
    // execute first command (write to pipe)
    command_tokens_t cmd1_tokens = {0};
    cmd1_tokens.token_count = cmd1_end;
    for (uint8_t i = 0; i < cmd1_end; i++) {
        cmd1_tokens.tokens[i] = tokens->tokens[i];
    }
    
    // execute second command (read from pipe)
    command_tokens_t cmd2_tokens = {0};
    cmd2_tokens.token_count = tokens->token_count - cmd2_start;
    for (uint8_t i = 0; i < cmd2_tokens.token_count; i++) {
        cmd2_tokens.tokens[i] = tokens->tokens[cmd2_start + i];
    }
    
    // for now, simple sequential execution
    // later in implementation, will spawn processes and wire pipes.... fuck my lifee
    terminal_write_string(term, "[PIPE] ");
    terminal_execute_command(term, &cmd1_tokens);
    terminal_write_string(term, " | ");
    terminal_execute_command(term, &cmd2_tokens);
    terminal_newline(term);
    
    terminal_close_pipe(pipe);
}

// pipe management
terminal_pipe_t *terminal_create_pipe(void) {
    static terminal_pipe_t static_pipes[max_pipe_commands];
    static uint8_t pipe_count = 0;
    
    if (pipe_count >= max_pipe_commands) {
        return NULL;
    }
    
    terminal_pipe_t *pipe_obj = &static_pipes[pipe_count++];
    
#ifndef ARDUINO
    int fds[2];
    if (pipe(fds) == 0) {
        pipe_obj->read_fd = fds[0];
        pipe_obj->write_fd = fds[1];
        pipe_obj->active = 1;
        return pipe_obj;
    }
#endif
    
    // ESP32 stub (would use different mechanism)
    pipe_obj->read_fd = -1;
    pipe_obj->write_fd = -1;
    pipe_obj->active = 0;
    return NULL;
}

void terminal_close_pipe(terminal_pipe_t *pipe) {
    if (pipe == NULL) return;
    
#ifndef ARDUINO
    if (pipe->read_fd >= 0) {
        close(pipe->read_fd);
    }
    if (pipe->write_fd >= 0) {
        close(pipe->write_fd);
    }
#endif
    
    pipe->active = 0;
    pipe->read_fd = -1;
    pipe->write_fd = -1;
}

void terminal_wire_pipes(terminal_pipe_t *pipe1, terminal_pipe_t *pipe2) {
    // wire pipe1's output to pipe2's input
    if (pipe1 != NULL && pipe2 != NULL && pipe1->active && pipe2->active) {
        // in full implementation, would redirect file descriptors
        // for now, this is a placeholder... so many placeholders, will they ever be implimented? who knows
        (void)pipe1;
        (void)pipe2;
    }
}
