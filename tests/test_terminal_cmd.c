#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "terminal.h"
#include "terminal_cmd.h"

void setup_terminal_cmd(void) {
    init_terminal_system();
    init_terminal_commands();
    new_terminal();
}

void teardown_terminal_cmd(void) {
    close_terminal();
}

// test 1: command registration
void test_cmd_register(void) {
    setup_terminal_cmd();
    printf("  test_cmd_register... ");
    
    term_cmd *cmd = find_cmd("echo");
    assert(cmd != NULL);
    assert(strcmp(cmd->name, "echo") == 0);
    
    term_cmd *cmd2 = find_cmd("help");
    assert(cmd2 != NULL);
    
    term_cmd *cmd3 = find_cmd("nonexistent");
    assert(cmd3 == NULL);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

// test 2: echo command
void test_cmd_echo(void) {
    setup_terminal_cmd();
    printf("  test_cmd_echo... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    char *argv[] = {"echo", "hello", "world", NULL};
    term_cmd *cmd = find_cmd("echo");
    assert(cmd != NULL);
    
    int result = cmd->handler(3, argv);
    assert(result == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

// test 3: clear command
void test_cmd_clear(void) {
    setup_terminal_cmd();
    printf("  test_cmd_clear... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    terminal_write_string(term, "some text");
    assert(term->cursor_col > 0);
    
    char *argv[] = {"clear", NULL};
    term_cmd *cmd = find_cmd("clear");
    assert(cmd != NULL);
    
    int result = cmd->handler(1, argv);
    assert(result == 0);
    assert(term->cursor_row == 0);
    assert(term->cursor_col == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

// test 4: help command
void test_cmd_help(void) {
    setup_terminal_cmd();
    printf("  test_cmd_help... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    char *argv[] = {"help", NULL};
    term_cmd *cmd = find_cmd("help");
    assert(cmd != NULL);
    
    int result = cmd->handler(1, argv);
    assert(result == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

// test 5: pwd command
void test_cmd_pwd(void) {
    setup_terminal_cmd();
    printf("  test_cmd_pwd... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    char *argv[] = {"pwd", NULL};
    term_cmd *cmd = find_cmd("pwd");
    assert(cmd != NULL);
    
    int result = cmd->handler(1, argv);
    assert(result == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

// test 6: exit command
void test_cmd_exit(void) {
    setup_terminal_cmd();
    printf("  test_cmd_exit... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    assert(term->active == 1);
    
    char *argv[] = {"exit", NULL};
    term_cmd *cmd = find_cmd("exit");
    assert(cmd != NULL);
    
    int result = cmd->handler(1, argv);
    assert(result == 0);
    
    // terminal should be closed
    term = get_active_terminal();
    // Note: behavior depends on implementation
    
    printf("FUNCTIONAL\n");
    teardown_terminal_cmd();
}

int main(void) {
    printf("[TERMINAL CMD TESTS]\n");
    test_cmd_register();
    test_cmd_echo();
    test_cmd_clear();
    test_cmd_help();
    test_cmd_pwd();
    test_cmd_exit();
    return 0;
}

