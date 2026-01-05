// main terminal file - includes all terminal sub-modules
#include "terminal.h"
#include "terminal_cmd.h"
#include "debug_helper.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

terminal_state terminals[max_windows];
uint8_t active_terminal = 0;
uint8_t window_count = 0;
uint8_t selected_terminal = 0;  // last opened terminal (for splitting) - accessed by terminal_layout.c

void init_terminal_system(void) {
    window_count = 0;
    active_terminal = 0;
    selected_terminal = 0;
    for (uint8_t i = 0; i < max_windows; i++) {
        terminals[i].active = 0;
        terminals[i].cursor_row = 0;
        terminals[i].cursor_col = 0;
        terminals[i].input_pos = 0;
        terminals[i].history_count = 0;
        terminals[i].history_pos = 0;
        terminals[i].cwd = NULL;
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
    extern command_tokens_t terminal_parse_command(const char *input);
    extern void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens);
    extern void terminal_execute_command(terminal_state *term, command_tokens_t *tokens);
    
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

extern command_tokens_t terminal_parse_command(const char *input);
extern void terminal_execute_pipeline(terminal_state *term, command_tokens_t *tokens);

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

#ifdef ARDUINO
    extern void terminal_render_all(void);
    extern void terminal_render_window(terminal_state *term);
#endif
