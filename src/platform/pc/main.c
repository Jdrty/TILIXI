#include <stdio.h>
#include "platform/pc/keyboard_pc.h"
#include "action_manager.h"
#include "event_processor.h"
#include "hotkey.h"

int main(void) {
    init_actions();
    register_key(mod_shift, key_a, "terminal");
    register_key(mod_shift, key_d, "close_terminal");
    sdlread();
    return 0;
}
