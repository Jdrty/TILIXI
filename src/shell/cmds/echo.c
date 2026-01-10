#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"

int cmd_echo(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_echo_def = {
    .name = "echo",
    .handler = cmd_echo,
    .help = "Echo arguments"
};

int cmd_echo(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    // echo accepts any number of arguments (0+), so no validation needed
    for (int i = 1; i < argc; i++) {
        if (argv[i] != NULL) {
            terminal_write_string(term, argv[i]);
            if (i < argc - 1) {
                terminal_write_char(term, ' ');
            }
        }
    }
    terminal_newline(term);
    return SHELL_OK;
}

