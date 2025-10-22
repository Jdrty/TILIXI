#include "action_manager.h"

#define max_actions 32
static action actions[max_actions];
static uint8_t action_count = 0;

void register_action(const char *name, action_handler handler) {
    if (action_count < max_actions) {
        actions[action_count++] = (action) {name, handler};
        printf("[ACTION] Registered: %s\n", name);
    }
}

void execute_action(const char *name) {
    for (uint8_t i = 0; i < action_count; i++) {
        if (strcmp(actions[i].name, name) == 0) {
            actions[i].handler();   // assign the function pointer the action
            return;
        }
    }
    printf("[ACTION?] Unknown action %s\n", name);
}

void init_actions(void) {
    register_action("terminal", new_terminal);
    register_action("close_terminal", close_terminal);
    // and so on
}
