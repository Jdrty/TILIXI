#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_reload(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_reload_def = {
    .name = "reload",
    .handler = cmd_reload,
    .help = "Reload TILIXI config"
};

int cmd_reload(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    (void)argv;
    
    if (argc > 1) {
        shell_error(term, "reload: too many arguments");
        return SHELL_EINVAL;
    }
    
    terminal_reload_config();
    return SHELL_OK;
}


