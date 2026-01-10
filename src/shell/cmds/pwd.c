#include "builtins.h"
#include "terminal.h"
#include "shell_path.h"
#include "shell_codes.h"
#include "shell_error.h"

int cmd_pwd(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_pwd_def = {
    .name = "pwd",
    .handler = cmd_pwd,
    .help = "Print working directory"
};

int cmd_pwd(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc > 1) {
        shell_error(term, "pwd: too many arguments");
        return SHELL_EINVAL;
    }
    
    if (term->cwd == NULL) {
        terminal_write_string(term, "/\n");
        return SHELL_OK;
    }
    
    char path[256];
    int result = shell_get_path(term->cwd, path, sizeof(path));
    if (result == SHELL_OK) {
        terminal_write_string(term, path);
        terminal_newline(term);
        return SHELL_OK;
    } else if (result == SHELL_ENOTSUP) {
        shell_error(term, "pwd: path resolution not implemented");
        return SHELL_ENOTSUP;
    }
    
    shell_error(term, "pwd: failed to get path");
    return SHELL_ERR;
}

