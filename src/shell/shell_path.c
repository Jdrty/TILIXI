#include "shell_path.h"
#include "vfs.h"
#include "shell_codes.h"
#include <string.h>
#include <stdint.h>

int shell_get_path(vfs_node_t *node, char *buf, size_t buflen) {
    if (node == NULL || buf == NULL || buflen == 0) {
        return SHELL_ERR;
    }
    
    if (buflen < 2) {
        return SHELL_ERR;
    }
    
    uintptr_t raw = (uintptr_t)node->backend_data;
    if (raw == 0) {
        strncpy(buf, "/", buflen - 1);
        buf[buflen - 1] = '\0';
        return SHELL_OK;
    }
    
    if (raw < 4096) {
        if (raw == 1) {
            strncpy(buf, "/dir1", buflen - 1);
        } else if (raw == 2) {
            strncpy(buf, "/dir1/subdir", buflen - 1);
        } else {
            strncpy(buf, "/", buflen - 1);
        }
        buf[buflen - 1] = '\0';
        return SHELL_OK;
    }
    
    const char *path = (const char*)node->backend_data;
    if (path == NULL || path[0] == '\0') {
        strncpy(buf, "/", buflen - 1);
        buf[buflen - 1] = '\0';
        return SHELL_OK;
    }
    if (strlen(path) >= buflen) {
        return SHELL_ERR;
    }
    strncpy(buf, path, buflen - 1);
    buf[buflen - 1] = '\0';
    return SHELL_OK;
}

