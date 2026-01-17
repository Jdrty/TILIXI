#ifdef ARDUINO
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "vfs.h"
#include "boot_sequence.h"
#include "debug_helper.h"
#include <stdlib.h>
#include <string.h>

// maximum path length
#define MAX_PATH_LEN 256
#define MAX_ENTRY_NAME_LEN 64

// directory iterator state
typedef struct {
    File dir_file;  // SD File handle for directory (kept open for iteration)
    char current_name_buffer[MAX_ENTRY_NAME_LEN];
    int initialized;
} sd_dir_iter_state_t;

static sd_dir_iter_state_t iter_states[4];  // support up to 4 concurrent iterators
static int iter_state_count = 0;

// find or allocate an iterator state
static sd_dir_iter_state_t* alloc_iter_state(void) {
    for (int i = 0; i < 4; i++) {
        if (!iter_states[i].initialized) {
            iter_states[i].initialized = 1;
            memset(iter_states[i].current_name_buffer, 0, MAX_ENTRY_NAME_LEN);
            return &iter_states[i];
        }
    }
    return NULL;  // no free iterator states
}

static void free_iter_state(sd_dir_iter_state_t *state) {
    if (state) {
        if (state->dir_file) {
            // SPI should already be in SD mode, but ensure it is before closing
            boot_sd_switch_to_sd_spi();
            state->dir_file.close();
            boot_sd_restore_tft_spi();  // restore TFT SPI after closing
        }
        memset(state, 0, sizeof(sd_dir_iter_state_t));
    }
}

// VFS directory iterator implementation for SD card
static vfs_dir_iter_t* sd_dir_iter_create(vfs_node_t *dir_node) {
    if (dir_node == NULL || dir_node->type != VFS_NODE_DIR) {
        return NULL;
    }
    
    boot_sd_switch_to_sd_spi();
    
    // get the path from backend_data (we'll store path there)
    const char *path = (const char*)dir_node->backend_data;
    if (path == NULL) {
        path = "/";  // default to root
    }
    
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        boot_sd_restore_tft_spi();  // restore TFT SPI before returning
        return NULL;
    }
    
    // allocate iterator state
    sd_dir_iter_state_t *state = alloc_iter_state();
    if (state == NULL) {
        dir.close();
        boot_sd_restore_tft_spi();  // restore TFT SPI before returning
        return NULL;
    }
    
    // store the directory File handle - keep it open for iteration
    // we'll keep SPI in SD mode while iterator is active to avoid File handle invalidation
    state->dir_file = dir;
    
    // allocate and initialize iterator
    vfs_dir_iter_t *iter = (vfs_dir_iter_t*)malloc(sizeof(vfs_dir_iter_t));
    if (iter == NULL) {
        state->dir_file.close();
        free_iter_state(state);
        boot_sd_restore_tft_spi();  // restore TFT SPI before returning
        return NULL;
    }
    
    iter->dir_node = dir_node;
    iter->backend_iter = state;
    iter->current_name = NULL;
    iter->name_len = 0;
    
    return iter;
}

static int sd_dir_iter_next(vfs_dir_iter_t *iter) {
    if (iter == NULL || iter->backend_iter == NULL) {
        return -1;
    }
    
    sd_dir_iter_state_t *state = (sd_dir_iter_state_t*)iter->backend_iter;
    
    // SPI should already be in SD mode from create(), but ensure it is
    boot_sd_switch_to_sd_spi();
    
    File entry = state->dir_file.openNextFile();
    if (!entry) {
        // end of directory
        return 0;
    }
    
    // get entry name
    char name_buf[MAX_ENTRY_NAME_LEN];
    const char *raw_name = entry.name();
    if (raw_name == NULL) {
        entry.close();
        return -1;
    }
    
    // copy the raw name to a local buffer before closing the entry
    strncpy(name_buf, raw_name, MAX_ENTRY_NAME_LEN - 1);
    name_buf[MAX_ENTRY_NAME_LEN - 1] = '\0';
    
    // close the entry now - the name is safely copied
    entry.close();
    
    // extract just the filename (SD library sometimes returns full path)
    // find the last '/' to get just the filename part
    const char *filename = strrchr(name_buf, '/');
    if (filename == NULL) {
        // no slash found, use the whole name
        filename = name_buf;
    } else {
        // skip the '/'
        filename++;
    }
    
    // Validate filename is not empty
    if (filename == NULL || filename[0] == '\0') {
        return sd_dir_iter_next(iter);  // skip empty filenames, get next entry
    }
    
    // skip "." and ".." entries
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return sd_dir_iter_next(iter);  // get next entry (recursive call)
    }
    
    // copy name to buffer
    size_t name_len = strlen(filename);
    if (name_len == 0 || name_len >= MAX_ENTRY_NAME_LEN) {
        if (name_len >= MAX_ENTRY_NAME_LEN) {
            name_len = MAX_ENTRY_NAME_LEN - 1;
        } else {
            // empty filename, skip it
            return sd_dir_iter_next(iter);
        }
    }
    strncpy(state->current_name_buffer, filename, name_len);
    state->current_name_buffer[name_len] = '\0';
    
    iter->current_name = state->current_name_buffer;
    iter->name_len = name_len;
    
    return 1;
}

static void sd_dir_iter_destroy(vfs_dir_iter_t *iter) {
    if (iter == NULL || iter->backend_iter == NULL) {
        return;
    }
    
    sd_dir_iter_state_t *state = (sd_dir_iter_state_t*)iter->backend_iter;
    // close file if still open (free_iter_state will handle SPI switching)
    free_iter_state(state);
    // note: iter structure itself is freed by vfs_dir_iter_destroy wrapper
}

// forward declaration
static vfs_node_t* create_sd_node(const char *path, vfs_node_type_t type);

// VFS directory create implementation for SD card
static vfs_node_t* sd_dir_create(vfs_node_t *dir_node, const char *name, vfs_node_type_t type) {
    if (dir_node == NULL || dir_node->type != VFS_NODE_DIR || name == NULL) {
        return NULL;
    }
    
    boot_sd_switch_to_sd_spi();
    
    // get the directory path from backend_data
    const char *dir_path = (const char*)dir_node->backend_data;
    if (dir_path == NULL) {
        dir_path = "/";
    }
    
    // construct full path for the new entry
    char full_path[MAX_PATH_LEN];
    size_t dir_path_len = strlen(dir_path);
    
    if (dir_path_len + strlen(name) + 2 >= MAX_PATH_LEN) {
        boot_sd_restore_tft_spi();
        return NULL;  // path too long
    }
    
    strncpy(full_path, dir_path, MAX_PATH_LEN - 1);
    
    // add '/' if directory path doesn't end with one (unless it's root)
    if (dir_path_len > 0 && dir_path[dir_path_len - 1] != '/') {
        full_path[dir_path_len] = '/';
        dir_path_len++;
    }
    
    strncpy(full_path + dir_path_len, name, MAX_PATH_LEN - dir_path_len - 1);
    full_path[dir_path_len + strlen(name)] = '\0';
    
    // check if entry already exists
    if (SD.exists(full_path)) {
        // entry exists - resolve and return it
        boot_sd_restore_tft_spi();
        return vfs_resolve(full_path);
    }
    
    // create the entry
    bool success = false;
    if (type == VFS_NODE_DIR) {
        // create directory
        success = SD.mkdir(full_path);
    } else if (type == VFS_NODE_FILE) {
        // create file - open in write mode and close it
        // the SD library creates the file entry when you open for writing
        File f = SD.open(full_path, FILE_WRITE);
        if (f) {
            f.close();
            success = true;
        }
    }
    
    boot_sd_restore_tft_spi();
    
    if (!success) {
        return NULL;
    }
    
    // create and return a node for the new entry
    return create_sd_node(full_path, type);
}

static int sd_dir_remove(vfs_node_t *dir_node, const char *name) {
    if (dir_node == NULL || dir_node->type != VFS_NODE_DIR || name == NULL || name[0] == '\0') {
        return VFS_EINVAL;
    }
    
    boot_sd_switch_to_sd_spi();
    
    char full_path[MAX_PATH_LEN];
    size_t name_len = strlen(name);
    const char *dir_path = (const char*)dir_node->backend_data;
    if (dir_path == NULL) {
        dir_path = "/";
    }
    
    size_t dir_path_len = strlen(dir_path);
    if (dir_path_len + name_len + 2 >= MAX_PATH_LEN) {
        boot_sd_restore_tft_spi();
        return VFS_ENAMETOOLONG;
    }
    
    strncpy(full_path, dir_path, MAX_PATH_LEN - 1);
    if (dir_path_len > 0 && dir_path[dir_path_len - 1] != '/') {
        full_path[dir_path_len] = '/';
        dir_path_len++;
    }
    
    strncpy(full_path + dir_path_len, name, MAX_PATH_LEN - dir_path_len - 1);
    full_path[dir_path_len + name_len] = '\0';
    
    if (!SD.exists(full_path)) {
        boot_sd_restore_tft_spi();
        return VFS_ENOENT;
    }
    
    File f = SD.open(full_path);
    if (!f) {
        boot_sd_restore_tft_spi();
        return VFS_EIO;
    }
    
    bool is_dir = f.isDirectory();
    f.close();
    
    bool success = false;
    if (is_dir) {
        success = SD.rmdir(full_path);
    } else {
        success = SD.remove(full_path);
    }
    
    boot_sd_restore_tft_spi();
    
    return success ? VFS_EOK : VFS_EPERM;
}

static int sd_dir_rename(vfs_node_t *old_dir, const char *old_name,
                         vfs_node_t *new_dir, const char *new_name) {
    if (old_dir == NULL || new_dir == NULL || old_name == NULL || new_name == NULL) {
        return VFS_EINVAL;
    }
    
    const char *old_dir_path = (const char*)old_dir->backend_data;
    if (old_dir_path == NULL) {
        old_dir_path = "/";
    }
    const char *new_dir_path = (const char*)new_dir->backend_data;
    if (new_dir_path == NULL) {
        new_dir_path = "/";
    }
    
    size_t old_dir_len = strlen(old_dir_path);
    size_t new_dir_len = strlen(new_dir_path);
    size_t old_name_len = strlen(old_name);
    size_t new_name_len = strlen(new_name);
    
    if (old_dir_len + old_name_len + 2 >= MAX_PATH_LEN ||
        new_dir_len + new_name_len + 2 >= MAX_PATH_LEN) {
        return VFS_ENAMETOOLONG;
    }
    
    char old_full[MAX_PATH_LEN];
    char new_full[MAX_PATH_LEN];
    
    strncpy(old_full, old_dir_path, MAX_PATH_LEN - 1);
    if (old_dir_len > 0 && old_dir_path[old_dir_len - 1] != '/') {
        old_full[old_dir_len] = '/';
        old_dir_len++;
    }
    strncpy(old_full + old_dir_len, old_name, MAX_PATH_LEN - old_dir_len - 1);
    old_full[old_dir_len + old_name_len] = '\0';
    
    strncpy(new_full, new_dir_path, MAX_PATH_LEN - 1);
    if (new_dir_len > 0 && new_dir_path[new_dir_len - 1] != '/') {
        new_full[new_dir_len] = '/';
        new_dir_len++;
    }
    strncpy(new_full + new_dir_len, new_name, MAX_PATH_LEN - new_dir_len - 1);
    new_full[new_dir_len + new_name_len] = '\0';
    
    boot_sd_switch_to_sd_spi();
    bool success = SD.rename(old_full, new_full);
    boot_sd_restore_tft_spi();
    
    return success ? VFS_EOK : VFS_EPERM;
}

// VFS operations table for SD filesystem
static void* sd_open(vfs_node_t *node, int flags) {
    if (node == NULL || node->type != VFS_NODE_FILE || node->backend_data == NULL) {
        return NULL;
    }
    
    const char *path = (const char*)node->backend_data;
    if (path[0] == '\0') {
        return NULL;
    }
    
    boot_sd_switch_to_sd_spi();
    
    const char *mode = (flags & (VFS_O_WRITE | VFS_O_CREATE)) ? FILE_WRITE : FILE_READ;
    bool create = (flags & VFS_O_CREATE) != 0;
    
    if ((flags & VFS_O_TRUNC) && (flags & (VFS_O_WRITE | VFS_O_CREATE))) {
        if (SD.exists(path)) {
            SD.remove(path);
        }
        create = true;
    }
    
    File *handle = new File(SD.open(path, mode, create));
    if (handle == NULL || !(*handle)) {
        if (handle != NULL) {
            delete handle;
        }
        boot_sd_restore_tft_spi();
        return NULL;
    }
    
    if (flags & VFS_O_APPEND) {
        handle->seek(handle->size());
    } else if (flags & VFS_O_WRITE) {
        handle->seek(0);
    }
    
    boot_sd_restore_tft_spi();
    return handle;
}

static int sd_close(void *handle) {
    if (handle == NULL) {
        return VFS_EINVAL;
    }
    File *file = (File*)handle;
    boot_sd_switch_to_sd_spi();
    file->close();
    boot_sd_restore_tft_spi();
    delete file;
    return VFS_EOK;
}

static ssize_t sd_read(void *handle, void *buf, size_t size) {
    if (handle == NULL || buf == NULL) {
        return VFS_EINVAL;
    }
    File *file = (File*)handle;
    boot_sd_switch_to_sd_spi();
    int read_bytes = file->read((uint8_t*)buf, size);
    boot_sd_restore_tft_spi();
    return read_bytes < 0 ? VFS_EIO : read_bytes;
}

static ssize_t sd_write(void *handle, const void *buf, size_t size) {
    if (handle == NULL || buf == NULL) {
        return VFS_EINVAL;
    }
    File *file = (File*)handle;
    boot_sd_switch_to_sd_spi();
    size_t written = file->write((const uint8_t*)buf, size);
    file->flush();
    boot_sd_restore_tft_spi();
    return written == size ? (ssize_t)written : VFS_EIO;
}

static ssize_t sd_size(vfs_node_t *node) {
    if (node == NULL || node->backend_data == NULL) {
        return VFS_EINVAL;
    }
    const char *path = (const char*)node->backend_data;
    boot_sd_switch_to_sd_spi();
    File f = SD.open(path, FILE_READ);
    if (!f) {
        boot_sd_restore_tft_spi();
        return VFS_EIO;
    }
    size_t size = f.size();
    f.close();
    boot_sd_restore_tft_spi();
    return (ssize_t)size;
}

static int sd_seek(void *handle, size_t offset) {
    if (handle == NULL) {
        return VFS_EINVAL;
    }
    File *file = (File*)handle;
    boot_sd_switch_to_sd_spi();
    bool ok = file->seek(offset);
    boot_sd_restore_tft_spi();
    return ok ? VFS_EOK : VFS_EIO;
}

static ssize_t sd_tell(void *handle) {
    if (handle == NULL) {
        return VFS_EINVAL;
    }
    File *file = (File*)handle;
    boot_sd_switch_to_sd_spi();
    size_t pos = file->position();
    boot_sd_restore_tft_spi();
    return (ssize_t)pos;
}

static const vfs_ops_t sd_ops = {
    .open = sd_open,
    .close = sd_close,
    .read = sd_read,
    .write = sd_write,
    .size = sd_size,
    .seek = sd_seek,
    .tell = sd_tell,
    .dir_iter_create = sd_dir_iter_create,
    .dir_iter_next = sd_dir_iter_next,
    .dir_iter_destroy = sd_dir_iter_destroy,
    .dir_create = sd_dir_create,
    .dir_remove = sd_dir_remove
};

static int normalize_absolute_path(const char *in_path, char *out_path) {
    if (in_path == NULL || out_path == NULL || in_path[0] != '/') {
        return 0;
    }
    size_t out_len = 0;
    out_path[out_len++] = '/';
    out_path[out_len] = '\0';
    
    const char *p = in_path;
    while (*p == '/') {
        p++;
    }
    
    while (*p != '\0') {
        char segment[MAX_PATH_LEN];
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
            if (out_len > 1) {
                size_t i = out_len - 1;
                if (out_path[i] == '/' && i > 0) {
                    i--;
                }
                while (i > 0 && out_path[i] != '/') {
                    i--;
                }
                out_len = i + 1;
                out_path[out_len] = '\0';
            }
            continue;
        }
        
        if (out_len + seg_len + 1 >= MAX_PATH_LEN) {
            return 0;
        }
        if (out_len > 1 && out_path[out_len - 1] != '/') {
            out_path[out_len++] = '/';
        }
        memcpy(out_path + out_len, segment, seg_len);
        out_len += seg_len;
        out_path[out_len] = '\0';
    }
    
    if (out_len > 1 && out_path[out_len - 1] == '/') {
        out_path[out_len - 1] = '\0';
    }
    return 1;
}

// helper to get full path from base and relative path
// returns a pointer to a path stored in the node cache (persists for node lifetime)
static const char* resolve_path_to_node_cache(vfs_node_t *base, const char *path, char *out_path) {
    if (path == NULL || out_path == NULL) {
        return NULL;
    }
    
    char raw_path[MAX_PATH_LEN];
    if (path[0] == '/') {
        if (strlen(path) >= sizeof(raw_path)) {
            return NULL;
        }
        strncpy(raw_path, path, sizeof(raw_path) - 1);
        raw_path[sizeof(raw_path) - 1] = '\0';
    } else {
        const char *base_path = "/";
        if (base != NULL && base->backend_data != NULL) {
            base_path = (const char*)base->backend_data;
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
    
    if (!normalize_absolute_path(raw_path, out_path)) {
        return NULL;
    }
    return out_path;
}

// node storage (we need to track nodes with their paths)
#define MAX_NODES 32
static struct {
    vfs_node_t node;
    char path[MAX_PATH_LEN];
    int in_use;
} node_cache[MAX_NODES];

static vfs_node_t* create_sd_node(const char *path, vfs_node_type_t type) {
    // find free slot in node cache
    int slot = -1;
    for (int i = 0; i < MAX_NODES; i++) {
        if (!node_cache[i].in_use) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL;  // no free slots
    }
    
    // store path and create node
    strncpy(node_cache[slot].path, path, MAX_PATH_LEN - 1);
    node_cache[slot].path[MAX_PATH_LEN - 1] = '\0';
    
    node_cache[slot].node.type = type;
    node_cache[slot].node.ops = &sd_ops;
    node_cache[slot].node.backend_data = (void*)node_cache[slot].path;
    node_cache[slot].node.is_readonly = 0;
    node_cache[slot].node.is_hidden = 0;
    node_cache[slot].node.reserved = 0;
    node_cache[slot].node.refcount = 1;
    node_cache[slot].in_use = 1;
    
    return &node_cache[slot].node;
}

int vfs_init(void) {
    // initialize node cache
    memset(node_cache, 0, sizeof(node_cache));
    memset(iter_states, 0, sizeof(iter_states));
    
    return VFS_EOK;
}

vfs_node_t* vfs_resolve(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    // check if node already exists in cache
    for (int i = 0; i < MAX_NODES; i++) {
        if (node_cache[i].in_use && strcmp(node_cache[i].path, path) == 0) {
            node_cache[i].node.refcount++;
            return &node_cache[i].node;
        }
    }
    
    boot_sd_switch_to_sd_spi();
    
    if (!SD.exists(path)) {
        boot_sd_restore_tft_spi();  // restore TFT SPI before returning
        return NULL;
    }
    
    File f = SD.open(path);
    if (!f) {
        boot_sd_restore_tft_spi();  // restore TFT SPI before returning
        return NULL;
    }
    
    vfs_node_type_t type = f.isDirectory() ? VFS_NODE_DIR : VFS_NODE_FILE;
    f.close();
    
    vfs_node_t* node = create_sd_node(path, type);
    
    boot_sd_restore_tft_spi();  // restore TFT SPI after SD operations complete
    
    return node;
}

vfs_node_t* vfs_resolve_at(vfs_node_t *base, const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    // find a free node cache slot to store the resolved path
    char temp_path[MAX_PATH_LEN];
    const char *full_path = resolve_path_to_node_cache(base, path, temp_path);
    if (full_path == NULL) {
        return NULL;
    }
    
    // check if node already exists in cache
    for (int i = 0; i < MAX_NODES; i++) {
        if (node_cache[i].in_use && strcmp(node_cache[i].path, full_path) == 0) {
            node_cache[i].node.refcount++;
            return &node_cache[i].node;
        }
    }
    
    // resolve using the temporary path (will be copied to node cache)
    return vfs_resolve(full_path);
}

void vfs_node_release(vfs_node_t *node) {
    if (node == NULL) {
        return;
    }
    
    if (node->refcount > 0) {
        node->refcount--;
        
        // if refcount reaches 0, mark node cache slot as free
        if (node->refcount == 0) {
            for (int i = 0; i < MAX_NODES; i++) {
                if (&node_cache[i].node == node) {
                    node_cache[i].in_use = 0;
                    break;
                }
            }
        }
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
    
    // free the iterator structure itself
    free(iter);
}

vfs_node_t* vfs_dir_create_node(vfs_node_t *dir_node, const char *name, vfs_node_type_t type) {
    if (dir_node == NULL || dir_node->ops == NULL) {
        return NULL;
    }
    
    if (dir_node->ops->dir_create == NULL) {
        return NULL;
    }
    
    return dir_node->ops->dir_create(dir_node, name, type);

    DEBUG_PRINT("VFS create: name='%s' ptr=%p\n", name, name);  // trying to figure out why touch doesnt work
}

int vfs_dir_remove_node(vfs_node_t *dir_node, const char *name) {
    if (dir_node == NULL || dir_node->ops == NULL || name == NULL) {
        return VFS_EINVAL;
    }
    
    if (dir_node->ops->dir_remove == NULL) {
        return VFS_EPERM;
    }
    
    return dir_node->ops->dir_remove(dir_node, name);
}

int vfs_dir_rename_node(vfs_node_t *old_dir, const char *old_name,
                        vfs_node_t *new_dir, const char *new_name) {
    if (old_dir == NULL || new_dir == NULL || old_name == NULL || new_name == NULL) {
        return VFS_EINVAL;
    }
    
    if (old_dir->ops == NULL || old_dir->ops != new_dir->ops) {
        return VFS_EPERM;
    }
    
    if (old_dir->ops->dir_remove == NULL) {
        return VFS_EPERM;
    }
    
    return sd_dir_rename(old_dir, old_name, new_dir, new_name);
}

vfs_file_t* vfs_open(const char *path, int flags) {
    vfs_node_t *node = vfs_resolve(path);
    if (node == NULL) {
        return NULL;
    }
    
    vfs_file_t *file = vfs_open_node(node, flags);
    vfs_node_release(node);
    return file;
}

vfs_file_t* vfs_open_node(vfs_node_t *node, int flags) {
    if (node == NULL || node->ops == NULL || node->ops->open == NULL) {
        return NULL;
    }
    
    void *handle = node->ops->open(node, flags);
    if (handle == NULL) {
        return NULL;
    }
    
    vfs_file_t *file = (vfs_file_t*)malloc(sizeof(vfs_file_t));
    if (file == NULL) {
        if (node->ops->close != NULL) {
            node->ops->close(handle);
        }
        return NULL;
    }
    
    node->refcount++;
    file->node = node;
    file->handle = handle;
    file->position = 0;
    return file;
}

int vfs_close(vfs_file_t *file) {
    if (file == NULL || file->node == NULL || file->node->ops == NULL) {
        return VFS_EINVAL;
    }
    
    int result = VFS_EOK;
    if (file->node->ops->close != NULL) {
        result = file->node->ops->close(file->handle);
    }
    vfs_node_release(file->node);
    free(file);
    return result;
}

ssize_t vfs_read(vfs_file_t *file, void *buf, size_t size) {
    if (file == NULL || file->node == NULL || file->node->ops == NULL ||
        file->node->ops->read == NULL) {
        return VFS_EINVAL;
    }
    
    ssize_t result = file->node->ops->read(file->handle, buf, size);
    if (result > 0) {
        file->position += (size_t)result;
    }
    return result;
}

ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t size) {
    if (file == NULL || file->node == NULL || file->node->ops == NULL ||
        file->node->ops->write == NULL) {
        return VFS_EINVAL;
    }
    
    ssize_t result = file->node->ops->write(file->handle, buf, size);
    if (result > 0) {
        file->position += (size_t)result;
    }
    return result;
}

ssize_t vfs_size(const char *path) {
    vfs_node_t *node = vfs_resolve(path);
    if (node == NULL) {
        return VFS_ENOENT;
    }
    ssize_t size = vfs_size_node(node);
    vfs_node_release(node);
    return size;
}

ssize_t vfs_size_node(vfs_node_t *node) {
    if (node == NULL || node->ops == NULL || node->ops->size == NULL) {
        return VFS_EINVAL;
    }
    return node->ops->size(node);
}

int vfs_seek(vfs_file_t *file, size_t offset) {
    if (file == NULL || file->node == NULL || file->node->ops == NULL ||
        file->node->ops->seek == NULL) {
        return VFS_EINVAL;
    }
    int result = file->node->ops->seek(file->handle, offset);
    if (result == VFS_EOK) {
        file->position = offset;
    }
    return result;
}

ssize_t vfs_tell(vfs_file_t *file) {
    if (file == NULL || file->node == NULL || file->node->ops == NULL ||
        file->node->ops->tell == NULL) {
        return VFS_EINVAL;
    }
    return file->node->ops->tell(file->handle);
}

#endif  // ARDUINO

