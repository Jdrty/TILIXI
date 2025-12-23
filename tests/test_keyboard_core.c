#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "keyboard_core.h"
#include "event_processor.h"
#include "event_queue.h"
#include "hotkey.h"
#include "action_manager.h"

static int test_action_calls = 0;

static void test_action_handler(void) {
    test_action_calls++;
}

void setup_keyboard(void) {
    // ensure a clean state for each test
    reset_event_queue();
    reset_hotkeys();
    reset_actions();
    test_action_calls = 0;
}

void teardown_keyboard(void) {
    // nothing to clean up yet
}

// test 1: register single hotkey
void test_hotkey_register(void) {
    setup_keyboard();
    printf("  test_hotkey_register... ");
    
    // register an action and a hotkey that should trigger it
    register_action("test_action", test_action_handler);
    register_key(mod_shift, key_a, "test_action");
    
    // press Shift+A - should trigger hotkey and queue an event
    key_event evt = {key_a, mod_shift};
    process(evt);
    
    keyboard_event queued = get_next_event();
    assert(queued.event == event_hotkey);
    assert(queued.key == key_a);
    assert(queued.modifiers == mod_shift);
    assert(queued.action != NULL);
    assert(strcmp(queued.action, "test_action") == 0);
    assert(test_action_calls == 1);
    
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
    process(evt);  // should not crash and should not queue any events
    
    keyboard_event result = get_next_event();
    assert(result.event == event_none);
    assert(result.key == key_none);
    
    printf("FUNCTIONAL\n");
    teardown_keyboard();
}

// test 4: hotkey matching with correct modifiers
void test_hotkey_match_with_modifiers(void) {
    setup_keyboard();
    printf("  test_hotkey_match_with_modifiers... ");
    
    register_action("shift_a", test_action_handler);
    register_key(mod_shift, key_a, "shift_a");
    
    // pressing 'a' without shift should not match
    key_event evt1 = {key_a, 0};
    process(evt1);
    
    keyboard_event result1 = get_next_event();
    assert(result1.event == event_none);  // no hotkey triggered
    assert(test_action_calls == 0);
    
    // pressing Shift+'a' should match and trigger the hotkey
    key_event evt2 = {key_a, mod_shift};
    process(evt2);
    
    keyboard_event result2 = get_next_event();
    assert(result2.event == event_hotkey);
    assert(result2.key == key_a);
    assert(result2.modifiers == mod_shift);
    assert(result2.action != NULL);
    assert(strcmp(result2.action, "shift_a") == 0);
    assert(test_action_calls == 1);
    
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
