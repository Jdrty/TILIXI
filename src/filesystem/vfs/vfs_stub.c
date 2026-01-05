// temp file just needed for good test

#include "vfs.h"
#include <stdlib.h>
#include <string.h>

static vfs_dir_iter_t stub_iter = {0};
static char stub_entry_names[][16] = {"file1.txt", "file2.txt", "dir1"};
static int stub_entry_idx = 0;

static vfs_dir_iter_t* stub_dir_iter_create(vfs_node_t *dir_node) {
    if (dir_node == NULL || dir_node->type != VFS_NODE_DIR) {
        return NULL;
    }
    
    stub_iter.dir_node = dir_node;
    stub_iter.backend_iter = NULL;
    stub_iter.current_name = NULL;
    stub_iter.name_len = 0;
    stub_entry_idx = 0;
    
    return &stub_iter;
}

static int stub_dir_iter_next(vfs_dir_iter_t *iter) {
    if (iter == NULL) {
        return -1;
    }
    
    if (stub_entry_idx >= 3) {
        return 0;
    }
    
    iter->current_name = stub_entry_names[stub_entry_idx];
    iter->name_len = strlen(stub_entry_names[stub_entry_idx]);
    stub_entry_idx++;
    
    return 1;
}

static void stub_dir_iter_destroy(vfs_dir_iter_t *iter) {
    if (iter == NULL) {
        return;
    }
    
    iter->dir_node = NULL;
    iter->backend_iter = NULL;
    iter->current_name = NULL;
    iter->name_len = 0;
}

static const vfs_ops_t root_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .size = NULL,
    .seek = NULL,
    .tell = NULL,
    .dir_iter_create = stub_dir_iter_create,
    .dir_iter_next = stub_dir_iter_next,
    .dir_iter_destroy = stub_dir_iter_destroy,
    .dir_create = NULL,
    .dir_remove = NULL
};

static vfs_node_t root_node = {
    .type = VFS_NODE_DIR,
    .ops = &root_ops,
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

vfs_dir_iter_t* vfs_dir_iter_create_node(vfs_node_t *dir_node) {
    if (dir_node == NULL || dir_node->ops == NULL) {
        return NULL;
    }
    
    if (dir_node->ops->dir_iter_create == NULL) {
        return NULL;
    }
    
    return dir_node->ops->dir_iter_create(dir_node);
}

int vfs_dir_iter_next(vfs_dir_iter_t *iter) {
    if (iter == NULL || iter->dir_node == NULL || iter->dir_node->ops == NULL) {
        return -1;
    }
    
    if (iter->dir_node->ops->dir_iter_next == NULL) {
        return -1;
    }
    
    return iter->dir_node->ops->dir_iter_next(iter);
}

void vfs_dir_iter_destroy(vfs_dir_iter_t *iter) {
    if (iter == NULL || iter->dir_node == NULL || iter->dir_node->ops == NULL) {
        return;
    }
    
    if (iter->dir_node->ops->dir_iter_destroy != NULL) {
        iter->dir_node->ops->dir_iter_destroy(iter);
    }
}

