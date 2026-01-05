#pragma once

/*
 *   IMPORTANT GLOBAL RULE - MUST BE ENFORCED FOR ALL FILESYSTEM ACCESS (ノ＞＜)ノ
 *   all filesystem access MUST go through the VFS.
 * 
 *   no command or subsystem may touch FAT directly.
 *   no direct SD library calls outside of VFS.
 *   all file/directory operations must use VFS APIs.
 * 
 *   MUST HAPPEN ALWAYS:
 *      every path resolution returns:
 *          vfs_node_t (node structure)
 *          fixed ops table (operations for this node type)
 *          immutable node type (cannot change)
 * 
 *   afterward, no further checks are required.
 *   the resolved node contains all information needed for operations.
 * 
 *   this rule applies to ALL code in the project
 */

// this file contains lots of documentation as it's likely the most important standalone file in the entire project
// and I'll likely forget a lot of this stuff if I ever revisit this project (⌒_⌒;)
// if you're just reading through this, much of the comments become redundant once you recognize the convention
// but I wanted to keep each function readable in isolation.
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// ssize_t definition (signed size_t, used for operations that can return size or error)
#ifndef _SSIZE_T_DEFINED
typedef long ssize_t;
#define _SSIZE_T_DEFINED
#endif

// error Codes
// standardized error codes used throughout VFS API
#define VFS_EOK        0           // success
#define VFS_EPERM      -1          // operation not permitted
#define VFS_ENOENT     -2          // no such file or directory
#define VFS_EIO        -5          // input/output error
#define VFS_ENXIO      -6          // no such device or address
#define VFS_E2BIG      -7          // argument list too long
#define VFS_EBADF      -9          // bad file descriptor
#define VFS_EAGAIN     -11         // resource temporarily unavailable
#define VFS_ENOMEM     -12         // cannot allocate memory
#define VFS_EACCES     -13         // permission denied
#define VFS_EFAULT     -14         // bad address
#define VFS_EBUSY      -16         // device or resource busy
#define VFS_EEXIST     -17         // file exists
#define VFS_ENODEV     -19         // no such device
#define VFS_ENOTDIR    -20         // not a directory
#define VFS_EISDIR     -21         // is a directory
#define VFS_EINVAL     -22         // invalid argument
#define VFS_ENFILE     -23         // too many open files in system
#define VFS_EMFILE     -24         // too many open files
#define VFS_ENOTTY     -25         // inappropriate ioctl for device
#define VFS_EFBIG      -27         // file too large
#define VFS_ENOSPC     -28         // no space left on device
#define VFS_ESPIPE     -29         // illegal seek
#define VFS_EROFS      -30         // read-only file system
#define VFS_EMLINK     -31         // too many links
#define VFS_EPIPE      -32         // broken pipe
#define VFS_EDOM       -33         // numerical argument out of domain
#define VFS_ERANGE     -34         // numerical result out of range
#define VFS_ENAMETOOLONG -36       // file name too long

// VFS node types (immutable after resolution)
typedef enum {
    VFS_NODE_FILE,      // regular file
    VFS_NODE_DIR,       // directory
    VFS_NODE_DEV,       // device file (special semantics - see notes)
    VFS_NODE_PROC,      // proc filesystem entry (special semantics - see notes)
    VFS_NODE_INVALID    // invalid/not found
} vfs_node_type_t;

// gile open flags (bitflags)
#define VFS_O_READ     (1 << 0)   // open for reading
#define VFS_O_WRITE    (1 << 1)   // open for writing
#define VFS_O_APPEND   (1 << 2)   // append mode
#define VFS_O_TRUNC    (1 << 3)   // truncate on open
#define VFS_O_CREATE   (1 << 4)   // create if not exists

// forward declarations
typedef struct vfs_node vfs_node_t;
typedef struct vfs_ops vfs_ops_t;
typedef struct vfs_dir_iter vfs_dir_iter_t;
typedef struct vfs_mount vfs_mount_t;

// file handle for operations
typedef struct {
    vfs_node_t *node;   // resolved node (pointer to stable node)
    void *handle;       // backend-specific handle
    size_t position;    // current position in file
} vfs_file_t;

// directory iterator (for listing directory contents)
struct vfs_dir_iter {
    vfs_node_t *dir_node;   // directory node being iterated
    void *backend_iter;     // backend-specific iterator state
    char *current_name;     // current entry name (owned by iterator)
    size_t name_len;        // length of current name
};

// operations table (fixed per node type)
// NOTE: regarding special node types (VFS_NODE_DEV, VFS_NODE_PROC), ops may be NULL
// for operations that don't apply.
// these must be checked once at resolution, after resolution, no further checks required.
typedef struct vfs_ops {
    // open file (returns handle, NULL on error)
    // flags: VFS_O_READ, VFS_O_WRITE, etc.
    // could be NULL for node types that cannot be opened
    void* (*open)(vfs_node_t *node, int flags);
    
    // close file/directory
    // returns: VFS_EOK on success, negative error code on failure
    int (*close)(void *handle);
    
    // read from file
    // returns: number of bytes read, 0 on EOF, error code on failure
    ssize_t (*read)(void *handle, void *buf, size_t size);
    
    // write to file
    // returns number of bytes written, error code on failure
    // can block or trigger actions for device files
    ssize_t (*write)(void *handle, const void *buf, size_t size);
    
    // get file size
    // returns: file size in bytes, negative error code on failure
    // can be NULL for node types where size is not meaningful like devices or something akin
    ssize_t (*size)(vfs_node_t *node);
    
    // seek in file
    // returns: VFS_EOK on success, error code on failure
    // may be NULL for node types where seek is illegal
    int (*seek)(void *handle, size_t offset);
    
    // get current position
    // returns: current position, error code on failure
    ssize_t (*tell)(void *handle);
    
    // directory operations (only valid for VFS_NODE_DIR)
    // create iterator for directory listing
    // returns iterator handle, NULL on error
    vfs_dir_iter_t* (*dir_iter_create)(vfs_node_t *dir_node);
    
    // get next entry in directory
    // returns: 1 if entry available, 0 if end, negative error code on error
    // entry name is stored in iter->current_name
    int (*dir_iter_next)(vfs_dir_iter_t *iter);
    
    // destroy directory iterator
    void (*dir_iter_destroy)(vfs_dir_iter_t *iter);
    
    // create file/directory in this directory
    // name: entry name (not full path)
    // type: VFS_NODE_FILE or VFS_NODE_DIR
    // returns: pointer to created node, NULL on error (sets errno)
    vfs_node_t* (*dir_create)(vfs_node_t *dir_node, const char *name, vfs_node_type_t type);
    
    // remove file/directory from this directory
    // name: entry name (not full path)
    // returns: VFS_EOK on success, negative error code on failure
    int (*dir_remove)(vfs_node_t *dir_node, const char *name);
} vfs_ops_t;

// VFS mount point structure
// mounts define how different filesystems are integrated into the VFS namespace
// required for the implementation of /proc, /dev, /sd.
struct vfs_mount {
    const char *mount_point;    // mount point path
    vfs_node_t *root;           // root node of mounted filesystem
    const vfs_ops_t *ops;       // operations table for this filesystem
    void *mount_data;           // filesystem-specific mount data (opaque)
};

// VFS node structure (immutable after resolution, owned by VFS)
// nodes represent capabilities and are stable, not copied
struct vfs_node {
    vfs_node_type_t type;       // node type (immutable)
    const vfs_ops_t *ops;       // operations table (fixed per type, may be NULL for special ops)
    void *backend_data;         // backend-specific data (opaque)
    
    // immutable flags
    uint8_t is_readonly:1;      // read-only node
    uint8_t is_hidden:1;        // hidden file
    uint8_t reserved:6;         // reserved bits
    
    // internal state (managed by VFS)
    uint32_t refcount;          // reference count
    // path is NOT stored here, paths are context, not identifiers
};


// VFS API Functions
// init VFS system
// returns VFS_EOK on success, error code on failure
int vfs_init(void);

// mount a filesystem at a mount point
// mount_point: path where filesystem should be mounted
// root: root node of the filesystem to mount
// ops: operations table for this filesystem
// mount_data: filesystem-specific mount data (opaque)
// returns VFS_EOK on success, error code on failure; for example it could return VFS_EEXIST if mount point exists
int vfs_mount(const char *mount_point, vfs_node_t *root, const vfs_ops_t *ops, void *mount_data);

// unmount a filesystem
// mount_point: path where filesystem is mounted
// returns: VFS_EOK on success, negative error code on failure (e.g., VFS_ENOENT if not mounted)
int vfs_umount(const char *mount_point);

// resolve a path to a VFS node
// path: filesystem path
// returns: pointer to resolved node, or NULL if not found/invalid
// the returned node is owned by VFS and is stable (not copied)
// after this call, the node contains all information needed (type, ops, etc.)
// no further checks are required, the ops table is fixed and type is immutable
// existence is determined by resolution - if node is NULL, path doesn't exist
// NOTE: for special node types (VFS_NODE_DEV, VFS_NODE_PROC), ops may have NULL
// entries for operations that don't apply. Check ops once at resolution.
vfs_node_t* vfs_resolve(const char *path);

// resolve a path relative to a base node
// base: base directory node (if NULL, resolves from root)
// path: filesystem path (if starts with '/', resolves from root; else relative to base)
// returns: pointer to resolved node, or NULL if not found/invalid
// the returned node is owned by VFS and must be released with vfs_node_release
// handles '.' and '..' components
vfs_node_t* vfs_resolve_at(vfs_node_t *base, const char *path);

// resolve parent directory and entry name from a path
// this is useful for create/remove operations to avoid double parsing and race conditions
// path: filesystem path
// parent: output pointer to parent directory node (must be released with vfs_node_release)
// name: output pointer to entry name (points into path string, valid until path is modified)
// returns: VFS_EOK on success, negative error code on failure
// for example: vfs_resolve_parent("/foo/bar/baz", &parent, &name)
//          -> parent points to "/foo/bar" node, name points to "baz"
int vfs_resolve_parent(const char *path, vfs_node_t **parent, const char **name);

// release a node reference (call when done with node)
// nodes are reference-counted and will be freed when refcount reaches 0
void vfs_node_release(vfs_node_t *node);

// open a file (uses resolved node's ops table)
// path: filesystem path
// flags: VFS_O_READ, VFS_O_WRITE, VFS_O_APPEND, VFS_O_TRUNC, VFS_O_CREATE
// returns: vfs_file_t handle, or NULL on error
vfs_file_t* vfs_open(const char *path, int flags);

// open a file from a resolved node (avoids re-resolution)
// node: pre-resolved node
// flags: VFS_O_READ, VFS_O_WRITE, etc.
// returns: vfs_file_t handle, or NULL on error
vfs_file_t* vfs_open_node(vfs_node_t *node, int flags);

// close a file
// returns: VFS_EOK on success, negative error code on failure
int vfs_close(vfs_file_t *file);

// read from file
// returns: number of bytes read, 0 on EOF, negative error code on failure
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t size);

// write to file
// returns: number of bytes written, negative error code on failure
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t size);

// get file size
// returns: file size in bytes, negative error code on failure
// mote: may not be valid for all node types like devices
ssize_t vfs_size(const char *path);
ssize_t vfs_size_node(vfs_node_t *node);

// seek in file
// returns: VFS_EOK on success, error code on failure
int vfs_seek(vfs_file_t *file, size_t offset);

// get current file position
// returns: current position, negative error code on failure
ssize_t vfs_tell(vfs_file_t *file);

// directory operations

// create directory iterator
// path: directory path
// returns: iterator handle, or NULL on error
vfs_dir_iter_t* vfs_dir_iter_create(const char *path);

// create directory iterator from resolved node
// dir_node: pre-resolved directory node
// returns: iterator handle, or NULL on error
vfs_dir_iter_t* vfs_dir_iter_create_node(vfs_node_t *dir_node);

// get next entry in directory
// iter: iterator handle
// returns: >0 if entry available, 0 if end of directory, <0 if error
// entry name is stored in iter->current_name (valid until next call or destroy)
int vfs_dir_iter_next(vfs_dir_iter_t *iter);

// destroy directory iterator
void vfs_dir_iter_destroy(vfs_dir_iter_t *iter);

// create a file or directory in a directory
// dir_path: parent directory path
// name: entry name (not full path)
// type: VFS_NODE_FILE or VFS_NODE_DIR
// returns: pointer to created node, or NULL on error
vfs_node_t* vfs_dir_create(const char *dir_path, const char *name, vfs_node_type_t type);

// create entry in directory from resolved directory node
// dir_node: pre-resolved parent directory node
// name: entry name (not full path)
// type: VFS_NODE_FILE or VFS_NODE_DIR
// returns: pointer to created node, or NULL on error
vfs_node_t* vfs_dir_create_node(vfs_node_t *dir_node, const char *name, vfs_node_type_t type);

// remove a file or directory from a directory
// dir_path: parent directory path
// name: entry name (not full path)
// returns: VFS_EOK on success, negative error code on failure
int vfs_dir_remove(const char *dir_path, const char *name);

// remove entry from directory using resolved directory node
// dir_node: pre-resolved parent directory node
// name: entry name (not full path)
// returns: VFS_EOK on success, error code on failure
int vfs_dir_remove_node(vfs_node_t *dir_node, const char *name);

#ifdef __cplusplus
}
#endif

