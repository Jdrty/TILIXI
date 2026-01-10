#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_reboot(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_reboot_def = {
    .name = "reboot",
    .handler = cmd_reboot,
    .help = "Reboot system"
};

int cmd_reboot(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc > 1) {
        shell_error(term, "reboot: too many arguments");
        return SHELL_EINVAL;
    }
    
    // TODO: implement reboot functionality
    terminal_write_string(term, "reboot: not implemented\n");
    return SHELL_ERR;
}

