// main event processor file - coordinates event routing
#include "event_processor.h"
#include "keyboard_core.h"

// forward declarations from sub-modules
extern int event_process_hotkey(key_event evt);
extern void event_process_terminal(key_event evt);

// main event processing function
// routes events to appropriate handlers (hotkeys first, then terminal)
void process(key_event evt) {
    // first check for hotkeys - must check BEFORE terminal input
    if (event_process_hotkey(evt)) {
        // IMPORTANT: return early to prevent hotkey from also being sent to terminal
        return;
    }
    
    // if no hotkey, send to terminal
    event_process_terminal(evt);
}
