#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_run(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_run_def = {
    .name = "run",
    .handler = cmd_run,
    .help = "Execute program"
};

int cmd_run(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "run: missing program name");
        return SHELL_EINVAL;
    }
    
    // TODO: implement run functionality
    terminal_write_string(term, "run: not implemented\n");
    return SHELL_ERR;
}

