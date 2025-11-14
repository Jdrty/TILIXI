#include "event_processor.h"
#include "hotkey.h"
#include "debug_helper.h"
#include <stdio.h>

void process(key_event evt) {
    const char *action = find_hotkey_action(evt);
    if (action != NULL) {
        keyboard_event ke = {event_hotkey, evt.key, evt.modifiers, action};
        push_event(&ke);
        execute_action(action);
        DEBUG_PRINT("[KEY] hotkey triggered! %s\n", ke.action);
        fflush(stdout);
    }
}

