#define _POSIX_C_SOURCE 200809L

#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

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
    
    // require at least one filename argument
    if (argc < 2) {
        shell_error(term, "touch: missing file operand");
        return SHELL_EINVAL;
    }
    
    // process each file argument
    for (int i = 1; i < argc; i++) {
        const char *filepath = argv[i];
        if (filepath == NULL) {
            shell_error(term, "touch: invalid argument");
            return SHELL_EINVAL;
        }
        
        // resolve the path relative to current working directory
        vfs_node_t *node = vfs_resolve_at(term->cwd, filepath);
        
        if (node != NULL) {
            // file exists, release the node and continue (unix touch updates timestamps,
            // but for now we just succeed if the file exists, wtv i can do it later)
            if (node->type == VFS_NODE_DIR) {
                shell_error(term, "touch: %s: is a directory", filepath);
                vfs_node_release(node);
                return SHELL_EINVAL;
            }
            vfs_node_release(node);
            // file exists, nothing to do (would update timestamp in full implementation)
            continue;
        }
        
        // file doesn't exist - need to create it
        // first, we need to find the parent directory and filename
        // parse the path to get parent directory and filename
        char *path_copy = strdup(filepath);
        if (path_copy == NULL) {
            shell_error(term, "touch: out of memory");
            return SHELL_ERR;
        }
        
        // find the last '/' to split into parent and filename
        char *last_slash = strrchr(path_copy, '/');
        vfs_node_t *parent_dir = NULL;
        char *filename_copy = NULL;
        
        if (last_slash == NULL) {
            // no slash - filename is in current directory
            filename_copy = strdup(filepath);
            parent_dir = term->cwd;
            if (parent_dir == NULL) {
                parent_dir = vfs_resolve("/");
            }
            if (parent_dir != NULL) {
                parent_dir->refcount++;  // don't take ownership, just increment refcount
            }
        } else {
            // has slash - split into parent path and filename
            *last_slash = '\0';
            const char *filename = last_slash + 1;
            
            // check if filename is empty (path ends with '/')
            if (filename[0] == '\0') {
                shell_error(term, "touch: %s: invalid filename (path ends with '/')", filepath);
                free(path_copy);
                return SHELL_EINVAL;
            }
            
            filename_copy = strdup(filename);
            
            // if parent path is empty after split, it's root
            if (path_copy[0] == '\0') {
                parent_dir = vfs_resolve("/");
            } else {
                parent_dir = vfs_resolve_at(term->cwd, path_copy);
            }
        }
        
        free(path_copy);
        
        if (filename_copy == NULL) {
            shell_error(term, "touch: out of memory");
            if (parent_dir != NULL) {
                vfs_node_release(parent_dir);
            }
            return SHELL_ERR;
        }
        
        if (parent_dir == NULL) {
            shell_error(term, "touch: %s: parent directory does not exist", filepath);
            free(filename_copy);
            return SHELL_ENOENT;
        }
        
        if (parent_dir->type != VFS_NODE_DIR) {
            shell_error(term, "touch: %s: parent is not a directory", filepath);
            vfs_node_release(parent_dir);
            free(filename_copy);
            return SHELL_ENOTDIR;
        }

        // create the file using VFS API
        vfs_node_t *new_file =
            vfs_dir_create_node(parent_dir, filename_copy, VFS_NODE_FILE);
        
        free(filename_copy);
        vfs_node_release(parent_dir);
        
        if (new_file == NULL) {
            shell_error(term, "touch: %s: failed to create file", filepath);
            return SHELL_ERR;
        }
        
        // release the created file node (we just wanted to create it)
        vfs_node_release(new_file);
    }
    
    return SHELL_OK;
}

