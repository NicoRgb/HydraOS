#include <kernel/fs/vfs.h>
#include <kernel/kmm.h>
#include <kernel/string.h>

static bool is_path_root(const char *path)
{
    if (!path)
        return false;
    while (*path == '/')
        path++;

    return *path == '\0';
}

char *path_join(const char *path1, const char *path2)
{
    if (!path1 && !path2)
        return NULL;
    if (!path1)
        return strdup(path2);
    if (!path2)
        return strdup(path1);

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    size_t needs_slash = (len1 > 0 && path1[len1 - 1] != '/' && path2[0] != '/');
    size_t total_len = len1 + len2 + (needs_slash ? 1 : 0) + 1;

    char *result = kmalloc(total_len);
    if (!result)
        return NULL;

    strcpy(result, path1);

    if (needs_slash)
    {
        strcat(result, "/");
    }

    if (path1[len1 - 1] == '/' && path2[0] == '/')
    {
        strcat(result, path2 + 1);
    }
    else
    {
        strcat(result, path2);
    }

    return result;
}

int split_path_component(const char *path, char **head, const char **tail)
{
    if (!path || !head || !tail)
        return -1;

    const char *start = path;

    const char *p = path;
    while (*p == '/')
        p++;
    if (*p == '\0')
    {
        *head = NULL;
        *tail = NULL;
        return -1;
    }

    const char *slash = strchr(p, '/');

    size_t len = slash ? (size_t)(slash - start) : strlen(start);
    *head = strndup(start, len);
    if (!*head)
        return -1;

    *tail = slash ? slash : (start + len);
    while (**tail == '/')
        (*tail)++;

    return 0;
}

const char *path_basename(const char *path)
{
    if (!path || !*path)
    {
        return path;
    }

    const char *end = path + strlen(path);
    while (end > path && *(end - 1) == '/')
    {
        --end;
    }

    const char *start = end;
    while (start > path && *(start - 1) != '/')
    {
        --start;
    }

    return start;
}

char *path_dirname(const char *path)
{
    if (!path || !*path)
    {
        return strdup(".");
    }

    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
    {
        len--;
    }

    size_t i = len;
    while (i > 0 && path[i - 1] != '/')
    {
        i--;
    }

    if (i == 0)
    {
        return strdup("/");
    }

    while (i > 1 && path[i - 1] == '/')
    {
        i--;
    }

    char *dir = kmalloc(i + 1);
    if (!dir)
    {
        return NULL;
    }

    memcpy(dir, path, i);
    dir[i] = '\0';
    return dir;
}

static bool is_full_path(const char *str)
{
    return str[0] == '/';
}

cvector(filesystem_t *) filesystems = CVECTOR;

int register_filesystem(filesystem_t *fs)
{
    if (!fs)
    {
        return -RES_INVARG;
    }

    cvector_push(filesystems, fs);
    return 0;
}

mount_node_t *root = NULL;

static int vfs_mount(mount_node_t *_mount, const char *path)
{
    if (!root && !is_path_root(path))
    {
        return -RES_EUNKNOWN;
    }

    if (!is_full_path(path))
    {
        return -RES_INVARG;
    }

    if (!root && is_path_root(path))
    {
        strcpy(_mount->path, "/");
        _mount->parent = NULL;
        cvector_init(_mount->children);

        root = _mount;

        return RES_SUCCESS;
    }

    char *path_copy = path_dirname(path);
    if (!path_copy)
    {
        return -RES_NOMEM;
    }

    char *tok = strtok(path_copy, "/");

    mount_node_t *mount = root;
    while (tok)
    {
        bool found = false;
        for (size_t i = 0; i < cvector_size(mount->children); i++)
        {
            if (strcmp(mount->children[i]->path, tok) == 0)
            {
                found = true;
                tok = strtok(NULL, "/");
                mount = mount->children[i];
                break;
            }
        }
        if (!found)
        {
            // TODO: validate if the directory actually exists
            break;
        }
    }

    if (tok == NULL)
    {
        _mount->parent = mount;
        strcpy(_mount->path, path_basename(path));
        cvector_init(_mount->children);

        cvector_push(mount->children, _mount);
    }
    else if (tok != NULL && mount->vbdev == NULL && mount->fs == NULL)
    {
        // TODO: go back to "real" mount and create nodes for path and the child
        kfree(path_copy);
        return -RES_EUNKNOWN;
    }
    else if (tok != NULL && mount->vbdev != NULL && mount->fs == NULL)
    {
        while (tok)
        {
            mount_node_t *node = kmalloc(sizeof(mount_node_t));
            if (!node)
            {
                return -RES_NOMEM;
            }

            memset(node, 0, sizeof(mount_node_t));
            strcpy(node->path, tok);
            cvector_init(node->children);

            cvector_push(mount->children, node);

            tok = strtok(NULL, "/");
            mount = node;
        }

        strcpy(_mount->path, path_basename(path));
        _mount->parent = mount;
        cvector_init(_mount->children);

        cvector_push(mount->children, _mount);
    }
    else
    {
        kfree(path_copy);
        return -RES_EUNKNOWN;
    }

    kfree(path_copy);
    return RES_SUCCESS;
}

int vfs_mount_blockdev(virtual_blockdev_t *vbdev, const char *path)
{
    if (!root && !is_path_root(path))
    {
        return -RES_EUNKNOWN;
    }

    if (!is_full_path(path))
    {
        return -RES_INVARG;
    }

    if (filesystems == CVECTOR)
    {
        cvector_init(filesystems);
    }

    filesystem_t *fs = NULL;
    for (size_t i = 0; i < cvector_size(filesystems); i++)
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

    mount_node_t *mount = kmalloc(sizeof(mount_node_t));
    if (!mount)
    {
        return -RES_NOMEM;
    }

    memset(mount, 0, sizeof(mount_node_t));

    mount->vbdev = vbdev;
    mount->fs = fs;
    mount->fs_data = fs->fs_init(vbdev);

    return vfs_mount(mount, path);
}

int vfs_mount_filesystem(filesystem_t *fs, const char *path)
{
    if (!root && !is_path_root(path))
    {
        return -RES_EUNKNOWN;
    }

    if (!is_full_path(path))
    {
        return -RES_INVARG;
    }

    mount_node_t *mount = kmalloc(sizeof(mount_node_t));
    if (!mount)
    {
        return -RES_NOMEM;
    }

    memset(mount, 0, sizeof(mount_node_t));

    mount->fs = fs;
    mount->fs_data = fs->fs_init(NULL);

    return vfs_mount(mount, path);
}

typedef struct
{
    mount_node_t *mount;
    char local_path[MAX_PATH]; // path within that mount
} vfs_mount_resolve_t;

vfs_mount_resolve_t vfs_resolve_real_mount_and_local_path(const char *path)
{
    vfs_mount_resolve_t result = {.mount = NULL};
    if (!path || !is_full_path(path) || !root)
    {
        return result;
    }

    mount_node_t *current = root;
    mount_node_t *last_real = root;

    size_t consumed_length = 0;
    size_t real_consumed_length = 0;

    char *path_copy = strdup(path);
    if (!path_copy)
    {
        return result;
    }

    char *tok = strtok(path_copy, "/");
    while (tok)
    {
        if (strcmp(tok, ".") == 0)
        {
            tok = strtok(NULL, "/");
            consumed_length += 3;
            continue;
        }
        else if (strcmp(tok, "..") == 0)
        {
            tok = strtok(NULL, "/");
            consumed_length += 4;
            current = current->parent;
            if (!current)
            {
                result.mount = NULL;
                return result;
            }
            continue;
        }

        bool found = false;
        for (size_t i = 0; i < cvector_size(current->children); i++)
        {
            if (strcmp(current->children[i]->path, tok) == 0)
            {
                // TODO: this is a bug as a path can cosume multiple slashes
                consumed_length += strlen(tok) + 1; // +1 for '/'
                current = current->children[i];
                if (current->vbdev || current->fs)
                {
                    last_real = current;
                    real_consumed_length = consumed_length;
                }
                found = true;
                break;
            }
        }

        if (!found)
        {
            break;
        }

        tok = strtok(NULL, "/");
    }

    kfree(path_copy);

    result.mount = last_real;

    if (strlen(path) > real_consumed_length)
    {
        strncpy(result.local_path, path + real_consumed_length, MAX_PATH - 1);
    }
    else
    {
        strcpy(result.local_path, "");
    }

    return result;
}

char *vfs_get_mount_path(mount_node_t *mount)
{
    if (!mount)
    {
        return NULL;
    }

    const char *segments[MAX_PATH];
    size_t segment_count = 0;

    mount_node_t *current = mount;
    while (current)
    {
        segments[segment_count++] = current->path;
        current = current->parent;
    }

    size_t total_length = 1;
    for (size_t i = segment_count; i-- > 0;)
    {
        if (segments[i][0] != '\0')
        {
            total_length += strlen(segments[i]);
        }
    }

    char *full_path = kmalloc(total_length + 1);
    if (!full_path)
    {
        return NULL;
    }

    full_path[0] = '\0';

    for (size_t i = segment_count; i-- > 0;)
    {
        if (segments[i][0] != '\0')
        {
            strcat(full_path, segments[i]);
            if (i != 0)
                strcat(full_path, "/");
        }
    }

    return full_path;
}

stream_t *vfs_open(const char *path, uint8_t action)
{
    if (!path)
    {
        return NULL;
    }

    vfs_mount_resolve_t mount_resolve = vfs_resolve_real_mount_and_local_path(path);
    if (!mount_resolve.mount)
    {
        return NULL;
    }

    stream_t *stream = mount_resolve.mount->fs->fs_open(mount_resolve.local_path, action, mount_resolve.mount);
    if (!stream)
    {
        return NULL;
    }

    return stream;
}

int vfs_close(stream_t *stream)
{
    if (!stream || !stream->mount)
    {
        return -RES_INVARG;
    }

    return stream->mount->fs->fs_close(stream);
}

int vfs_read(stream_t *stream, size_t size, uint8_t *buf)
{
    if (!stream || !stream->mount)
    {
        return -RES_INVARG;
    }

    return stream->mount->fs->fs_read(stream, size, buf);
}

int vfs_write(stream_t *stream, size_t size, const uint8_t *buf)
{
    if (!stream || !stream->mount)
    {
        return -RES_INVARG;
    }

    return stream->mount->fs->fs_write(stream, size, buf);
}

int vfs_readdir(stream_t *stream, int index, dirent_t *dirent)
{
    if (!stream || !stream->mount || !dirent)
    {
        return -RES_INVARG;
    }

    char *mount_path = vfs_get_mount_path(stream->mount);
    if (!mount_path)
    {
        return -RES_NOMEM;
    }
    strcpy(dirent->path, mount_path);
    kfree(mount_path);

    int status = stream->mount->fs->fs_readdir(stream, index, (char *)((uintptr_t)dirent->path + strlen(dirent->path)));
    if (status < 0)
    {
        return status;
    }

    return RES_SUCCESS;
}

int vfs_delete(stream_t *stream)
{
    if (!stream || !stream->mount)
    {
        return -RES_INVARG;
    }

    return stream->mount->fs->fs_delete(stream);
}

int vfs_seek(stream_t *stream, size_t n, uint8_t type)
{
    if (!stream || stream->type != STREAM_TYPE_FILE)
    {
        return -RES_INVARG;
    }

    if (type == SEEK_TYPE_SET)
    {
        stream->node->offset = n;
    }
    else if (type == SEEK_TYPE_ADD)
    {
        stream->node->offset += n;
    }
    else if (type == SEEK_TYPE_END)
    {
        stream->node->offset = stream->node->filesize - n;
    }
    else
    {
        return -RES_INVARG;
    }

    return 0;
}

/*typedef struct
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
*/
