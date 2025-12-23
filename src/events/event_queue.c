#include "event_queue.h"
#include <stddef.h>

// queue system
#define queue_size 32
static keyboard_event event_queue[queue_size];  // array representing the queue
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;  // its a snake!

// reset the event queue (used by tests and initialization)
void reset_event_queue(void) {
    queue_head = 0;
    queue_tail = 0;
}

// system to push next event into queue assuming queue has room
void push_event(const keyboard_event *evt) {
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

