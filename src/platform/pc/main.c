#include <stdio.h>
#include "platform/pc/keyboard_pc.h"
#include "action_manager.h"
#include "event_processor.h"
#include "hotkey.h"
#include "process.h"
#include "process_scheduler.h"
#include "process_script.h"
#include "builtins.h"

int main(void) {
    // initialize process system
    init_process_system();
    init_scheduler();
    init_script_system();
    
    // initialize terminal system
    init_terminal_system();
    init_terminal_commands();
    builtins_init();
    
    init_actions();
    register_key(mod_ctrl, key_enter, "terminal");
    register_key(mod_ctrl, key_q, "close_terminal");
    sdlread();
    return 0;
}
