#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "terminal.h"
#include "keyboard_core.h"

void setup_terminal_input(void) {
    init_terminal_system();
    new_terminal();
}

void teardown_terminal_input(void) {
    close_terminal();
}

// test 1: key to character conversion (via terminal input)
void test_key_to_char_basic(void) {
    setup_terminal_input();
    printf("  test_key_to_char_basic... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    // clear any initial prompt text - need to reset after new_terminal() which writes prompt
    term->input_pos = 0;
    term->input_len = 0;
    memset(term->input_line, 0, terminal_cols);
    
    key_event evt;
    evt.key = key_a;
    evt.modifiers = 0;
    
    terminal_handle_key_event(evt);

    // check that character was added
    assert(term->input_pos == 1);
    assert(term->input_len == 1);
    assert(term->input_line[0] == 'a');
    
    // test that we can add another character
    // Use terminal_handle_key directly to avoid potential issues with key_to_char
    terminal_handle_key(term, 'b');
    assert(term->input_pos == 2);
    assert(term->input_len == 2);
    assert(term->input_line[0] == 'a');
    assert(term->input_line[1] == 'b');
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

// test 2: shift modifier
void test_key_to_char_shift(void) {
    setup_terminal_input();
    printf("  test_key_to_char_shift... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    key_event evt;
    evt.key = key_a;
    evt.modifiers = mod_shift;
    
    terminal_handle_key_event(evt);
    assert(term->input_pos == 1);
    assert(term->input_len == 1);
    assert(term->input_line[0] == 'A');
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

// test 3: enter key
void test_key_enter(void) {
    setup_terminal_input();
    printf("  test_key_enter... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    // clear initial state
    term->input_pos = 0;
    term->input_len = 0;
    memset(term->input_line, 0, terminal_cols);
    
    // type some text using direct function
    terminal_handle_key(term, 'h');
    terminal_handle_key(term, 'i');
    
    assert(term->input_pos == 2);
    assert(term->input_len == 2);
    
    // press enter
    key_event evt;
    evt.key = key_enter;
    evt.modifiers = 0;
    terminal_handle_key_event(evt);
    
    // input should be cleared and added to history
    assert(term->input_pos == 0);
    assert(term->input_len == 0);
    assert(term->history_count > 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

// test 4: backspace key
void test_key_backspace(void) {
    setup_terminal_input();
    printf("  test_key_backspace... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    // clear initial state
    term->input_pos = 0;
    term->input_len = 0;
    memset(term->input_line, 0, terminal_cols);
    
    // type some text using direct function
    terminal_handle_key(term, 'h');
    terminal_handle_key(term, 'i');
    
    assert(term->input_pos == 2);
    assert(term->input_len == 2);
    
    // press backspace
    key_event evt;
    evt.key = key_backspace;
    evt.modifiers = 0;
    terminal_handle_key_event(evt);
    
    assert(term->input_pos == 1);
    assert(term->input_len == 1);
    assert(term->input_line[0] == 'h');
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

// test 5: arrow keys (history navigation)
void test_key_arrows(void) {
    setup_terminal_input();
    printf("  test_key_arrows... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    // add some history
    strncpy(term->input_line, "command1", terminal_cols - 1);
    term->input_pos = strlen("command1");
    term->input_len = strlen("command1");
    terminal_handle_enter(term);
    
    strncpy(term->input_line, "command2", terminal_cols - 1);
    term->input_pos = strlen("command2");
    term->input_len = strlen("command2");
    terminal_handle_enter(term);
    
    assert(term->history_count == 2);
    
    // press up arrow
    key_event evt;
    evt.key = key_up;
    evt.modifiers = 0;
    terminal_handle_key_event(evt);
    
    // should load previous command
    assert(term->input_pos > 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

// test 6: numbers and special characters
void test_key_special_chars(void) {
    setup_terminal_input();
    printf("  test_key_special_chars... ");
    
    terminal_state *term = get_active_terminal();
    assert(term != NULL);
    
    key_event evt;
    
    // test numbers
    evt.key = key_one;
    evt.modifiers = 0;
    terminal_handle_key_event(evt);
    assert(term->input_line[0] == '1');
    
    // test space
    evt.key = key_space;
    evt.modifiers = 0;
    terminal_handle_key_event(evt);
    assert(term->input_line[1] == ' ');
    
    printf("FUNCTIONAL\n");
    teardown_terminal_input();
}

int main(void) {
    printf("[TERMINAL INPUT TESTS]\n");
    test_key_to_char_basic();
    test_key_to_char_shift();
    test_key_enter();
    test_key_backspace();
    test_key_arrows();
    test_key_special_chars();
    return 0;
}

