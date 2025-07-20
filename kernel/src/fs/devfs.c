#include <kernel/fs/devfs.h>
#include <kernel/string.h>
#include <kernel/log.h>

typedef struct
{
    device_t *dev;
    const char *name;
} mounted_device_t;

cvector(mounted_device_t) mounted_devices = CVECTOR;

const char *const device_type_names[] = {
    "block",
    "char",
    "input",
    "video",
    "rng",
    "net",
};

static const char *generate_device_mount_name(device_t *dev)
{
    uint8_t index = 0;
    for (size_t i = 0; i < cvector_size(mounted_devices); i++)
    {
        if (mounted_devices[i].dev->type == dev->type)
        {
            index++;
        }
    }

    if (index > 9)
    {
        return NULL;
    }

    const char *type_str = "dev";
    switch (dev->type)
    {
    case DEVICE_BLOCK:
        type_str = "block";
        break;
    case DEVICE_CHAR:
        type_str = "char";
        break;
    case DEVICE_INPUT:
        type_str = "input";
        break;
    case DEVICE_VIDEO:
        type_str = "video";
        break;
    case DEVICE_RNG:
        type_str = "rng";
        break;
    case DEVICE_NET:
        type_str = "net";
        break;
    }

    size_t type_len = strlen(type_str);

    char *res = kmalloc(type_len + 2);
    if (!res)
        return NULL;

    memcpy(res, type_str, type_len);
    res[type_len] = '0' + index;
    res[type_len + 1] = '\0';

    return res;
}

int devfs_mount_device(device_t *dev)
{
    const char *mount_name = generate_device_mount_name(dev);
    if (!mount_name)
    {
        return -RES_EUNKNOWN;
    }

    mounted_device_t mount_dev = {
        .dev = dev,
        .name = mount_name,
    };
    cvector_push(mounted_devices, mount_dev);

    return RES_SUCCESS;
}

stream_t *devfs_open(const char *path, uint8_t action, mount_node_t *mount)
{
    for (size_t i = 0; i < cvector_size(mounted_devices); i++)
    {
        // TODO: again, multiple slashes
        if (strcmp((const char *)((uintptr_t)path + 1), mounted_devices[i].name) == 0)
        {
            return stream_create_driver(0, mounted_devices[i].dev, mount);
        }
    }

    return NULL;
}

int devfs_close(stream_t *stream_clone)
{
}

int devfs_readdir(stream_t *stream_clone, int index, char *path)
{
}

int devfs_delete(stream_t *stream_clone)
{
}

void *devfs_init(virtual_blockdev_t *bdev)
{
    cvector_init(mounted_devices);

    return NULL;
}

int devfs_free(virtual_blockdev_t *bdev, void *data)
{
    return 0;
}

filesystem_t devfs = {
    .fs_init = &devfs_init,
    .fs_free = &devfs_free,
    .fs_test = NULL,
    .fs_open = &devfs_open,
    .fs_close = &devfs_close,
    .fs_read = NULL,
    .fs_write = NULL,
    .fs_readdir = &devfs_readdir,
    .fs_delete = &devfs_delete
};
