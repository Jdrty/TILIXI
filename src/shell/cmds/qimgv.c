#include "builtins.h"
#include "terminal.h"
#include "vfs.h"
#include "shell_codes.h"
#include "shell_error.h"
#include "shell_path.h"
#include "compat.h"
#include <string.h>

int cmd_qimgv(terminal_state *term, int argc, char **argv);

const builtin_cmd cmd_qimgv_def = {
    .name = "qimgv",
    .handler = cmd_qimgv,
    .help = "Open RGB565 image viewer in a new window"
};

static int has_rgb565_extension(const char *path) {
    if (path == NULL) {
        return 0;
    }
    const char *dot = strrchr(path, '.');
    return (dot != NULL && strcmp(dot, ".rgb565") == 0);
}

int cmd_qimgv(terminal_state *term, int argc, char **argv) {
    if (term == NULL) {
        return SHELL_ERR;
    }
    if (argc < 2 || argv[1] == NULL) {
        shell_error(term, "qimgv: missing file operand");
        return SHELL_EINVAL;
    }
    if (!has_rgb565_extension(argv[1])) {
        shell_error(term, "qimgv: expected .rgb565 file");
        return SHELL_EINVAL;
    }
    
    const char *path = argv[1];
    vfs_node_t *base = term->cwd;
    int base_owned = 0;
    if (base == NULL) {
        base = vfs_resolve("/");
        base_owned = 1;
    }
    
    vfs_node_t *node = NULL;
    if (path[0] == '/') {
        node = vfs_resolve(path);
    } else {
        node = vfs_resolve_at(base, path);
    }
    
    if (base_owned && base != NULL) {
        vfs_node_release(base);
    }
    
    if (node == NULL) {
        shell_error(term, "qimgv: no such file: %s", path);
        return SHELL_ENOENT;
    }
    if (node->type != VFS_NODE_FILE) {
        vfs_node_release(node);
        shell_error(term, "qimgv: not a file: %s", path);
        return SHELL_EINVAL;
    }
    
    char abs_path[256];
    if (shell_get_path(node, abs_path, sizeof(abs_path)) != SHELL_OK) {
        vfs_node_release(node);
        shell_error(term, "qimgv: failed to resolve path");
        return SHELL_ERR;
    }
    vfs_node_release(node);
    
    new_terminal();
    terminal_state *viewer = get_active_terminal();
    if (viewer == NULL) {
        return SHELL_ERR;
    }
    
    terminal_clear(viewer);
    viewer->input_len = 0;
    viewer->input_pos = 0;
    viewer->input_line[0] = '\0';
    viewer->image_view_active = 1;
    size_t copy_len = strnlen(abs_path, sizeof(viewer->image_view_path) - 1);
    memcpy(viewer->image_view_path, abs_path, copy_len);
    viewer->image_view_path[copy_len] = '\0';
    viewer->fastfetch_image_active = 0;
    
#ifdef ARDUINO
    terminal_render_window(viewer);
#endif
    
    return SHELL_OK;
}

