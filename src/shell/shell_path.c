#include "shell_path.h"
#include "vfs.h"
#include "shell_codes.h"
#include <string.h>

int shell_get_path(vfs_node_t *node, char *buf, size_t buflen) {
    if (node == NULL || buf == NULL || buflen == 0) {
        return SHELL_ERR;
    }
    
    if (buflen < 2) {
        return SHELL_ERR;
    }
    
    return SHELL_ENOTSUP;
}

