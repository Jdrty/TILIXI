#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

int cmd_mkdir(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_mkdir_def = {
    .name = "mkdir",
    .handler = cmd_mkdir,
    .help = "Create directories"
};

static int has_non_slash(const char *str) {
    if (str == NULL) {
        return 0;
    }
    while (*str != '\0') {
        if (*str != '/') {
            return 1;
        }
        str++;
    }
    return 0;
}

static int name_has_extension(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    const char *dot = strrchr(name, '.');
    return (dot != NULL && dot != name);
}

static int mkdir_single(terminal_state *term, const char *path, int parents) {
    if (path == NULL || path[0] == '\0') {
        shell_error(term, "mkdir: missing operand");
        return SHELL_EINVAL;
    }
    
    vfs_node_t *current = NULL;
    if (path[0] == '/') {
        current = vfs_resolve("/");
    } else {
        current = term->cwd;
        if (current != NULL) {
            current->refcount++;
        } else {
            current = vfs_resolve("/");
        }
    }
    
    if (current == NULL) {
        shell_error(term, "mkdir: no filesystem mounted");
        return SHELL_ERR;
    }
    
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
        vfs_node_release(current);
        shell_error(term, "mkdir: out of memory");
        return SHELL_ERR;
    }
    
    char *cursor = path_copy;
    while (*cursor == '/') {
        cursor++;
    }
    
    if (*cursor == '\0') {
        free(path_copy);
        vfs_node_release(current);
        shell_error(term, "mkdir: %s: file exists", path);
        return SHELL_EINVAL;
    }
    
    while (*cursor != '\0') {
        char *slash = strchr(cursor, '/');
        int is_last = 0;
        if (slash == NULL) {
            is_last = 1;
        } else {
            *slash = '\0';
            is_last = !has_non_slash(slash + 1);
        }
        
        if (*cursor == '\0') {
            if (slash == NULL) {
                break;
            }
            cursor = slash + 1;
            continue;
        }
        
        if (strcmp(cursor, ".") == 0) {
            if (slash == NULL) {
                break;
            }
            cursor = slash + 1;
            continue;
        }
        
        if (strcmp(cursor, "..") == 0) {
            vfs_node_t *next_node = vfs_resolve_at(current, "..");
            if (next_node == NULL) {
                free(path_copy);
                vfs_node_release(current);
                shell_error(term, "mkdir: %s: no such file or directory", path);
                return SHELL_ENOENT;
            }
            vfs_node_release(current);
            current = next_node;
            
            if (slash == NULL) {
                break;
            }
            cursor = slash + 1;
            continue;
        }
        
        vfs_node_t *next = vfs_resolve_at(current, cursor);
        if (next != NULL) {
            if (next->type != VFS_NODE_DIR) {
                shell_error(term, "mkdir: %s: not a directory", cursor);
                free(path_copy);
                vfs_node_release(next);
                vfs_node_release(current);
                return SHELL_ENOTDIR;
            }
            if (is_last && !parents) {
                free(path_copy);
                vfs_node_release(next);
                vfs_node_release(current);
                shell_error(term, "mkdir: %s: file exists", path);
                return SHELL_EINVAL;
            }
            vfs_node_release(current);
            current = next;
        } else {
            if (!parents && !is_last) {
                free(path_copy);
                vfs_node_release(current);
                shell_error(term, "mkdir: %s: no such file or directory", path);
                return SHELL_ENOENT;
            }
            
            if (name_has_extension(cursor)) {
                shell_error(term, "mkdir: %s: invalid directory name", cursor);
                free(path_copy);
                vfs_node_release(current);
                return SHELL_EINVAL;
            }

            vfs_node_t *created = vfs_dir_create_node(current, cursor, VFS_NODE_DIR);
            if (created == NULL) {
                free(path_copy);
                vfs_node_release(current);
                shell_error(term, "mkdir: %s: failed to create directory", path);
                return SHELL_ERR;
            }
            vfs_node_release(current);
            current = created;
        }
        
        if (slash == NULL) {
            break;
        }
        cursor = slash + 1;
    }
    
    free(path_copy);
    vfs_node_release(current);
    return SHELL_OK;
}

int cmd_mkdir(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        shell_error(term, "mkdir: missing operand");
        return SHELL_EINVAL;
    }
    
    int parents = 0;
    int paths_found = 0;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            return SHELL_ERR;
        }
        
        if (strcmp(arg, "-p") == 0) {
            parents = 1;
            continue;
        }
        
        if (arg[0] == '-') {
            shell_error(term, "mkdir: invalid option -- %s", arg);
            return SHELL_EINVAL;
        }
        
        paths_found = 1;
        int result = mkdir_single(term, arg, parents);
        if (result != SHELL_OK) {
            return result;
        }
    }
    
    if (!paths_found) {
        shell_error(term, "mkdir: missing operand");
        return SHELL_EINVAL;
    }
    
    return SHELL_OK;
}

