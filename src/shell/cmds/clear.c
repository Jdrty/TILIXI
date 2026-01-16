#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <string.h>

int cmd_clear(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_clear_def = {
    .name = "clear",
    .handler = cmd_clear,
    .help = "Clear terminal screen and history"
};

int cmd_clear(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc > 1) {
        shell_error(term, "clear: too many arguments");
        return SHELL_EINVAL;
    }
    
    // clear terminal buffer
    terminal_clear(term);
    
    // clear terminal history
    term->history_count = 0;
    term->history_pos = 0;
    for (uint8_t i = 0; i < max_input_history; i++) {
        memset(term->history[i], 0, terminal_cols);
    }
    
    return SHELL_OK;
}




