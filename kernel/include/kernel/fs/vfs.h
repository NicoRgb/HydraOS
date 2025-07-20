#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/vec.h>

#include <kernel/status.h>
#include <kernel/fs/vpt.h>

#include <kernel/dev/devm.h>
#include <kernel/proc/stream.h>

#define MAX_PATH 128

#define FS_FILE 0x01
#define FS_DIRECTORY 0x02
#define FS_SYMLINK 0x03

#define MASK_READONLY 1 << 1
#define MASK_HIDDEN 1 << 2
#define MASK_SYSTEM 1 << 3

struct _filesystem;

typedef struct _mount_node
{
    virtual_blockdev_t *vbdev;
    struct _filesystem *fs;
    char path[MAX_PATH];

    void *fs_data;

    struct _mount_node *parent;
    cvector(struct _mount_node *) children;
} mount_node_t;

typedef struct _file_node
{
    char local_path[MAX_PATH];
    mount_node_t *mount;
    struct _filesystem *fs;

    size_t offset;

    size_t filesize;
    uint32_t mask;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t last_access_date;
    uint32_t flags;

    uint64_t _data; // private data for fs implementation
} file_node_t;

typedef struct
{
    char path[MAX_PATH]; // global path
} dirent_t;

typedef struct _filesystem
{
    void *(*fs_init)(virtual_blockdev_t *);
    int (*fs_free)(virtual_blockdev_t *, void *);
    int (*fs_test)(virtual_blockdev_t *);

    stream_t *(*fs_open)(const char *, uint8_t, mount_node_t *);
    int (*fs_close)(stream_t *);
    int (*fs_read)(stream_t *, size_t, uint8_t *);
    int (*fs_write)(stream_t *, size_t, const uint8_t *);
    int (*fs_readdir)(stream_t *, int, char *);
    int (*fs_delete)(stream_t *);
} filesystem_t;

int register_filesystem(filesystem_t *fs);

int vfs_mount_blockdev(virtual_blockdev_t *vbdev, const char *path);
int vfs_mount_filesystem(filesystem_t *fs, const char *path);
//int vfs_unmount_blockdev(const char *path);

#define OPEN_ACTION_READ 0
#define OPEN_ACTION_WRITE 1
#define OPEN_ACTION_CLEAR 2
#define OPEN_ACTION_CREATE 3

#define SEEK_TYPE_SET 0
#define SEEK_TYPE_ADD 1
#define SEEK_TYPE_END 2

stream_t *vfs_open(const char *path, uint8_t action);
int vfs_close(stream_t *stream);
int vfs_read(stream_t *stream, size_t size, uint8_t *buf);
int vfs_write(stream_t *stream, size_t size, const uint8_t *buf);
int vfs_readdir(stream_t *stream, int index, dirent_t *dirent);
int vfs_delete(stream_t *stream); // doesnt close node
int vfs_seek(stream_t *stream, size_t n, uint8_t type);

#endif