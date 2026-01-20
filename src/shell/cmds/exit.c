#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <stdlib.h>

int cmd_exit(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_exit_def = {
    .name = "exit",
    .handler = cmd_exit,
    .help = "Close the current terminal"
};

int cmd_exit(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc > 2) {
        shell_error(term, "exit: too many arguments");
        return SHELL_EINVAL;
    }
    
    if (argc == 2 && argv[1] != NULL) {
        char *end = NULL;
        (void)strtol(argv[1], &end, 10);
        if (end == argv[1] || (end != NULL && *end != '\0')) {
            shell_error(term, "exit: numeric argument required");
            return SHELL_EINVAL;
        }
    }
    
    close_terminal();
    return SHELL_OK;
}









