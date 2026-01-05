#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_shutdown(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_shutdown_def = {
    .name = "shutdown",
    .handler = cmd_shutdown,
    .help = "Shutdown system"
};

int cmd_shutdown(terminal_state *term, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    terminal_write_string(term, "shutdown: not implemented\n");
    return SHELL_ERR;
}

