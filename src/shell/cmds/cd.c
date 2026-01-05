#include "builtins.h"
#include "vfs.h"
#include "terminal.h"
#include "shell_error.h"
#include "shell_codes.h"
#include <string.h>

int cmd_cd(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_cd_def = {
    .name = "cd",
    .handler = cmd_cd,
    .help = "Change directory"
};

int cmd_cd(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc == 1) {
        vfs_node_t *root = vfs_resolve("/");
        if (root == NULL) {
            shell_error(term, "cd: root directory not found");
            return SHELL_ENOENT;
        }
        
        if (term->cwd != NULL) {
            vfs_node_release(term->cwd);
        }
        term->cwd = root;
        return SHELL_OK;
    }
    
    if (argc > 2) {
        shell_error(term, "cd: too many arguments");
        return SHELL_EINVAL;
    }
    
    const char *path = argv[1];
    if (path == NULL) {
        return SHELL_ERR;
    }
    
    vfs_node_t *new_cwd = vfs_resolve_at(term->cwd, path);
    if (new_cwd == NULL) {
        shell_error(term, "cd: %s: no such file or directory", path);
        return SHELL_ENOENT;
    }
    
    if (new_cwd->type != VFS_NODE_DIR) {
        shell_error(term, "cd: %s: not a directory", path);
        vfs_node_release(new_cwd);
        return SHELL_ENOTDIR;
    }
    
    if (term->cwd != NULL) {
        vfs_node_release(term->cwd);
    }
    term->cwd = new_cwd;
    
    return SHELL_OK;
}

