#include "terminal.h"

void new_terminal(void) {
    if (window_count >= max_windows) {
        printf("[LIMIT] Max terminal count\n");
        return;
    }
    window_count++;
    printf("[ACTION] Terminal opened count is %d\n", window_count);
}

void close_terminal(void) {
    window_count--;
    printf("[ACTION] Terminal closed count is %d\n", window_count);
}
