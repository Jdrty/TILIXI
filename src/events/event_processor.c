#include "event_processor.h"
#include "hotkey.h"
#include "terminal.h"
#include "debug_helper.h"
#include <stdio.h>

void process(key_event evt) {
    // first check for hotkeys
    const char *action = find_hotkey_action(evt);
    if (action != NULL) {
        keyboard_event ke = {event_hotkey, evt.key, evt.modifiers, action};
        push_event(&ke);
        execute_action(action);
        DEBUG_PRINT("[KEY] hotkey triggered! %s\n", ke.action);
        fflush(stdout);
        return;
    }
    
    // if no hotkey, send to active terminal for input
    terminal_state *term = get_active_terminal();
    if (term != NULL && term->active) {
        terminal_handle_key_event(evt);
    }
}

