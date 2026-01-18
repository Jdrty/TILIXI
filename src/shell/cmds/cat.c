#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>

int cmd_cat(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_cat_def = {
    .name = "cat",
    .handler = cmd_cat,
    .help = "Display file contents"
};

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
    
    while (strlen(path_copy) > 1 && path_copy[strlen(path_copy) - 1] == '/') {
        path_copy[strlen(path_copy) - 1] = '\0';
    }
    
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

static int cat_write_output(terminal_state *term, vfs_file_t *out_file,
                            const char *buf, size_t len) {
    if (out_file != NULL) {
        ssize_t written = vfs_write(out_file, buf, len);
        return (written < 0 || (size_t)written != len) ? SHELL_ERR : SHELL_OK;
    }
    
    for (size_t i = 0; i < len; i++) {
        terminal_write_char(term, buf[i]);
    }
    return SHELL_OK;
}

int cmd_cat(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    
    if (argc < 2) {
        if (term->pipe_input != NULL && term->pipe_input_len > 0) {
            cat_write_output(term, NULL, term->pipe_input, term->pipe_input_len);
            return SHELL_OK;
        }
        shell_error(term, "cat: missing file operand");
        return SHELL_EINVAL;
    }
    
    int redirect_pos = -1;
    for (int i = 1; i < argc; i++) {
        if (argv[i] != NULL && strcmp(argv[i], ">") == 0) {
            redirect_pos = i;
            break;
        }
    }
    
    int last_input = (redirect_pos > 0) ? redirect_pos - 1 : argc - 1;
    if (last_input < 1) {
        if (term->pipe_input != NULL && term->pipe_input_len > 0) {
            last_input = 0;
        } else {
            shell_error(term, "cat: missing file operand");
            return SHELL_EINVAL;
        }
    }
    
    const char *out_path = NULL;
    if (redirect_pos >= 0) {
        if (redirect_pos + 1 >= argc) {
            shell_error(term, "cat: missing output file operand");
            return SHELL_EINVAL;
        }
        if (redirect_pos + 2 != argc) {
            shell_error(term, "cat: too many arguments");
            return SHELL_EINVAL;
        }
        out_path = argv[redirect_pos + 1];
    }
    
    vfs_file_t *out_file = NULL;
    vfs_node_t *out_parent = NULL;
    char *out_name = NULL;
    
    if (out_path != NULL) {
        vfs_node_t *out_node = vfs_resolve_at(term->cwd, out_path);
        if (out_node != NULL) {
            if (out_node->type != VFS_NODE_FILE) {
                shell_error(term, "cat: %s: not a file", out_path);
                vfs_node_release(out_node);
                return SHELL_EINVAL;
            }
            out_file = vfs_open_node(out_node, VFS_O_WRITE | VFS_O_TRUNC | VFS_O_CREATE);
            vfs_node_release(out_node);
        } else {
            int res = resolve_parent_and_name(term, out_path, &out_parent, &out_name);
            if (res != SHELL_OK) {
                if (res == SHELL_ENOENT) {
                    shell_error(term, "cat: %s: no such file or directory", out_path);
                } else {
                    shell_error(term, "cat: %s: invalid path", out_path);
                }
                return res;
            }
            vfs_node_t *created =
                vfs_dir_create_node(out_parent, out_name, VFS_NODE_FILE);
            if (created == NULL) {
                shell_error(term, "cat: %s: failed to create file", out_path);
                vfs_node_release(out_parent);
                free(out_name);
                return SHELL_ERR;
            }
            out_file = vfs_open_node(created, VFS_O_WRITE | VFS_O_TRUNC | VFS_O_CREATE);
            vfs_node_release(created);
        }
        
        if (out_parent != NULL) {
            vfs_node_release(out_parent);
        }
        free(out_name);
        
        if (out_file == NULL) {
            shell_error(term, "cat: %s: unable to open output", out_path);
            return SHELL_ERR;
        }
    }
    
    if (last_input == 0) {
        if (out_path != NULL) {
            if (cat_write_output(term, out_file, term->pipe_input, term->pipe_input_len) != SHELL_OK) {
                if (out_file != NULL) {
                    vfs_close(out_file);
                }
                shell_error(term, "cat: write error");
                return SHELL_ERR;
            }
        } else {
            cat_write_output(term, NULL, term->pipe_input, term->pipe_input_len);
        }
        if (out_file != NULL) {
            vfs_close(out_file);
        }
        return SHELL_OK;
    }
    
    char buffer[128];
    for (int i = 1; i <= last_input; i++) {
        const char *path = argv[i];
        if (path == NULL) {
            if (out_file != NULL) {
                vfs_close(out_file);
            }
            return SHELL_ERR;
        }
        vfs_node_t *node = vfs_resolve_at(term->cwd, path);
        if (node == NULL) {
            shell_error(term, "cat: %s: no such file or directory", path);
            if (out_file != NULL) {
                vfs_close(out_file);
            }
            return SHELL_ENOENT;
        }
        if (node->type != VFS_NODE_FILE) {
            shell_error(term, "cat: %s: not a file", path);
            vfs_node_release(node);
            if (out_file != NULL) {
                vfs_close(out_file);
            }
            return SHELL_EINVAL;
        }
        
        vfs_file_t *file = vfs_open_node(node, VFS_O_READ);
        vfs_node_release(node);
        if (file == NULL) {
            shell_error(term, "cat: %s: unable to open", path);
            if (out_file != NULL) {
                vfs_close(out_file);
            }
    return SHELL_ERR;
        }
        
        while (1) {
            ssize_t read_bytes = vfs_read(file, buffer, sizeof(buffer));
            if (read_bytes < 0) {
                vfs_close(file);
                if (out_file != NULL) {
                    vfs_close(out_file);
                }
                shell_error(term, "cat: %s: read error", path);
                return SHELL_ERR;
            }
            if (read_bytes == 0) {
                break;
            }
            if (cat_write_output(term, out_file, buffer, (size_t)read_bytes) != SHELL_OK) {
                vfs_close(file);
                if (out_file != NULL) {
                    vfs_close(out_file);
                }
                shell_error(term, "cat: write error");
                return SHELL_ERR;
            }
        }
        
        vfs_close(file);
    }
    
    if (out_file != NULL) {
        vfs_close(out_file);
    }
    
    return SHELL_OK;
}

