#ifndef _FAT32_FAT32_H
#define _FAT32_FAT32_H

#include <kernel/fs/vfs.h>

file_node_t *fat32_open(const char *path, uint8_t action, virtual_blockdev_t *bdev, void *data);
int fat32_close(file_node_t *node, virtual_blockdev_t *bdev, void *data);
int fat32_read(file_node_t *node, size_t size, uint8_t *buf, virtual_blockdev_t *bdev, void *data);
int fat32_write(file_node_t *node, size_t size, const uint8_t *buf, virtual_blockdev_t *bdev, void *data);
int fat32_readdir(file_node_t *node, int index, char *path, virtual_blockdev_t *bdev, void *data);
int fat32_delete(file_node_t *node, virtual_blockdev_t *bdev, void *data);
void *fat32_init(virtual_blockdev_t *bdev);
int fat32_free(virtual_blockdev_t *bdev, void *data);
int fat32_test(virtual_blockdev_t *bdev);

#endif