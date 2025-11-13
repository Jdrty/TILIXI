#include <stdio.h>
#include <assert.h>
#include "include/terminal.h"

void setup_terminal(void) {
    window_count = 0;  // reset to known state
}

void teardown_terminal(void) {
}

// test 1: Open one terminal
void test_terminal_new_single(void) {
    setup_terminal();
    printf("  test_terminal_new_single... ");
    
    assert(window_count == 0);
    new_terminal();
    assert(window_count == 1);
    
    printf("FUNCTIONAL\n");
    teardown_terminal();
}

// test 2: close one terminal
void test_terminal_close_single(void) {
    setup_terminal();
    printf("  test_terminal_close_single... ");
    
    new_terminal();
    assert(window_count == 1);
    close_terminal();
    assert(window_count == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal();
}

// test 3: cannot close when already at 0
void test_terminal_close_when_empty(void) {
    setup_terminal();
    printf("  test_terminal_close_when_empty... ");
    
    assert(window_count == 0);
    close_terminal();
    assert(window_count == 0);  // should stay at 0, not go negative
    
    printf("FUNCTIONAL\n");
    teardown_terminal();
}

// test 4: reach max window limit
void test_terminal_max_windows(void) {
    setup_terminal();
    printf("  test_terminal_max_windows... ");
    
    for (int i = 0; i < max_windows; i++) {
        new_terminal();
    }
    assert(window_count == max_windows);
    
    // try to open one more
    new_terminal();
    assert(window_count == max_windows);  // should not exceed max
    
    printf("FUNCTIONAL\n");
    teardown_terminal();
}

// test 5: multiple opens and closes
void test_terminal_multiple_operations(void) {
    setup_terminal();
    printf("  test_terminal_multiple_operations... ");
    
    new_terminal();
    new_terminal();
    assert(window_count == 2);
    
    close_terminal();
    assert(window_count == 1);
    
    new_terminal();
    assert(window_count == 2);
    
    close_terminal();
    close_terminal();
    assert(window_count == 0);
    
    printf("FUNCTIONAL\n");
    teardown_terminal();
}

int main(void) {
    printf("[TERMINAL TESTS]\n");
    test_terminal_new_single();
    test_terminal_close_single();
    test_terminal_close_when_empty();
    test_terminal_max_windows();
    test_terminal_multiple_operations();
    return 0;
}
