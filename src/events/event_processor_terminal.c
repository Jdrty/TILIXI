#include "event_processor.h"
#include "terminal.h"

// process terminal input events
void event_process_terminal(key_event evt) {
    // if no hotkey, send to active terminal for input
    terminal_state *term = get_active_terminal();
    if (term != NULL && term->active) {
        terminal_handle_key_event(evt);
    }
}



