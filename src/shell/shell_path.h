#pragma once
#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

int shell_get_path(vfs_node_t *node, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

