#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_kill(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_kill_def = {
    .name = "kill",
    .handler = cmd_kill,
    .help = "Kill process"
};

int cmd_kill(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    (void)argv;
    
    if (argc < 2) {
        shell_error(term, "kill: missing process ID");
        return SHELL_EINVAL;
    }
    
    if (argc > 2) {
        shell_error(term, "kill: too many arguments");
        return SHELL_EINVAL;
    }
    
    // TODO: implement kill functionality
    terminal_write_string(term, "kill: not implemented\n");
    return SHELL_ERR;
}

