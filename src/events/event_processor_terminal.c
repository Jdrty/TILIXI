#include "event_processor.h"
#include "terminal.h"
#include "ino_helper.h"
#include "debug_helper.h"
#include <stdio.h>

// process terminal input events
void event_process_terminal(key_event evt) {
    // if no hotkey, send to active terminal for input
    terminal_state *term = get_active_terminal();
    
    // debug output for Arduino builds is handled in keyboard_esp.cpp (has Serial access)
    // for PC builds, use DEBUG_PRINT
#ifndef ARDUINO
    if (term == NULL) {
        DEBUG_PRINT("[TERMINAL] event_process_terminal: no active terminal\n");
        return;
    }
    if (!term->active) {
        DEBUG_PRINT("[TERMINAL] event_process_terminal: terminal exists but not active\n");
        return;
    }
    DEBUG_PRINT("[TERMINAL] event_process_terminal: sending key to terminal (key=%d, mods=0x%02x)\n",
                evt.key, evt.modifiers);
#endif
    
    if (term != NULL && term->active) {
        terminal_handle_key_event(evt);
    }
}



