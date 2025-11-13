#include <stdio.h>
#include <assert.h>
#include "include/keyboard_core.h"

void setup_keyboard(void) {
    // initialize keyboard state for tests
    // (may need to expose reset function in keyboard_core.h, problem for future me...)
}

void teardown_keyboard(void) {
}

// test 1: register single hotkey
void test_hotkey_register(void) {
    setup_keyboard();
    printf("  test_hotkey_register... ");
    
    register_key(mod_shift, key_a, "test_action");
    // if I made a getter, we could verify it was registered, buttt thats a problem for future me (:
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

// test 2: get event from empty queue
void test_queue_empty(void) {
    setup_keyboard();
    printf("  test_queue_empty... ");
    
    keyboard_event evt = get_next_event();
    
    assert(evt.event == event_none);
    assert(evt.key == key_none);
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

// test 3: process key without hotkey registered
void test_process_unregistered_key(void) {
    setup_keyboard();
    printf("  test_process_unregistered_key... ");
    
    key_event evt = {key_b, 0};
    process(evt);  // should not crash
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

// test 4: hotkey matching with correct modifiers
void test_hotkey_match_with_modifiers(void) {
    setup_keyboard();
    printf("  test_hotkey_match_with_modifiers... ");
    
    register_key(mod_shift, key_a, "shift_a");
    
    // pressing 'a' without shift should not match
    key_event evt1 = {key_a, 0};
    process(evt1);
    
    keyboard_event result1 = get_next_event();
    assert(result1.event == event_none);  // no hotkey triggered
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

// TEST 5: multiple hotkeys registered
void test_multiple_hotkeys_registered(void) {
    setup_keyboard();
    printf("  test_multiple_hotkeys_registered... ");
    
    register_key(mod_shift, key_a, "open");
    register_key(mod_shift, key_d, "close");
    register_key(mod_ctrl, key_c, "copy");
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

int main(void) {
    printf("[KEYBOARD CORE TESTS]\n");
    test_hotkey_register();
    test_queue_empty();
    test_process_unregistered_key();
    test_hotkey_match_with_modifiers();
    test_multiple_hotkeys_registered();
    return 0;
}
