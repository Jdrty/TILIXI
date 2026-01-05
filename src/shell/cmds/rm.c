#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_rm(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_rm_def = {
    .name = "rm",
    .handler = cmd_rm,
    .help = "Remove file or directory"
};

int cmd_rm(terminal_state *term, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    terminal_write_string(term, "rm: not implemented\n");
    return SHELL_ERR;
}

