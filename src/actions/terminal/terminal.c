#include "terminal.h"

terminal_state terminals[max_windows];
uint8_t active_terminal = 0;
uint8_t window_count = 0;

void new_terminal(void) {
    if (window_count >= max_windows) {
        DEBUG_PRINT("[LIMIT] Max terminal count\n");
        return;
    }
    window_count++;
    DEBUG_PRINT("[ACTION] Terminal opened count is %d\n", window_count);
}

void close_terminal(void) {
    if (window_count == 0) {
        DEBUG_PRINT("[ERROR] No terminals to close\n");
        return;
    }
    window_count--;
    DEBUG_PRINT("[ACTION] Terminal closed count is %d\n", window_count);
}                                                                                          
