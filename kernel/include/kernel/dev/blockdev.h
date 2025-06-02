#ifndef _KERNEL_BLOCKDEV_H
#define _KERNEL_BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/status.h>

#define BLOCKDEV_MODEL_MAX_LEN 41
#define BLOCKDEV_TYPE_HARD_DRIVE 0
#define BLOCKDEV_TYPE_REMOVABLE 1

typedef struct _blockdev
{
    int (*free)(struct _blockdev *);
    int (*read_block)(uint64_t, uint8_t *, struct _blockdev *);
    int (*write_block)(uint64_t, const uint8_t *, struct _blockdev *);
    int (*eject)(struct _blockdev *);

    uint32_t block_size;
    size_t num_blocks;
    uint8_t type;
    char model[BLOCKDEV_MODEL_MAX_LEN];
    bool available;

    uint64_t _data; // private data for driver
    uint32_t references;
} blockdev_t;

blockdev_t *blockdev_new_ref(blockdev_t *bdev);
KRES blockdev_free_ref(blockdev_t *bdev);

KRES blockdev_read_block(uint64_t lba, uint8_t *data, blockdev_t *bdev);
KRES blockdev_write_block(uint64_t lba, const uint8_t *data, blockdev_t *bdev);
KRES blockdev_eject(blockdev_t *bdev);

#endif