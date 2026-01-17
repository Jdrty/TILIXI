#include "action_manager.h"

// forward declaration for process actions
void register_process_actions(void);

#define max_actions 32
static action actions[max_actions];
static uint8_t action_count = 0;

void register_action(const char *name, action_handler handler) {
    if (action_count < max_actions) {
        actions[action_count++] = (action) {name, handler};
        DEBUG_PRINT("[ACTION] Registered: %s\n", name);
    }
}

// reset all registered actions (primarily for tests)
void reset_actions(void) {
    action_count = 0;
    for (uint8_t i = 0; i < max_actions; i++) {
        actions[i].name = NULL;
        actions[i].handler = NULL;
    }
}

void execute_action(const char *name) {
    for (uint8_t i = 0; i < action_count; i++) {
        if (strcmp(actions[i].name, name) == 0) {
            actions[i].handler();   // assign the function pointer the action
            return;
        }
    }
    DEBUG_PRINT("[ACTION?] Unknown action %s\n", name);
}

void init_actions(void) {
    register_action("terminal", new_terminal);
    register_action("close_terminal", close_terminal);
    register_action("select_left", terminal_select_left);
    register_action("select_right", terminal_select_right);
    register_action("select_up", terminal_select_up);
    register_action("select_down", terminal_select_down);
    
    // register process-related actions
    register_process_actions();
    
    // and so on
}
