#include "../../../include/terminal.h"

terminal_state terminals[max_windows];
uint8_t active_terminal = 0;
uint8_t window_count = 0;

void new_terminal(void) {
    if (window_count >= max_windows) {
        printf("[LIMIT] Max terminal count\n");
        return;
    }
    window_count++;
    printf("[ACTION] Terminal opened count is %d\n", window_count);
}

void close_terminal(void) {
    if (window_count == 0) {
        printf("[ERROR] No terminals to close\n");
        return;
    }
    window_count--;
    printf("[ACTION] Terminal closed count is %d\n", window_count);
}                                                                                          
