#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

int cmd_mv(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_mv_def = {
    .name = "mv",
    .handler = cmd_mv,
    .help = "Move or rename files"
};

static char *trim_trailing_slashes(char *path) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
    return path;
}

static int name_has_extension(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    const char *dot = strrchr(name, '.');
    return (dot != NULL && dot != name);
}

static char *basename_from_path(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    char *copy = strdup(path);
    if (copy == NULL) {
        return NULL;
    }
    
    trim_trailing_slashes(copy);
    
    char *last_slash = strrchr(copy, '/');
    char *name = (last_slash == NULL) ? copy : last_slash + 1;
    if (name[0] == '\0') {
        free(copy);
        return NULL;
    }
    
    char *result = strdup(name);
    free(copy);
    return result;
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

static int mv_single(terminal_state *term, const char *src_path,
                     const char *dst_path, int dst_is_dir) {
    vfs_node_t *src_node = vfs_resolve_at(term->cwd, src_path);
    if (src_node == NULL) {
        shell_error(term, "mv: %s: no such file or directory", src_path);
        return SHELL_ENOENT;
    }
    
    vfs_node_t *src_parent = NULL;
    char *src_name = NULL;
    int src_res = resolve_parent_and_name(term, src_path, &src_parent, &src_name);
    if (src_res != SHELL_OK) {
        vfs_node_release(src_node);
        shell_error(term, "mv: %s: invalid path", src_path);
        return SHELL_EINVAL;
    }
    
    vfs_node_t *dst_dir = NULL;
    char *dst_name = NULL;
    vfs_node_t *dst_node = NULL;
    
    if (dst_is_dir) {
        dst_dir = vfs_resolve_at(term->cwd, dst_path);
        if (dst_dir == NULL) {
            vfs_node_release(src_node);
            vfs_node_release(src_parent);
            free(src_name);
            shell_error(term, "mv: %s: no such file or directory", dst_path);
            return SHELL_ENOENT;
        }
        if (dst_dir->type != VFS_NODE_DIR) {
            vfs_node_release(src_node);
            vfs_node_release(src_parent);
            vfs_node_release(dst_dir);
            free(src_name);
            shell_error(term, "mv: %s: not a directory", dst_path);
            return SHELL_ENOTDIR;
        }
        dst_name = basename_from_path(src_path);
        if (dst_name == NULL) {
            vfs_node_release(src_node);
            vfs_node_release(src_parent);
            vfs_node_release(dst_dir);
            free(src_name);
            shell_error(term, "mv: %s: invalid path", src_path);
            return SHELL_EINVAL;
        }
    } else {
        int dst_res = resolve_parent_and_name(term, dst_path, &dst_dir, &dst_name);
        if (dst_res != SHELL_OK) {
            vfs_node_release(src_node);
            vfs_node_release(src_parent);
            free(src_name);
            if (dst_res == SHELL_ENOENT) {
                shell_error(term, "mv: %s: no such file or directory", dst_path);
                return SHELL_ENOENT;
            }
            shell_error(term, "mv: %s: invalid path", dst_path);
            return SHELL_EINVAL;
        }
    }
    
    dst_node = vfs_resolve_at(dst_dir, dst_name);
    if (src_node->type == VFS_NODE_DIR && name_has_extension(dst_name)) {
        vfs_node_release(dst_dir);
        vfs_node_release(src_parent);
        vfs_node_release(src_node);
        free(src_name);
        shell_error(term, "mv: %s: invalid directory name", dst_name);
        free(dst_name);
        return SHELL_EINVAL;
    }
    
    if (dst_node != NULL) {
        if (dst_node == src_node) {
            vfs_node_release(dst_node);
            vfs_node_release(dst_dir);
            vfs_node_release(src_parent);
            vfs_node_release(src_node);
            free(src_name);
            free(dst_name);
            return SHELL_OK;
        }
        
        if (dst_node->type == VFS_NODE_DIR) {
            vfs_node_release(dst_node);
            vfs_node_release(dst_dir);
            vfs_node_release(src_parent);
            vfs_node_release(src_node);
            free(src_name);
            shell_error(term, "mv: %s: is a directory", dst_name);
            free(dst_name);
            return SHELL_ENOTDIR;
        }
        
        if (src_node->type == VFS_NODE_DIR) {
            vfs_node_release(dst_node);
            vfs_node_release(dst_dir);
            vfs_node_release(src_parent);
            vfs_node_release(src_node);
            free(src_name);
            free(dst_name);
            shell_error(term, "mv: %s: not a directory", dst_path);
            return SHELL_ENOTDIR;
        }
        
        vfs_node_release(dst_node);
        int rm_res = vfs_dir_remove_node(dst_dir, dst_name);
        if (rm_res != VFS_EOK) {
            vfs_node_release(dst_dir);
            vfs_node_release(src_parent);
            vfs_node_release(src_node);
            free(src_name);
            free(dst_name);
            shell_error(term, "mv: %s: failed to remove", dst_path);
            return SHELL_ERR;
        }
    }
    
    int mv_res = vfs_dir_rename_node(src_parent, src_name, dst_dir, dst_name);
    
    vfs_node_release(dst_dir);
    vfs_node_release(src_parent);
    vfs_node_release(src_node);
    free(src_name);
    free(dst_name);
    
    if (mv_res != VFS_EOK) {
        shell_error(term, "mv: %s: failed to move", src_path);
        return SHELL_ERR;
    }
    
    return SHELL_OK;
}

int cmd_mv(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 3) {
        shell_error(term, "mv: missing file operand");
        return SHELL_EINVAL;
    }
    
    int src_count = argc - 2;
    const char *target = argv[argc - 1];
    if (target == NULL) {
        return SHELL_ERR;
    }
    
    size_t target_len = strlen(target);
    int target_trailing_slash = (target_len > 1 && target[target_len - 1] == '/');
    int target_is_dir = 0;
    
    vfs_node_t *target_node = vfs_resolve_at(term->cwd, target);
    if (target_node != NULL) {
        if (target_node->type == VFS_NODE_DIR) {
            target_is_dir = 1;
        }
        vfs_node_release(target_node);
    } else if (target_trailing_slash) {
        shell_error(term, "mv: %s: not a directory", target);
        return SHELL_ENOTDIR;
    }
    
    if (src_count > 1) {
        if (!target_is_dir) {
            shell_error(term, "mv: %s: not a directory", target);
            return SHELL_ENOTDIR;
        }
        
        for (int i = 1; i < argc - 1; i++) {
            int result = mv_single(term, argv[i], target, 1);
            if (result != SHELL_OK) {
                return result;
            }
        }
        return SHELL_OK;
    }
    
    return mv_single(term, argv[1], target, target_is_dir);
}

