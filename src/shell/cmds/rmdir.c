#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

int cmd_rmdir(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_rmdir_def = {
    .name = "rmdir",
    .handler = cmd_rmdir,
    .help = "Remove empty directories"
};

static char *trim_trailing_slashes(char *path) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
    return path;
}

static int resolve_parent_and_name(terminal_state *term, const char *path,
                                   vfs_node_t **out_parent, char **out_name) {
    if (term == NULL || path == NULL || out_parent == NULL || out_name == NULL) {
        return SHELL_ERR;
    }
    
    *out_parent = NULL;
    *out_name = NULL;
    
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        return SHELL_ERR;
    }
    
    trim_trailing_slashes(path_copy);
    
    if (strcmp(path_copy, "/") == 0) {
        free(path_copy);
        return SHELL_EINVAL;
    }
    
    char *last_slash = strrchr(path_copy, '/');
    char *name = NULL;
    
    if (last_slash == NULL) {
        name = path_copy;
        *out_parent = term->cwd;
        if (*out_parent == NULL) {
            *out_parent = vfs_resolve("/");
        } else {
            (*out_parent)->refcount++;
        }
    } else {
        *last_slash = '\0';
        name = last_slash + 1;
        if (path_copy[0] == '\0') {
            *out_parent = vfs_resolve("/");
        } else {
            *out_parent = vfs_resolve_at(term->cwd, path_copy);
        }
    }
    
    if (name == NULL || name[0] == '\0') {
        if (*out_parent != NULL) {
            vfs_node_release(*out_parent);
        }
        free(path_copy);
        return SHELL_EINVAL;
    }
    
    *out_name = strdup(name);
    free(path_copy);
    
    if (*out_name == NULL) {
        if (*out_parent != NULL) {
            vfs_node_release(*out_parent);
        }
        return SHELL_ERR;
    }
    
    if (strcmp(*out_name, ".") == 0 || strcmp(*out_name, "..") == 0) {
        vfs_node_release(*out_parent);
        free(*out_name);
        *out_parent = NULL;
        *out_name = NULL;
        return SHELL_EINVAL;
    }
    
    if (*out_parent == NULL) {
        free(*out_name);
        *out_name = NULL;
        return SHELL_ENOENT;
    }
    
    if ((*out_parent)->type != VFS_NODE_DIR) {
        vfs_node_release(*out_parent);
        free(*out_name);
        *out_parent = NULL;
        *out_name = NULL;
        return SHELL_ENOTDIR;
    }
    
    return SHELL_OK;
}

static int dir_is_empty(terminal_state *term, vfs_node_t *dir_node) {
    vfs_dir_iter_t *iter = vfs_dir_iter_create_node(dir_node);
    if (iter == NULL) {
        shell_error(term, "rmdir: directory iteration not supported");
        return SHELL_ERR;
    }
    
    int empty = 1;
    while (1) {
        int result = vfs_dir_iter_next(iter);
        if (result < 0) {
            vfs_dir_iter_destroy(iter);
            shell_error(term, "rmdir: error reading directory");
            return SHELL_ERR;
        }
        if (result == 0) {
            break;
        }
        if (iter->current_name == NULL) {
            continue;
        }
        if (strcmp(iter->current_name, ".") == 0 || strcmp(iter->current_name, "..") == 0) {
            continue;
        }
        empty = 0;
        break;
    }
    
    vfs_dir_iter_destroy(iter);
    return empty ? 1 : 0;
}

int cmd_rmdir(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "rmdir: missing operand");
        return SHELL_EINVAL;
    }
    
    for (int i = 1; i < argc; i++) {
        const char *path = argv[i];
        if (path == NULL) {
            return SHELL_ERR;
        }
        
        vfs_node_t *parent = NULL;
        char *name = NULL;
        int resolve_result = resolve_parent_and_name(term, path, &parent, &name);
        if (resolve_result != SHELL_OK) {
            if (resolve_result == SHELL_ENOENT) {
                shell_error(term, "rmdir: %s: no such file or directory", path);
                return SHELL_ENOENT;
            }
            shell_error(term, "rmdir: %s: invalid path", path);
            return SHELL_EINVAL;
        }
        
        vfs_node_t *node = vfs_resolve_at(parent, name);
        if (node == NULL) {
            vfs_node_release(parent);
            free(name);
            shell_error(term, "rmdir: %s: no such file or directory", path);
            return SHELL_ENOENT;
        }
        
        if (node->type != VFS_NODE_DIR) {
            vfs_node_release(node);
            vfs_node_release(parent);
            free(name);
            shell_error(term, "rmdir: %s: not a directory", path);
            return SHELL_ENOTDIR;
        }
        
        int empty_result = dir_is_empty(term, node);
        if (empty_result <= 0) {
            vfs_node_release(node);
            vfs_node_release(parent);
            free(name);
            if (empty_result == 0) {
                shell_error(term, "rmdir: %s: directory not empty", path);
                return SHELL_ERR;
            }
            return SHELL_ERR;
        }
        
        int remove_result = vfs_dir_remove_node(parent, name);
        vfs_node_release(node);
        vfs_node_release(parent);
        free(name);
        
        if (remove_result != VFS_EOK) {
            shell_error(term, "rmdir: %s: failed to remove", path);
            return SHELL_ERR;
        }
    }
    
    return SHELL_OK;
}

