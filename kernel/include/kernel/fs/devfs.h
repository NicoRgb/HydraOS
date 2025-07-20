#ifndef _KERNEL_DEVFS_H
#define _KERNEL_DEVFS_H

#include <kernel/fs/vfs.h>

int devfs_mount_device(device_t *dev);

stream_t *devfs_open(const char *path, uint8_t action, mount_node_t *mount);
int devfs_close(stream_t *stream_clone);
int devfs_read(stream_t *stream_clone, size_t size, uint8_t *buf);
int devfs_write(stream_t *stream_clone, size_t size, const uint8_t *buf);
int devfs_readdir(stream_t *stream_clone, int index, char *path);
int devfs_delete(stream_t *stream_clone);
void *devfs_init(virtual_blockdev_t *bdev);
int devfs_free(virtual_blockdev_t *bdev, void *data);

extern filesystem_t devfs;

#endif