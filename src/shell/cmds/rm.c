#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

int cmd_rm(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_rm_def = {
    .name = "rm",
    .handler = cmd_rm,
    .help = "Remove files"
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

static int rm_dir_contents(terminal_state *term, vfs_node_t *dir_node, int force);

static int rm_entry_from_parent(terminal_state *term, vfs_node_t *parent,
                                const char *name, int recursive, int force) {
    if (parent == NULL || name == NULL) {
        return SHELL_ERR;
    }
    
    vfs_node_t *node = vfs_resolve_at(parent, name);
    if (node == NULL) {
        if (force) {
            return SHELL_OK;
        }
        shell_error(term, "rm: %s: no such file or directory", name);
        return SHELL_ENOENT;
    }
    
    if (node->type == VFS_NODE_DIR) {
        if (!recursive) {
            shell_error(term, "rm: %s: is a directory", name);
            vfs_node_release(node);
            return SHELL_EINVAL;
        }
        
        int result = rm_dir_contents(term, node, force);
        if (result != SHELL_OK) {
            vfs_node_release(node);
            return result;
        }
    }
    
    int remove_result = vfs_dir_remove_node(parent, name);
    vfs_node_release(node);
    
    if (remove_result != VFS_EOK) {
        if (force) {
            return SHELL_OK;
        }
        shell_error(term, "rm: %s: failed to remove", name);
        return SHELL_ERR;
    }
    
    return SHELL_OK;
}

static int rm_dir_contents(terminal_state *term, vfs_node_t *dir_node, int force) {
    vfs_dir_iter_t *iter = vfs_dir_iter_create_node(dir_node);
    if (iter == NULL) {
        shell_error(term, "rm: directory iteration not supported");
        return SHELL_ERR;
    }
    
    while (1) {
        int result = vfs_dir_iter_next(iter);
        if (result < 0) {
            vfs_dir_iter_destroy(iter);
            shell_error(term, "rm: error reading directory");
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
        
        int entry_result = rm_entry_from_parent(term, dir_node, iter->current_name, 1, force);
        if (entry_result != SHELL_OK) {
            vfs_dir_iter_destroy(iter);
            return entry_result;
        }
    }
    
    vfs_dir_iter_destroy(iter);
    return SHELL_OK;
}

static int rm_path(terminal_state *term, const char *path, int recursive, int force) {
    vfs_node_t *parent = NULL;
    char *name = NULL;
    int resolve_result = resolve_parent_and_name(term, path, &parent, &name);
    if (resolve_result != SHELL_OK) {
        if (force && resolve_result == SHELL_ENOENT) {
            return SHELL_OK;
        }
        if (resolve_result == SHELL_ENOENT) {
            shell_error(term, "rm: %s: no such file or directory", path);
            return SHELL_ENOENT;
        }
        shell_error(term, "rm: %s: invalid path", path);
        return SHELL_EINVAL;
    }
    
    int result = rm_entry_from_parent(term, parent, name, recursive, force);
    
    vfs_node_release(parent);
    free(name);
    return result;
}

int cmd_rm(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "rm: missing operand");
        return SHELL_EINVAL;
    }
    
    int recursive = 0;
    int force = 0;
    int paths_found = 0;
    int parsing_opts = 1;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
    return SHELL_ERR;
        }
        
        if (parsing_opts && strcmp(arg, "--") == 0) {
            parsing_opts = 0;
            continue;
        }
        
        if (parsing_opts && arg[0] == '-' && arg[1] != '\0') {
            if (strcmp(arg, "-r") == 0 || strcmp(arg, "-R") == 0) {
                recursive = 1;
                continue;
            }
            if (strcmp(arg, "-f") == 0) {
                force = 1;
                continue;
            }
            if (strcmp(arg, "-rf") == 0 || strcmp(arg, "-fr") == 0 ||
                strcmp(arg, "-Rf") == 0 || strcmp(arg, "-fR") == 0) {
                recursive = 1;
                force = 1;
                continue;
            }
            shell_error(term, "rm: invalid option -- %s", arg);
            return SHELL_EINVAL;
        }
        
        parsing_opts = 0;
        paths_found = 1;
        int result = rm_path(term, arg, recursive, force);
        if (result != SHELL_OK) {
            return result;
        }
    }
    
    if (!paths_found) {
        shell_error(term, "rm: missing operand");
        return SHELL_EINVAL;
    }
    
    return SHELL_OK;
}

