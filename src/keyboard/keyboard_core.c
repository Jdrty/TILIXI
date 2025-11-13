// includes
#include "keyboard_core.h"

// hotkeys (:
#define max_hotkeys 16  // you could have more hotkeys but were working with limited RAM
typedef struct {
    uint8_t modifiers;
    key_code key;
    const char *action;
} hotkey;
static hotkey hotkeys[max_hotkeys];
static uint8_t hotkey_count = 0;

// queue system
#define queue_size 32
static keyboard_event event_queue[queue_size];  // array representing the queue
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;  // if you're a reader trying to understand this, think of it as a snake

// system to push next event into queue assuming queue has room
static inline void push_event(const keyboard_event *evt) {
    uint8_t next = (uint8_t) (queue_head+1);    // wrap around logic, don't want 8 bit integer being promoted to higher values unnecessarily
    if (next == queue_size) {
        next = 0;
    }

    if (next == queue_tail) {   // full
        return;
    }

    event_queue[queue_head] = *evt;
    queue_head              = next;
}

keyboard_event get_next_event(void) {
    if (queue_tail == queue_head) {
        return (keyboard_event) {event_none, key_none, 0, NULL};    // if it caught up then no events in queue
    }   

    keyboard_event evt = event_queue[queue_tail];
    queue_tail = (queue_tail + 1) % queue_size;
    return evt;
}

void process(key_event evt) {
    if (evt.modifiers) {
        for (uint8_t i = 0; i < hotkey_count; i++) {
            if (hotkeys[i].key == evt.key && hotkeys[i].modifiers == evt.modifiers) {
                keyboard_event ke = {event_hotkey,evt.key,evt.modifiers,hotkeys[i].action};
                push_event(&ke);
                execute_action(hotkeys[i].action);
                DEBUG_PRINT("[KEY] hotkey triggered! %s\n", ke.action);
                fflush(stdout);
                return;
            }
        }
    }
}

// how user registers a new hotkey
void register_key(uint8_t modifiers, key_code key, const char *action) {
    if (max_hotkeys > hotkey_count) {
        hotkeys[hotkey_count++] = (hotkey){ modifiers, key, action }; 
    }
}
