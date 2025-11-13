#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "include/action_manager.h"

// mock handler
static int mock_handler_calls = 0;

void mock_handler(void) {
    mock_handler_calls++;
}

void reset_test_state(void) {
    mock_handler_calls = 0;
}

void teardown(void) {
    // cleanup if needed
}

// test numero uno: basic action registration
void test_action_register_single(void) {
    reset_test_state();
    printf("  test_action_register_single... ");
    
    register_action("test", mock_handler);
    execute_action("test");
    
    assert(mock_handler_calls == 1);
    printf("FUNCTIONAL\n");
    teardown();
}

// test 2: execute non-existent action
void test_action_execute_unknown(void) {
    reset_test_state();
    printf("  test_action_execute_unknown... ");
    
    // should not crash, just print error
    execute_action("does_not_exist");
    
    assert(mock_handler_calls == 0);
    printf("FUNCTIONAL\n");
    teardown();
}

// test 3: same action called multiple times
void test_action_execute_multiple(void) {
    reset_test_state();
    printf("  test_action_execute_multiple... ");
    
    register_action("multi", mock_handler);
    execute_action("multi");
    execute_action("multi");
    execute_action("multi");
    
    assert(mock_handler_calls == 3);
    printf("FUNCTIONAL\n");
    teardown();
}

// test 4: multiple different actions
void test_action_register_multiple(void) {
    reset_test_state();
    printf("  test_action_register_multiple... ");
    
    static int handler1_calls = 0;
    static int handler2_calls = 0;
    
    void handler1(void) { handler1_calls++; }
    void handler2(void) { handler2_calls++; }
    
    register_action("action1", handler1);
    register_action("action2", handler2);
    
    execute_action("action1");
    execute_action("action2");
    execute_action("action1");
    
    assert(handler1_calls == 2);
    assert(handler2_calls == 1);
    printf("FUNCTIONAL\n");
    teardown();
}

// run all tests for this module
int main(void) {
    printf("[ACTION MANAGER TESTS]\n");
    test_action_register_single();
    test_action_execute_unknown();
    test_action_execute_multiple();
    test_action_register_multiple();
    return 0;
}
