#ifndef _MBR_MBR_H
#define _MBR_MBR_H

#include <kernel/fs/vfs.h>

void *mbr_init(device_t *bdev);
int mbr_free(device_t *bdev, void *mbr_data);
int mbr_test(device_t *bdev);
int mbr_get(uint8_t index, void *mbr_data, virtual_blockdev_t *vbdev);

#endif