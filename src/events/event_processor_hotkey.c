#include "event_processor.h"
#include "hotkey.h"
#include "action_manager.h"
#include "event_queue.h"
#include "debug_helper.h"
#include <stdio.h>

// process hotkey events
// returns 1 if hotkey was processed, 0 otherwise
int event_process_hotkey(key_event evt) {
    // check for hotkeys - must check BEFORE terminal input
    const char *action = find_hotkey_action(evt);
    if (action != NULL) {
        keyboard_event ke = {event_hotkey, evt.key, evt.modifiers, action};
        push_event(&ke);
        execute_action(action);
        DEBUG_PRINT("[KEY] hotkey triggered! %s\n", ke.action);
        fflush(stdout);
        return 1;  // hotkey processed
    }
    return 0;  // no hotkey
}
