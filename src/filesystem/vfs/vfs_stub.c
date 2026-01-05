// temp file just needed for good test

#include "vfs.h"
#include <stdlib.h>
#include <string.h>

static vfs_node_t root_node = {
    .type = VFS_NODE_DIR,
    .ops = NULL,
    .backend_data = NULL,
    .is_readonly = 0,
    .is_hidden = 0,
    .reserved = 0,
    .refcount = 1
};

int vfs_init(void) {
    return VFS_EOK;
}

vfs_node_t* vfs_resolve(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    if (strcmp(path, "/") == 0) {
        root_node.refcount++;
        return &root_node;
    }
    
    return NULL;
}

vfs_node_t* vfs_resolve_at(vfs_node_t *base, const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    if (path[0] == '/') {
        return vfs_resolve(path);
    }
    
    if (base == NULL) {
        return vfs_resolve("/");
    }
    
    if (strcmp(path, ".") == 0) {
        base->refcount++;
        return base;
    }
    
    if (strcmp(path, "..") == 0) {
        root_node.refcount++;
        return &root_node;
    }
    
    return NULL;
}

void vfs_node_release(vfs_node_t *node) {
    if (node == NULL) {
        return;
    }
    
    if (node->refcount > 0) {
        node->refcount--;
    }
}

