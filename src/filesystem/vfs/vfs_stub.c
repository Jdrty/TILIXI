// temp file just needed for good test
// redundant now, got real vfs working

#include "vfs.h"
#include <stdlib.h>
#include <string.h>

// Directory structure:
// / (root)
//   - file1.txt
//   - file2.txt
//   - dir1/
//     - file3.txt
//     - subdir/
//       - file4.txt

// Root directory entries
static char root_entries[][16] = {"file1.txt", "file2.txt", "dir1"};
#define ROOT_ENTRY_COUNT 3

// dir1 entries
static char dir1_entries[][16] = {"file3.txt", "subdir"};
#define DIR1_ENTRY_COUNT 2

// subdir entries
static char subdir_entries[][16] = {"file4.txt"};
#define SUBDIR_ENTRY_COUNT 1

// iterator state structure
typedef struct {
    vfs_node_t *dir_node;
    int entry_idx;
    char (*entries)[16];
    int entry_count;
} stub_iter_state_t;

static stub_iter_state_t stub_iter_state = {0};

static vfs_dir_iter_t stub_iter = {0};

static vfs_dir_iter_t* stub_dir_iter_create(vfs_node_t *dir_node) {
    if (dir_node == NULL || dir_node->type != VFS_NODE_DIR) {
        return NULL;
    }
    
    void *dir_id = dir_node->backend_data;
    
    stub_iter_state.dir_node = dir_node;
    stub_iter_state.entry_idx = 0;
    
    if (dir_id == (void*)1) {
        // dir1
        stub_iter_state.entries = dir1_entries;
        stub_iter_state.entry_count = DIR1_ENTRY_COUNT;
    } else if (dir_id == (void*)2) {
        // subdir
        stub_iter_state.entries = subdir_entries;
        stub_iter_state.entry_count = SUBDIR_ENTRY_COUNT;
    } else {
        // root (default)
        stub_iter_state.entries = root_entries;
        stub_iter_state.entry_count = ROOT_ENTRY_COUNT;
    }
    
    stub_iter.dir_node = dir_node;
    stub_iter.backend_iter = &stub_iter_state;
    stub_iter.current_name = NULL;
    stub_iter.name_len = 0;
    
    return &stub_iter;
}

static int stub_dir_iter_next(vfs_dir_iter_t *iter) {
    if (iter == NULL || iter->backend_iter == NULL) {
        return -1;
    }
    
    stub_iter_state_t *state = (stub_iter_state_t *)iter->backend_iter;
    
    if (state->entry_idx >= state->entry_count) {
        return 0;  // end of directory
    }
    
    iter->current_name = state->entries[state->entry_idx];
    iter->name_len = strlen(state->entries[state->entry_idx]);
    state->entry_idx++;
    
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
    
    stub_iter_state.dir_node = NULL;
    stub_iter_state.entry_idx = 0;
    stub_iter_state.entries = NULL;
    stub_iter_state.entry_count = 0;
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

// directory nodes
static vfs_node_t root_node = {
    .type = VFS_NODE_DIR,
    .ops = &root_ops,
    .backend_data = NULL,  // root is identified by NULL
    .is_readonly = 0,
    .is_hidden = 0,
    .reserved = 0,
    .refcount = 1
};

static vfs_node_t dir1_node = {
    .type = VFS_NODE_DIR,
    .ops = &root_ops,  // same ops for all directories
    .backend_data = (void*)1,  // dir1 identifier
    .is_readonly = 0,
    .is_hidden = 0,
    .reserved = 0,
    .refcount = 0
};

static vfs_node_t subdir_node = {
    .type = VFS_NODE_DIR,
    .ops = &root_ops,  // same ops for all directories
    .backend_data = (void*)2,  // subdir identifier
    .is_readonly = 0,
    .is_hidden = 0,
    .reserved = 0,
    .refcount = 0
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
    
    // handle absolute paths
    if (strcmp(path, "/dir1") == 0) {
        dir1_node.refcount++;
        return &dir1_node;
    }
    
    if (strcmp(path, "/dir1/subdir") == 0) {
        subdir_node.refcount++;
        return &subdir_node;
    }
    
    return NULL;
}

static int normalize_absolute_path(const char *in_path, char *out_path, size_t out_len) {
    if (in_path == NULL || out_path == NULL || out_len == 0 || in_path[0] != '/') {
        return 0;
    }
    size_t written = 0;
    out_path[written++] = '/';
    out_path[written] = '\0';
    
    const char *p = in_path;
    while (*p == '/') {
        p++;
    }
    
    while (*p != '\0') {
        char segment[64];
        size_t seg_len = 0;
        while (*p != '\0' && *p != '/') {
            if (seg_len + 1 >= sizeof(segment)) {
                return 0;
            }
            segment[seg_len++] = *p++;
        }
        segment[seg_len] = '\0';
        while (*p == '/') {
            p++;
        }
        
        if (seg_len == 0 || strcmp(segment, ".") == 0) {
            continue;
        }
        if (strcmp(segment, "..") == 0) {
            if (written > 1) {
                size_t i = written - 1;
                if (out_path[i] == '/' && i > 0) {
                    i--;
                }
                while (i > 0 && out_path[i] != '/') {
                    i--;
                }
                written = i + 1;
                out_path[written] = '\0';
            }
            continue;
        }
        
        if (written + seg_len + 1 >= out_len) {
            return 0;
        }
        if (written > 1 && out_path[written - 1] != '/') {
            out_path[written++] = '/';
        }
        memcpy(out_path + written, segment, seg_len);
        written += seg_len;
        out_path[written] = '\0';
    }
    
    if (written > 1 && out_path[written - 1] == '/') {
        out_path[written - 1] = '\0';
    }
    return 1;
}

vfs_node_t* vfs_resolve_at(vfs_node_t *base, const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    char raw_path[128];
    if (path[0] == '/') {
        strncpy(raw_path, path, sizeof(raw_path) - 1);
        raw_path[sizeof(raw_path) - 1] = '\0';
    } else {
        const char *base_path = "/";
        if (base != NULL) {
            if (base->backend_data == (void*)1) {
                base_path = "/dir1";
            } else if (base->backend_data == (void*)2) {
                base_path = "/dir1/subdir";
            }
        }
        size_t base_len = strlen(base_path);
        size_t path_len = strlen(path);
        if (base_len + path_len + 2 >= sizeof(raw_path)) {
            return NULL;
        }
        strncpy(raw_path, base_path, sizeof(raw_path) - 1);
        raw_path[sizeof(raw_path) - 1] = '\0';
        if (base_len == 0 || raw_path[base_len - 1] != '/') {
            raw_path[base_len] = '/';
            raw_path[base_len + 1] = '\0';
            base_len++;
        }
        strncpy(raw_path + base_len, path, sizeof(raw_path) - base_len - 1);
        raw_path[base_len + path_len] = '\0';
    }
    
    char normalized[128];
    if (!normalize_absolute_path(raw_path, normalized, sizeof(normalized))) {
        return NULL;
    }
    return vfs_resolve(normalized);
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

vfs_node_t* vfs_dir_create_node(vfs_node_t *dir_node, const char *name, vfs_node_type_t type) {
    if (dir_node == NULL || dir_node->ops == NULL) {
        return NULL;
    }
    
    if (dir_node->ops->dir_create == NULL) {
        return NULL;
    }
    
    return dir_node->ops->dir_create(dir_node, name, type);
}

int vfs_dir_remove_node(vfs_node_t *dir_node, const char *name) {
    if (dir_node == NULL || dir_node->ops == NULL || name == NULL) {
        return VFS_EINVAL;
    }
    
    if (dir_node->ops->dir_remove == NULL) {
        return VFS_EPERM;  // operation not supported in stub
    }
    
    return dir_node->ops->dir_remove(dir_node, name);
}

int vfs_dir_rename_node(vfs_node_t *old_dir, const char *old_name,
                        vfs_node_t *new_dir, const char *new_name) {
    if (old_dir == NULL || new_dir == NULL || old_name == NULL || new_name == NULL) {
        return VFS_EINVAL;
    }
    
    return VFS_EPERM;  // operation not supported in stub
}

vfs_file_t* vfs_open(const char *path, int flags) {
    (void)path;
    (void)flags;
    return NULL;
}

vfs_file_t* vfs_open_node(vfs_node_t *node, int flags) {
    (void)node;
    (void)flags;
    return NULL;
}

int vfs_close(vfs_file_t *file) {
    (void)file;
    return VFS_EPERM;
}

ssize_t vfs_read(vfs_file_t *file, void *buf, size_t size) {
    (void)file;
    (void)buf;
    (void)size;
    return VFS_EPERM;
}

ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t size) {
    (void)file;
    (void)buf;
    (void)size;
    return VFS_EPERM;
}

ssize_t vfs_size(const char *path) {
    (void)path;
    return VFS_EPERM;
}

ssize_t vfs_size_node(vfs_node_t *node) {
    (void)node;
    return VFS_EPERM;
}

int vfs_seek(vfs_file_t *file, size_t offset) {
    (void)file;
    (void)offset;
    return VFS_EPERM;
}

ssize_t vfs_tell(vfs_file_t *file) {
    (void)file;
    return VFS_EPERM;
}

