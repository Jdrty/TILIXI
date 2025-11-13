#include <stdio.h>
#include "../include/keyboard_pc.h"
#include "../include/action_manager.h"

int main(void) {
    init_actions();
    register_key(mod_shift, key_a, "terminal");
    register_key(mod_shift, key_d, "close_terminal");
    sdlread();
    return 0;
}
