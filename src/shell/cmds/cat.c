#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_cat(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_cat_def = {
    .name = "cat",
    .handler = cmd_cat,
    .help = "Display file contents"
};

int cmd_cat(terminal_state *term, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    terminal_write_string(term, "cat: not implemented\n");
    return SHELL_ERR;
}

