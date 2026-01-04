#include "terminal.h"
#include "terminal_cmd.h"
#include <string.h>
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

// external functions
extern void terminal_write_string(terminal_state *term, const char *str);
extern void terminal_write_char(terminal_state *term, char c);
extern void terminal_newline(terminal_state *term);
extern void terminal_execute_command(terminal_state *term, command_tokens_t *tokens);

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



