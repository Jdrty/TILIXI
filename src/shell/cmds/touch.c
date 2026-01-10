#include "builtins.h"
#include "terminal.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_touch(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_touch_def = {
    .name = "touch",
    .handler = cmd_touch,
    .help = "Create empty file"
};

int cmd_touch(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "touch: missing file operand");
        return SHELL_EINVAL;
    }
    
    // TODO: implement touch functionality
    terminal_write_string(term, "touch: not implemented\n");
    return SHELL_ERR;
}

