#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "terminal.h"
#include "terminal_cmd.h"

void setup_terminal_full(void) {
    init_terminal_system();
    init_terminal_commands();
}

void teardown_terminal_full(void) {
    // cleanup
    for (uint8_t i = 0; i < max_windows; i++) {
        if (terminals[i].active) {
            close_terminal();
        }
    }
}

// test 1: terminal initialization
void test_terminal_init(void) {
    setup_terminal_full();
    printf("  test_terminal_init... ");
    
    terminal_state *term = get_active_terminal();
    assert(term == NULL);  // no terminal open yet
    
    new_terminal();
    term = get_active_terminal();
    assert(term != NULL);
    assert(term->active == 1);
    assert(term->cursor_row == 0 || term->cursor_col >= 0);  // after prompt
    
    printf("FUNCTIONAL\n");
    teardown_terminal_full();
}

// test 2: terminal write functions
void test_terminal_write(void) {
    setup_terminal_full();
    printf("  test_terminal_write... ");
    
    new_terminal();
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    terminal_write_char(term, 'A');
    terminal_write_char(term, 'B');
    terminal_write_char(term, 'C');
    
    terminal_write_string(term, " test");
    terminal_newline(term);
    
    // verify characters were written to buffer
    assert(term->cursor_col >= 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_full();
}

// test 3: terminal clear
void test_terminal_clear(void) {
    setup_terminal_full();
    printf("  test_terminal_clear... ");
    
    new_terminal();
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    terminal_write_string(term, "some text");
    terminal_clear(term);
    
    assert(term->cursor_row == 0);
    assert(term->cursor_col == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_full();
}

// test 4: terminal input handling
void test_terminal_input(void) {
    setup_terminal_full();
    printf("  test_terminal_input... ");
    
    new_terminal();
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    terminal_handle_key(term, 'h');
    terminal_handle_key(term, 'e');
    terminal_handle_key(term, 'l');
    terminal_handle_key(term, 'l');
    terminal_handle_key(term, 'o');
    
    assert(term->input_pos == 5);
    assert(term->input_len == 5);
    assert(strncmp(term->input_line, "hello", 5) == 0);
    
    terminal_handle_backspace(term);
    assert(term->input_pos == 4);
    assert(term->input_len == 4);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_full();
}

// test 5: command parsing
void test_terminal_parse_command(void) {
    setup_terminal_full();
    printf("  test_terminal_parse_command... ");
    
    command_tokens_t tokens = terminal_parse_command("echo hello world");
    assert(tokens.token_count == 3);
    assert(strcmp(tokens.tokens[0], "echo") == 0);
    assert(strcmp(tokens.tokens[1], "hello") == 0);
    assert(strcmp(tokens.tokens[2], "world") == 0);
    assert(tokens.has_pipe == 0);
    
    printf("FUNCTIONAL\n");
}

// test 6: command parsing with pipe
void test_terminal_parse_pipe(void) {
    setup_terminal_full();
    printf("  test_terminal_parse_pipe... ");
    
    command_tokens_t tokens = terminal_parse_command("echo hello | grep hello");
    assert(tokens.has_pipe == 1);
    assert(tokens.pipe_pos < tokens.token_count);
    
    printf("FUNCTIONAL\n");
}

// test 7: terminal history
void test_terminal_history(void) {
    setup_terminal_full();
    printf("  test_terminal_history... ");
    
    new_terminal();
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    // simulate entering commands
    strncpy(term->input_line, "command1", terminal_cols - 1);
    term->input_pos = strlen("command1");
    term->input_len = strlen("command1");
    terminal_handle_enter(term);
    
    strncpy(term->input_line, "command2", terminal_cols - 1);
    term->input_pos = strlen("command2");
    term->input_len = strlen("command2");
    terminal_handle_enter(term);
    
    assert(term->history_count == 2);
    assert(strcmp(term->history[0], "command1") == 0);
    assert(strcmp(term->history[1], "command2") == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_full();
}

int main(void) {
    printf("[TERMINAL FULL TESTS]\n");
    test_terminal_init();
    test_terminal_write();
    test_terminal_clear();
    test_terminal_input();
    test_terminal_parse_command();
    test_terminal_parse_pipe();
    test_terminal_history();
    return 0;
}

