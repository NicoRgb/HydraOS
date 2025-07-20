#include <kernel/proc/stream.h>
#include <kernel/kmm.h>
#include <kernel/string.h>
#include <kernel/kprintf.h>
#include <kernel/fs/vfs.h>

stream_t *stream_create_bidirectional(uint8_t flags)
{
    stream_t *stream = kmalloc(sizeof(stream_t));
    if (!stream)
    {
        return NULL;
    }

    memset(stream, 0, sizeof(stream_t));

    stream->type = STREAM_TYPE_BIDIRECTIONAL;
    stream->flags = flags;

    stream->buffer = kmalloc(sizeof(shared_ring_buffer_t));
    if (!stream->buffer)
    {
        return NULL;
    }

    stream->buffer->buffer = pmm_alloc();
    if (!stream->buffer)
    {
        return NULL;
    }
    
    stream->buffer->max_size = PAGE_SIZE;
    stream->buffer->read_offset = 0;
    stream->buffer->write_offset = 0;
    stream->buffer->refcount = 1;
    stream->mount = NULL;

    return stream;
}

stream_t *stream_create_file(struct _file_node *node, mount_node_t *mount)
{
    stream_t *stream = kmalloc(sizeof(stream_t));
    if (!stream)
    {
        return NULL;
    }

    memset(stream, 0, sizeof(stream_t));

    stream->type = STREAM_TYPE_FILE;
    stream->node = node;
    stream->mount = mount;

    return stream;
}

stream_t *stream_create_driver(uint8_t flags, device_t *device, mount_node_t *mount)
{
    stream_t *stream = kmalloc(sizeof(stream_t));
    if (!stream)
    {
        return NULL;
    }

    memset(stream, 0, sizeof(stream_t));

    stream->type = STREAM_TYPE_DRIVER;
    stream->flags = flags;
    stream->mount = mount;

    stream->device = device;

    return stream;
}

void stream_free(stream_t *stream)
{
    switch (stream->type)
    {
    case STREAM_TYPE_BIDIRECTIONAL:
        stream->buffer->refcount -= 1;
        if (stream->buffer->refcount <= 0)
        {
            pmm_free((uint64_t *)stream->buffer->buffer);
            kfree(stream->buffer);
        }
        break;
    case STREAM_TYPE_FILE:
        vfs_close(stream);
        break;
    case STREAM_TYPE_DRIVER:
        break;
    default:
        PANIC("invalid stream type");
        break;
    }
}

int stream_read(stream_t *stream, uint8_t *data, size_t size, size_t *bytes_read)
{
    if (!stream || !data || !bytes_read)
    {
        return -RES_INVARG;
    }

    memset(data, 0, size);

    *bytes_read = 0;
    switch (stream->type)
    {
    case STREAM_TYPE_BIDIRECTIONAL:
        for (size_t i = 0; i < size && stream->buffer->read_offset != stream->buffer->write_offset; i++)
        {
            data[i] = stream->buffer->buffer[stream->buffer->read_offset];
            stream->buffer->read_offset = (stream->buffer->read_offset + 1) % stream->buffer->max_size;
            (*bytes_read)++;
        }
        break;
    case STREAM_TYPE_FILE:
        // TODO: maybe check for filesize and offset
        *bytes_read = size;
        int res = vfs_read(stream, size, data);
        if (res < 0)
        {
            return -RES_EUNKNOWN;
        }

        break;
    case STREAM_TYPE_DRIVER:
        switch (stream->device->type)
        {
        case DEVICE_INPUT:
            inputpacket_t packet;
            size_t i = 0;
            while (device_poll(&packet, stream->device) == 0 && packet.type != IPACKET_NULL && i < size)
            {
                if (packet.type != IPACKET_KEYDOWN && packet.type != IPACKET_KEYREPEAT)
                {
                    continue;
                }
                data[i++] = inputdev_packet_to_ascii(&packet);
                (*bytes_read)++;
            }

            break;
        case DEVICE_BLOCK:
            // TODO: implement
            break;
        case DEVICE_CHAR:
            return -RES_EUNKNOWN;
        default:
            return -RES_INVARG;
        }
        break;
    default:
        return -RES_INVARG;
    }

    return 0;
}

int stream_write(stream_t *stream, const uint8_t *data, size_t size, size_t *bytes_written)
{
    if (!stream || !data || !bytes_written)
    {
        return -RES_INVARG;
    }

    *bytes_written = 0;
    switch (stream->type)
    {
    case STREAM_TYPE_BIDIRECTIONAL:
        for (size_t i = 0; i < size; i++)
        {
            stream->buffer->buffer[stream->buffer->write_offset] = data[i];
            stream->buffer->write_offset = (stream->buffer->write_offset + 1) % stream->buffer->max_size;
            (*bytes_written)++;
        }
        
        // TODO: this is a suboptimal way to do this as it will leave artifacts after an overflow
        if (stream->buffer->write_offset == stream->buffer->read_offset && *bytes_written > 0)
        {
            stream->buffer->buffer[stream->buffer->write_offset] = 0;
            stream->buffer->write_offset = (stream->buffer->write_offset + 1) % stream->buffer->max_size;
        }
        break;
    case STREAM_TYPE_FILE:
        *bytes_written = size;
        int res = vfs_write(stream, size, data);
        if (res < 0)
        {
            return -RES_EUNKNOWN;
        }

        break;
    case STREAM_TYPE_DRIVER:
        switch (stream->device->type)
        {
        case DEVICE_CHAR:
            for (size_t i = 0; i < size; i++)
            {
                int res = device_write((char)data[i], CHARDEV_COLOR_WHITE, CHARDEV_COLOR_BLACK, stream->device);
                if (res < 0)
                {
                    return res;
                }

                (*bytes_written)++;
            }

            break;
        case DEVICE_BLOCK:
            // TODO: implement
            break;

        case DEVICE_INPUT:
            return -RES_EUNKNOWN;
        default:
            return -RES_INVARG;
        }
        break;
    default:
        return -RES_INVARG;
    }

    return 0;
}

int stream_flush(stream_t *stream)
{
    if (stream->type == STREAM_TYPE_BIDIRECTIONAL)
    {
        stream->buffer->read_offset = stream->buffer->write_offset;
    }

    return 0;
}

stream_t *stream_clone(stream_t *src)
{
    if (!src)
    {
        return NULL;
    }

    switch(src->type)
    {
    case STREAM_TYPE_BIDIRECTIONAL:
    {
        stream_t *dest = kmalloc(sizeof(stream_t));
        if (!dest)
        {
            return NULL;
        }

        memset(dest, 0, sizeof(stream_t));

        dest->type = src->type;
        dest->flags = src->flags;
        dest->buffer = src->buffer;
        dest->buffer->refcount += 1;
        dest->buffer->max_size = src->buffer->max_size;
        dest->buffer->read_offset = src->buffer->read_offset;
        dest->buffer->write_offset = src->buffer->write_offset;

        return dest;
    }
    case STREAM_TYPE_FILE:
        return stream_create_file(src->node, src->mount);
    case STREAM_TYPE_DRIVER:
        return stream_create_driver(src->flags, src->device, src->mount);
    default:
        return NULL;
    }

    return NULL;
}
