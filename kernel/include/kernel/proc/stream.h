#ifndef _KERNEL_STREAM_H
#define _KERNEL_STREAM_H

#include <kernel/dev/devm.h>
#include <stdint.h>

typedef enum
{
    STREAM_TYPE_NULL = 0,
    STREAM_TYPE_BIDIRECTIONAL = 1,
    STREAM_TYPE_FILE = 2,
    STREAM_TYPE_DRIVER = 3
} stream_type_t;

struct _file_node;
struct _mount_node;

typedef struct
{
    uint8_t *buffer;
    uint32_t refcount;
    size_t write_offset;
    size_t read_offset;
    size_t max_size;
} shared_ring_buffer_t;

typedef struct
{
    stream_type_t type;
    uint8_t flags; // not yet used
    struct _mount_node *mount;

    union
    {
        struct
        {
            shared_ring_buffer_t *buffer;
        };

        struct
        {
            struct _file_node *node;
        };

        struct
        {
            device_t *device;
        };
    };
} stream_t;

stream_t *stream_create_bidirectional(uint8_t flags);
stream_t *stream_create_file(struct _file_node *node, struct _mount_node *mount);
stream_t *stream_create_driver(uint8_t flags, device_t *device, struct _mount_node *mount);

void stream_free(stream_t *stream);

int stream_read(stream_t *stream, uint8_t *data, size_t size, size_t *bytes_read);
int stream_write(stream_t *stream, const uint8_t *data, size_t size, size_t *bytes_written);
int stream_flush(stream_t *stream);
stream_t *stream_clone(stream_t *src);

#endif