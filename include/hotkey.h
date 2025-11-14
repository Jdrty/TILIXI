#pragma once
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register a hotkey
void register_key(uint8_t modifiers, key_code key, const char *action);

// Check if a key event matches a registered hotkey and return the action name
// Returns NULL if no match found
const char *find_hotkey_action(key_event evt);

#ifdef __cplusplus
}
#endif

