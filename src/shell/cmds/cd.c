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

static int read_username_from_passwd(char *out, size_t out_len) {
    if (out == NULL || out_len == 0) {
        return 0;
    }
    vfs_file_t *file = vfs_open("/etc/passwd", VFS_O_READ);
    if (file == NULL) {
        return 0;
    }
    char buf[128];
    ssize_t read_bytes = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (read_bytes <= 0) {
        return 0;
    }
    buf[read_bytes] = '\0';
    char *newline = strchr(buf, '\n');
    if (newline) {
        *newline = '\0';
    }
    char *colon = strchr(buf, ':');
    if (colon) {
        *colon = '\0';
    }
    if (buf[0] == '\0') {
        return 0;
    }
    strncpy(out, buf, out_len - 1);
    out[out_len - 1] = '\0';
    return 1;
}

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
    
    char expanded_path[256];
    if (path[0] == '~' && (path[1] == '\0' || path[1] == '/')) {
        char username[64];
        if (read_username_from_passwd(username, sizeof(username))) {
            snprintf(expanded_path, sizeof(expanded_path), "/home/%s%s",
                     username, path + 1);
        } else {
            snprintf(expanded_path, sizeof(expanded_path), "/home%s", path + 1);
        }
        path = expanded_path;
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

