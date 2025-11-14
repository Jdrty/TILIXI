#pragma once
#include <stdint.h>
#include "keyboard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// what kind of events can exist
typedef enum __attribute__ ((packed)) {
    event_none,
    event_keypressed,
    event_hotkey
} event_type;

// define event characteristics
typedef struct {
    event_type event;
    key_code key;
    uint8_t modifiers;
    const char *action;
} keyboard_event;

// Queue functions
void push_event(const keyboard_event *evt);
keyboard_event get_next_event(void);

#ifdef __cplusplus
}
#endif

