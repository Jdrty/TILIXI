#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_kill(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_kill_def = {
    .name = "kill",
    .handler = cmd_kill,
    .help = "Kill process"
};

int cmd_kill(terminal_state *term, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    terminal_write_string(term, "kill: not implemented\n");
    return SHELL_ERR;
}

