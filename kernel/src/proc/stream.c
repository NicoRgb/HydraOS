#include <kernel/proc/stream.h>
#include <kernel/kmm.h>
#include <kernel/string.h>
#include <kernel/kprintf.h>
#include <kernel/fs/vfs.h>

int stream_create_bidirectional(stream_t *stream, uint8_t flags)
{
    stream->type = STREAM_TYPE_BIDIRECTIONAL;
    stream->flags = flags;

    stream->buffer = kmalloc(sizeof(shared_ring_buffer_t));
    if (!stream->buffer)
    {
        return -RES_NOMEM;
    }

    stream->buffer->buffer = pmm_alloc();
    if (!stream->buffer)
    {
        return -RES_NOMEM;
    }
    
    stream->buffer->max_size = PAGE_SIZE;
    stream->buffer->read_offset = 0;
    stream->buffer->write_offset = 0;
    stream->buffer->refcount = 1;

    return 0;
}

int stream_create_file(stream_t *stream, uint8_t flags, const char *path, uint8_t open_action)
{
    stream->type = STREAM_TYPE_FILE;
    stream->flags = flags;
    stream->path = strdup(path);
    stream->open_action = open_action;

    stream->node = vfs_open(path, open_action);
    if (!stream->node)
    {
        return -RES_EUNKNOWN;
    }

    return 0;
}

int stream_create_driver(stream_t *stream, uint8_t flags, device_t *device)
{
    stream->type = STREAM_TYPE_DRIVER;
    stream->flags = flags;

    stream->device = device;

    return 0;
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
        vfs_close(stream->node);
        kfree(stream->path);
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
        int res = vfs_read(stream->node, size, data);
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
        int res = vfs_write(stream->node, size, data);
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

int stream_clone(stream_t *src, stream_t *dest)
{
    if (!src || !dest)
    {
        return -RES_INVARG;
    }

    switch(src->type)
    {
    case STREAM_TYPE_BIDIRECTIONAL:
        dest->type = src->type;
        dest->flags = src->flags;
        dest->buffer = src->buffer;
        dest->buffer->refcount += 1;
        dest->buffer->max_size = src->buffer->max_size;
        dest->buffer->read_offset = src->buffer->read_offset;
        dest->buffer->write_offset = src->buffer->write_offset;
        break;
    case STREAM_TYPE_FILE:
        stream_create_file(dest, src->flags, src->path, src->open_action);
        break;
    case STREAM_TYPE_DRIVER:
        stream_create_driver(dest, src->flags, src->device);
        break;
    default:
        return -RES_INVARG;
    }

    return 0;
}
