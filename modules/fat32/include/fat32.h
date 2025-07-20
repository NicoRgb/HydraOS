#ifndef _FAT32_FAT32_H
#define _FAT32_FAT32_H

#include <kernel/fs/vfs.h>

stream_t *fat32_open(const char *path, uint8_t action, mount_node_t *mount);
int fat32_close(stream_t *stream_clone);
int fat32_read(stream_t *stream_clone, size_t size, uint8_t *buf);
int fat32_write(stream_t *stream_clone, size_t size, const uint8_t *buf);
int fat32_readdir(stream_t *stream_clone, int index, char *path);
int fat32_delete(stream_t *stream_clone);

void *fat32_init(virtual_blockdev_t *bdev);
int fat32_free(virtual_blockdev_t *bdev, void *data);
int fat32_test(virtual_blockdev_t *bdev);

#endif