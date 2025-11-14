#pragma once
#include "keyboard_core.h"
#include "event_queue.h"
#include "action_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// Process a key event (check for hotkeys, queue events, etc.)
void process(key_event evt);

#ifdef __cplusplus
}
#endif

