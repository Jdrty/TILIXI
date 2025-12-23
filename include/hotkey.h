#pragma once
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// register a hotkey
void register_key(uint8_t modifiers, key_code key, const char *action);

// clear all registered hotkeys (primarily for tests)
void reset_hotkeys(void);

// check if a key event matches a registered hotkey and return the action name
// returns NULL if no match found
const char *find_hotkey_action(key_event evt);

#ifdef __cplusplus
}
#endif

