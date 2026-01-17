#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include <string.h>

int cmd_ls(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_ls_def = {
    .name = "ls",
    .handler = cmd_ls,
    .help = "List directory contents"
};

int cmd_ls(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    // reject too many arguments
    if (argc > 2) {
        shell_error(term, "ls: too many arguments");
        return SHELL_EINVAL;
    }
    
    // determine target directory
    vfs_node_t *dir = term->cwd;
    int dir_owned = 0;
    
    if (argc > 1) {
        // list specified path
        const char *path = argv[1];
        if (path == NULL) {
            return SHELL_ERR;
        }
        
        dir = vfs_resolve_at(term->cwd, path);
        if (dir == NULL) {
            shell_error(term, "ls: %s: no such file or directory", path);
            return SHELL_ENOENT;
        }
        
        if (dir->type != VFS_NODE_DIR) {
            shell_error(term, "ls: %s: not a directory", path);
            vfs_node_release(dir);
            return SHELL_ENOTDIR;
        }
        dir_owned = 1;
    } else {
        // list current directory
        if (dir == NULL) {
            dir = vfs_resolve("/");
            if (dir == NULL) {
                shell_error(term, "ls: no filesystem mounted");
                return SHELL_ERR;
            }
            dir_owned = 0;
        }
    }
    
    // create directory iterator
    vfs_dir_iter_t *iter = vfs_dir_iter_create_node(dir);
    if (iter == NULL) {
        shell_error(term, "ls: directory iteration not supported");
        if (dir_owned) {
            vfs_node_release(dir);
        }
        return SHELL_ERR;
    }
    
    int use_newlines = terminal_capture_is_active();
    
    // iterate through entries
    int entry_count = 0;
    while (1) {
        int result = vfs_dir_iter_next(iter);
        if (result < 0) {
            // error reading directory
            shell_error(term, "ls: error reading directory");
            vfs_dir_iter_destroy(iter);
            if (dir_owned) {
                vfs_node_release(dir);
            }
            return SHELL_ERR;
        }
        
        if (result == 0) {
            // end of directory
            break;
        }
        
        // result > 0 hence entry available
        if (iter->current_name != NULL) {
            if (use_newlines) {
                terminal_write_string(term, iter->current_name);
                terminal_newline(term);
            } else {
                if (entry_count > 0) {
                    terminal_write_char(term, ' ');
                }
                terminal_write_string(term, iter->current_name);
            }
            entry_count++;
        }
    }
    
    // always print a newline after listing (even if directory is empty)
    if (!use_newlines) {
        terminal_newline(term);
    }
    
    vfs_dir_iter_destroy(iter);
    if (dir_owned) {
        vfs_node_release(dir);
    }
    
    return SHELL_OK;
}

