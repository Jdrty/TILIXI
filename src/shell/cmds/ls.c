#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_ls(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_ls_def = {
    .name = "ls",
    .handler = cmd_ls,
    .help = "List directory contents"
};

int cmd_ls(terminal_state *term, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    terminal_write_string(term, "ls: not implemented\n");
    return SHELL_ERR;
}

