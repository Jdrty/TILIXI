#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include "keyboard_pc.h"

// modifier masks (matching keyboard_core.h)
#define mod_ctrl  (1 << 1)
#define mod_shift (1 << 0)

// enable raw terminal input
static void enable_raw_mode(struct termios *orig) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);  // disable line buffering and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// restore terminal to previous mode
static void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSANOW, orig);
}

// convert ASCII char to key_code
static key_code key_from_char(char c) {
    if (c >= 'a' && c <= 'z') return key_a + (c - 'a');
    if (c >= 'A' && c <= 'Z') return key_a + (c - 'A'); // shift handled separately
    if (c == '\n') return key_enter;
    if (c == 27) return key_esc;
    if (c == ' ') return key_space;
    return key_none;
}

// "sdlread" replacement for terminal input
void sdlread(void) {
    struct termios orig;
    enable_raw_mode(&orig);

    fflush(stdout);

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n != 1) continue;

        key_event evt;
        evt.modifiers = 0;

        if ((unsigned char)c <= 26 && c != '\n') {
            evt.modifiers |= mod_ctrl;
            c += 'a' - 1; // convert back to lowercase
        }

        if (c >= 'A' && c <= 'Z') {
            evt.modifiers |= mod_shift;
        }

        evt.key = key_from_char(c);

        printf("[DEBUG] Key pressed: key=%d, mods=%d\n", evt.key, evt.modifiers);
        fflush(stdout);

        process(evt);
    }

    disable_raw_mode(&orig);
}
