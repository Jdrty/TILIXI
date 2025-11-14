#include "hotkey.h"
#include <stddef.h>

// hotkeys (:
#define max_hotkeys 16  // you could have more hotkeys but were working with limited RAM
typedef struct {
    uint8_t modifiers;
    key_code key;
    const char *action;
} hotkey;
static hotkey hotkeys[max_hotkeys];
static uint8_t hotkey_count = 0;

// how user registers a new hotkey
void register_key(uint8_t modifiers, key_code key, const char *action) {
    if (max_hotkeys > hotkey_count) {
        hotkeys[hotkey_count++] = (hotkey){ modifiers, key, action }; 
    }
}

// Check if a key event matches a registered hotkey and return the action name
// Returns NULL if no match found
const char *find_hotkey_action(key_event evt) {
    if (evt.modifiers) {
        for (uint8_t i = 0; i < hotkey_count; i++) {
            if (hotkeys[i].key == evt.key && hotkeys[i].modifiers == evt.modifiers) {
                return hotkeys[i].action;
            }
        }
    }
    return NULL;
}

