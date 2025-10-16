#include <stdio.h>
#include "src/keyboard/pc/keyboard_pc.h"
#include "src/actions/action_manager.h"

int main(void) {
    init_actions();
    register_key(mod_shift, key_a, "terminal");
    sdlread();
    return 0;
}
