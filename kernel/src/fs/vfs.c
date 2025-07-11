#include <kernel/fs/vfs.h>
#include <kernel/kmm.h>
#include <kernel/string.h>
#include <kernel/vec.h>

#define FILESYSTEMS_CAPACITY_INCREASE 3

filesystem_t **filesystems = NULL;
size_t filesystems_capacity = 0;
size_t filesystems_size = 0;

int register_filesystem(filesystem_t *fs)
{
    if (!fs)
    {
        return -RES_INVARG;
    }

    if (!filesystems)
    {
        filesystems = kmalloc(sizeof(filesystem_t *) * FILESYSTEMS_CAPACITY_INCREASE);
        if (!filesystems)
        {
            return -RES_NOMEM;
        }

        filesystems_capacity = FILESYSTEMS_CAPACITY_INCREASE;
        filesystems_size = 0;
    }

    if (filesystems_size >= filesystems_capacity)
    {
        filesystems = krealloc(filesystems, filesystems_capacity, filesystems_capacity + sizeof(filesystem_t *) * FILESYSTEMS_CAPACITY_INCREASE);
        filesystems_capacity += FILESYSTEMS_CAPACITY_INCREASE;
    }

    filesystems[filesystems_size++] = fs;
    return 0;
}

typedef struct _mount
{
    virtual_blockdev_t *vbdev;
    filesystem_t *fs;
    int id;

    struct _mount *prev;
    struct _mount *next;
    void *fs_data;
} mount_t;

mount_t *mounts_head = NULL;

static int allocate_mount_id(void)
{
    for (int i = 0; i < INT32_MAX; i++)
    {
        bool found = false;
        for (mount_t *mnt = mounts_head; mnt != NULL; mnt = mnt->next)
        {
            if (mnt->id == i)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return i;
        }
    }

    return -RES_EUNKNOWN;
}

int vfs_mount_blockdev(virtual_blockdev_t *vbdev)
{
    if (!vbdev)
    {
        return -RES_EUNKNOWN;
    }

    filesystem_t *fs = NULL;
    for (size_t i = 0; i < filesystems_size; i++)
    {
        if (filesystems[i]->fs_test(vbdev) == 0)
        {
            fs = filesystems[i];
            break;
        }
    }

    if (!fs)
    {
        return -RES_EUNKNOWN;
    }

    if (!mounts_head)
    {
        mounts_head = kmalloc(sizeof(mount_t));
        if (!mounts_head)
        {
            return -RES_NOMEM;
        }

        mounts_head->vbdev = vbdev;
        mounts_head->fs = fs;
        mounts_head->fs_data = fs->fs_init(vbdev);
        mounts_head->id = 0;
        mounts_head->next = NULL;
        mounts_head->prev = NULL;
        return mounts_head->id;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt->next != NULL; mnt = mnt->next)
        ;

    if (!mnt)
    {
        return -RES_EUNKNOWN;
    }

    mount_t *new_mount = kmalloc(sizeof(mount_t));
    if (!new_mount)
    {
        return -RES_NOMEM;
    }

    new_mount->vbdev = vbdev;
    new_mount->fs = fs;
    new_mount->fs_data = fs->fs_init(vbdev);
    int status = new_mount->id = allocate_mount_id();
    if (status < 0)
    {
        return status;
    }

    new_mount->next = NULL;
    new_mount->prev = mnt;
    mnt->next = new_mount;

    return new_mount->id;
}

int vfs_unmount_blockdev(int id)
{
    if (!mounts_head)
    {
        return -RES_CORRUPT;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != id)
        {
            continue;
        }

        mnt->prev->next = mnt->next;
        mnt->next->prev = mnt->prev;

        mnt->fs->fs_free(mnt->vbdev, mnt->fs_data);

        kfree(mnt);
        return 0;
    }

    return -RES_EUNKNOWN;
}

int extract_disk_id(const char *str)
{
    if (!str)
    {
        return -RES_INVARG;
    }

    char *end;
    long id = strtol(str, &end, 10);

    if (end == str || *end != ':')
    {
        return -RES_INVARG;
    }

    if (id < 0 || id > INT32_MAX)
    {
        return -RES_INVARG;
    }

    return (int)id;
}

const char *extract_path(const char *str)
{
    if (!str)
    {
        return NULL;
    }

    const char *colon_pos = strchr(str, ':');
    if (!colon_pos || *(colon_pos + 1) != '/')
    {
        return NULL;
    }

    return (const char *)((uintptr_t)colon_pos + 1);
}

file_node_t *vfs_open(const char *path, uint8_t action)
{
    if (!path)
    {
        return NULL;
    }

    int id = extract_disk_id(path);
    if (id < 0)
    {
        return NULL;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != id)
        {
            continue;
        }

        const char *local_path = extract_path(path);
        if (!local_path)
        {
            return NULL;
        }

        file_node_t *node = mnt->fs->fs_open(local_path, action, mnt->vbdev, mnt->fs_data);
        if (!node)
        {
            return NULL;
        }

        strncpy(node->local_path, local_path, MAX_PATH);
        node->mount_id = id;
        node->fs = mnt->fs;
        node->offset = 0;
        return node;
    }

    return NULL;
}

int vfs_close(file_node_t *node)
{
    if (!node)
    {
        return -RES_INVARG;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != node->mount_id)
        {
            continue;
        }

        return mnt->fs->fs_close(node, mnt->vbdev, mnt->fs_data);
    }

    return -RES_INVARG;
}

int vfs_read(file_node_t *node, size_t size, uint8_t *buf)
{
    if (!node)
    {
        return -RES_INVARG;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != node->mount_id)
        {
            continue;
        }

        return mnt->fs->fs_read(node, size, buf, mnt->vbdev, mnt->fs_data);
    }

    return -RES_CORRUPT;
}

int vfs_write(file_node_t *node, size_t size, const uint8_t *buf)
{
    if (!node)
    {
        return -RES_INVARG;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != node->mount_id)
        {
            continue;
        }

        return mnt->fs->fs_write(node, size, buf, mnt->vbdev, mnt->fs_data);
    }

    return -RES_CORRUPT;
}

static void int_to_string(int num, char *str)
{
    if (num == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int is_negative = 0;
    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }

    int index = 0;
    while (num > 0)
    {
        int digit = num % 10;
        str[index++] = '0' + digit;
        num /= 10;
    }

    if (is_negative)
    {
        str[index++] = '-';
    }

    int i = 0, j = index - 1;
    while (i < j)
    {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }

    str[index] = '\0';
}

int vfs_readdir(file_node_t *node, int index, dirent_t *dirent)
{
    if (!node || !dirent)
    {
        return -RES_INVARG;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != node->mount_id)
        {
            continue;
        }

        char int_str[12];
        int_to_string(node->mount_id, int_str);
        size_t int_str_len = strlen(int_str);
        strcpy(dirent->path, int_str);
        dirent->path[int_str_len] = ':';

        int status = mnt->fs->fs_readdir(node, index, (char *)((uintptr_t)dirent->path + int_str_len + 1), mnt->vbdev, mnt->fs_data);
        if (status < 0)
        {
            return status;
        }

        return 0;
    }

    return -RES_EUNKNOWN;
}

int vfs_delete(file_node_t *node)
{
    if (!node)
    {
        return -RES_INVARG;
    }

    mount_t *mnt = NULL;
    for (mnt = mounts_head; mnt != NULL; mnt = mnt->next)
    {
        if (mnt->id != node->mount_id)
        {
            continue;
        }

        return mnt->fs->fs_delete(node, mnt->vbdev, mnt->fs_data);
    }

    return -RES_CORRUPT;
}

int vfs_seek(file_node_t *node, size_t n, uint8_t type)
{
    if (!node)
    {
        return -RES_INVARG;
    }

    if (type == SEEK_TYPE_SET)
    {
        node->offset = n;
    }
    else if (type == SEEK_TYPE_ADD)
    {
        node->offset += n;
    }
    else if (type == SEEK_TYPE_END)
    {
        node->offset = node->filesize - n;
    }
    else
    {
        return -RES_INVARG;
    }

    return 0;
}

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
    if (!res) return NULL;

    memcpy(res, type_str, type_len);
    res[type_len] = '0' + index;
    res[type_len + 1] = '\0';

    return res;
}

int vfs_mount_device(device_t *dev)
{
    if (mounted_devices == CVECTOR)
    {
        cvector_init(mounted_devices);
    }

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

stream_t *vfs_open_file(const char *path, uint8_t action, stream_t *stream)
{
    if (!path)
    {
        return NULL;
    }

    int id = extract_disk_id(path);
    if (id < 0)
    {
        return NULL;
    }

    if (id == 9) // mounted device
    {
        for (size_t i = 0; i < cvector_size(mounted_devices); i++)
        {
            const char *p = (const char *)((uintptr_t)extract_path(path) + 1);

            if (strncmp(mounted_devices[i].name, p, strlen(device_type_names[mounted_devices[i].dev->type]) + 1) == 0)
            {
                memset(stream, 0, sizeof(stream_t));
                if (stream_create_driver(stream, 0, mounted_devices[i].dev) < 0)
                {
                    return NULL;
                }

                return stream;
            }
        }

        return NULL;
    }

    memset(stream, 0, sizeof(stream_t));

    if (stream_create_file(stream, 0, path, action))
    {
        return NULL;
    }

    return stream;
}

int vfs_close_file(stream_t *stream)
{
    stream_free(stream);
    return RES_SUCCESS;
}
