#include <fat32.h>
#include <kernel/kmm.h>
#include <kernel/string.h>

/*
 known bugs / unsupported features:
 - no deleting directories
 - clusters are not deleted from directories when their are no needed anymore
 - long filenames can't go over multiple clusters
 - files and directories can't be created with long filenames
*/

static void read_fat_device(virtual_blockdev_t *device, size_t lba, size_t size, uint8_t *buf)
{
    size_t current_lba = lba + device->lba_offset;
    size_t remaining_sectors = size;

    while (remaining_sectors > 0)
    {
        if ((current_lba + 1) > device->bdev->num_blocks)
        {
            while (1);
        }

        if (device_read_block(current_lba, buf, device->bdev) < 0)
        {
            while (1);
        }

        current_lba++;
        buf += device->bdev->block_size;
        remaining_sectors--;
    }
}

static void write_fat_device(virtual_blockdev_t *device, size_t lba, size_t size, const uint8_t *buf)
{
    size_t current_lba = lba + device->lba_offset;
    size_t remaining_sectors = size;

    while (remaining_sectors > 0)
    {
        if ((current_lba + 1) > device->bdev->num_blocks)
        {
            while (1);
        }

        if (device_write_block(current_lba, buf, device->bdev) < 0)
        {
            while (1);
        }

        current_lba++;
        buf += device->bdev->block_size;
        remaining_sectors--;
    }
}

uint16_t get_fat32_date()
{
    return 0;
}

uint16_t get_fat32_time()
{
    return 0;
}

uint8_t get_fat32_time_tenth()
{
    return 0;
}

char utf16_to_ascii(const uint16_t c)
{
    if (c <= 0x7F)
    {
        return (char)(c & 0x7F);
    }

    return '?';
}

#define BACKUP_SECTOR_NUM 6
#define ROUND_UP_INT_DIV(x, y) (x + y - 1) / y

#define MAX_FILENAME_LENGTH 256

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_INVALID (0x80 | 0x40 | 0x08)

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04

typedef struct
{
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_FATs;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t FAT_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint32_t FAT_size_32;
    uint16_t ext_flags;
    uint16_t filesystem_version;
    uint32_t root_cluster;
    uint16_t filesystem_info;
    uint16_t backup_boot_sector;
    uint8_t zero1[12];
    uint8_t drive_num;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_ID;
    char volume_label[11];
    char filesystem_type[8];
    uint8_t zero2[420];
    uint16_t signature;
} __attribute__((packed)) bios_parameter_block_t;

typedef struct
{
    uint8_t jmp_boot[3];
    char OEM_name[8];
    bios_parameter_block_t bpb;
} __attribute__((packed)) boot_sector_t;

typedef struct
{
    union
    {
        struct
        {

            char name[8];
            char ext[3];
        };
        char nameext[11];
    };

    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) directory_entry_t;

typedef struct
{
    uint8_t ord;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t first_cluster_low;
    uint16_t name3[2];
} __attribute__((packed)) lfn_directory_entry_t;

int verify_boot_sector(boot_sector_t *boot_sector, uint32_t bytes_per_sector)
{
    if (!(boot_sector->jmp_boot[0] == 0xEB && boot_sector->jmp_boot[2] == 0x90) && boot_sector->jmp_boot[0] != 0xE9)
    {
        return -RES_EUNKNOWN;
    }

    if (boot_sector->bpb.bytes_per_sector != bytes_per_sector)
    {
        if (boot_sector->bpb.bytes_per_sector == 512 || boot_sector->bpb.bytes_per_sector == 1024 || boot_sector->bpb.bytes_per_sector == 2048 || boot_sector->bpb.bytes_per_sector == 4096)
        {
            return -2;
        }
        else
        {
            return -3;
        }
    }

    if (boot_sector->bpb.sectors_per_cluster == 0 || (boot_sector->bpb.sectors_per_cluster & (boot_sector->bpb.sectors_per_cluster - 1)) != 0)
    {
        return -4;
    }

    if (boot_sector->bpb.reserved_sector_count == 0)
    {
        return -5;
    }

    if (boot_sector->bpb.root_entry_count != 0)
    {
        return -6;
    }

    if (boot_sector->bpb.total_sectors_16 == 0 && boot_sector->bpb.total_sectors_32 == 0)
    {
        return -7;
    }

    if (boot_sector->bpb.media != 0xF0 && boot_sector->bpb.media < 0xF8)
    {
        return -8;
    }

    if (boot_sector->bpb.FAT_size_16 != 0)
    {
        return -9;
    }

    if (boot_sector->bpb.total_sectors_32 == 0 && boot_sector->bpb.total_sectors_16 == 0)
    {
        return -11;
    }

    if (boot_sector->bpb.filesystem_version != 0)
    {
        return -12;
    }

    if (boot_sector->bpb.root_cluster < 2)
    {
        return -13;
    }

    if (boot_sector->bpb.backup_boot_sector != 0 && boot_sector->bpb.backup_boot_sector != 6)
    {
        return -14;
    }

    if (boot_sector->bpb.reserved != 0)
    {
        return -15;
    }

    if (strncmp(boot_sector->bpb.filesystem_type, "FAT32", 5))
    {
        return -16;
    }

    if (boot_sector->bpb.signature != 0xAA55)
    {
        return -17;
    }

    return 0;
}

boot_sector_t *scan_fat(uint32_t bytes_per_sector, virtual_blockdev_t *dev)
{
    boot_sector_t *boot_sector = kmalloc(bytes_per_sector);
    read_fat_device(dev, 0, 1, (uint8_t *)boot_sector);
    if (verify_boot_sector(boot_sector, bytes_per_sector) < 0)
    {
        return NULL;
    }

    return boot_sector;
}

void free_fat(boot_sector_t *boot_sector)
{
    kfree(boot_sector);
}

static char *fat32_nameext_to_name(const char *nameext, char *filename)
{
    if (nameext[0] == 0x20)
    {
        return NULL;
    }

    size_t file_name_cnt = 0;
    bool before_extension = true;
    bool in_spaces = false;
    bool in_extension = false;

    for (int i = 0; i < 11; i++)
    {
        if (before_extension)
        {
            if (nameext[i] == 0x20)
            {
                before_extension = false;
                in_spaces = true;
                filename[file_name_cnt++] = '.';
            }
            else if (i == 8)
            {
                before_extension = false;
                in_spaces = true;
                filename[file_name_cnt++] = '.';
                filename[file_name_cnt++] = nameext[i];
                in_extension = true;
            }
            else
            {
                filename[file_name_cnt++] = nameext[i];
            }
        }
        else if (in_spaces)
        {
            if (nameext[i] != 0x20)
            {
                in_spaces = false;
                in_extension = true;
                filename[file_name_cnt++] = nameext[i];
            }
        }
        else if (in_extension)
        {
            if (nameext[i] == 0x20)
            {
                break;
            }
            else
            {
                filename[file_name_cnt++] = nameext[i];
            }
        }
    }

    if (strncmp(filename, ".          ", 11) == 0)
    {
        filename[0] = '.';
        filename[1] = '\0';
    }
    else if (strncmp(filename, "..         ", 11) == 0)
    {
        filename[0] = '.';
        filename[1] = '.';
        filename[2] = '\0';
    }

    if (filename[file_name_cnt - 1] == '.')
    {
        file_name_cnt--;
    }
    filename[file_name_cnt] = '\0';

    for (size_t i = 0; i < file_name_cnt; i++)
    {
        if (!isalpha(filename[i]))
        {
            continue;
        }

        filename[i] = tolower(filename[i]);
    }

    return filename;
}

static int is_valid_fat32_filename(const char *filename)
{
    size_t len = strlen(filename);
    if (len == 0 || len > 12)
    {
        // Filename must be non-empty and less than or equal to 12 characters (8.3 format)
        return -RES_EUNKNOWN;
    }

    bool dot_found = false;
    for (size_t i = 0; i < len; ++i)
    {
        if (filename[i] == '.')
        {
            if (dot_found || i == 0 || i == len - 1)
            {
                return -RES_EUNKNOWN;
            }
            dot_found = true;
        }
        else if (!isalnum(filename[i]) || filename[i] == '/')
        {
            return -RES_EUNKNOWN;
        }
    }

    return 0;
}

static int name_to_fat32_nameext(const char *filename, char *nameext)
{
    if (is_valid_fat32_filename(filename) < 0)
    {
        return -RES_EUNKNOWN;
    }

    memset(nameext, 0x20, 11);

    size_t len = strlen(filename);
    size_t i = 0;
    size_t j = 0;

    for (; i < len && j < 8 && filename[i] != '.'; ++i, ++j)
    {
        nameext[j] = toupper(filename[i]);
    }

    if (filename[i] == '.')
    {
        ++i;
    }

    for (j = 8; i < len && j < 11; ++i, ++j)
    {
        nameext[j] = toupper(filename[i]);
    }

    return 0;
}

static void read_cluster(uint32_t cluster_num, uint8_t *buf, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t fat_size = (uint32_t)boot_sector->bpb.FAT_size_16;
    if (fat_size == 0)
    {
        fat_size = boot_sector->bpb.FAT_size_32;
    }

    size_t data_start = boot_sector->bpb.reserved_sector_count + boot_sector->bpb.num_FATs * fat_size;
    size_t lba = data_start + cluster_num - 2;

    read_fat_device(dev, lba, boot_sector->bpb.sectors_per_cluster, buf);
}

static void write_cluster(uint32_t cluster_num, const uint8_t *buf, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t fat_size = (uint32_t)boot_sector->bpb.FAT_size_16;
    if (fat_size == 0)
    {
        fat_size = boot_sector->bpb.FAT_size_32;
    }

    size_t data_start = boot_sector->bpb.reserved_sector_count + boot_sector->bpb.num_FATs * fat_size;
    size_t lba = data_start + cluster_num - 2;

    write_fat_device(dev, lba, boot_sector->bpb.sectors_per_cluster, buf);
}

static uint32_t read_fat_entry(uint32_t cluster_num, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    size_t fat_offset = cluster_num * sizeof(uint32_t);
    size_t sector_offset = fat_offset / boot_sector->bpb.bytes_per_sector;
    size_t lba_offset = fat_offset % boot_sector->bpb.bytes_per_sector;

    size_t lba = boot_sector->bpb.reserved_sector_count + sector_offset;

    uint32_t *fat_section = kmalloc(boot_sector->bpb.bytes_per_sector);
    read_fat_device(dev, lba, 1, (uint8_t *)fat_section);

    uint32_t fat_entry = *(uint32_t *)((uint8_t *)fat_section + lba_offset);

    kfree(fat_section);
    return fat_entry;
}

static void write_fat_entry(uint32_t cluster_num, uint32_t value, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    size_t fat_offset = cluster_num * sizeof(uint32_t);
    size_t sector_offset = fat_offset / boot_sector->bpb.bytes_per_sector;
    size_t lba_offset = fat_offset % boot_sector->bpb.bytes_per_sector;

    size_t lba = boot_sector->bpb.reserved_sector_count + sector_offset;

    uint32_t *fat_section = kmalloc(boot_sector->bpb.bytes_per_sector);
    read_fat_device(dev, lba, 1, (uint8_t *)fat_section);

    uint32_t *fat_entry = (uint32_t *)((uint8_t *)fat_section + lba_offset);
    *fat_entry = value;
    write_fat_device(dev, lba, 1, (uint8_t *)fat_section);

    kfree(fat_section);
}

static directory_entry_t *find_entry_by_name(const char *name, uint32_t directory_cluster_num, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t current_cluster = directory_cluster_num;
    uint8_t *cluster_buf = kmalloc(boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector);

    while (current_cluster < 0x0FFFFFF8)
    {
        read_cluster(current_cluster, cluster_buf, boot_sector, dev);
        directory_entry_t *direntries = (directory_entry_t *)cluster_buf;
        for (uint8_t i = 0; i < 16; i++)
        {
            if (direntries[i].name[0] == 0x00)
            {
                kfree(cluster_buf);
                return NULL;
            }

            if ((uint8_t)direntries[i].name[0] == 0xE5)
            {
                continue;
            }

            if ((direntries[i].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
            {
                continue;
            }

            char filename[256];
            if (i >= 1 && ((direntries[i - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
            {
                size_t long_filename_size = 0;

                size_t j = i;
                while (j >= 1 && ((direntries[j - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
                {
                    j--;
                    for (uint8_t k = 0; k < 5; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name1[k]);
                    }
                    for (uint8_t k = 0; k < 6; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name2[k]);
                    }
                    for (uint8_t k = 0; k < 2; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name3[k]);
                    }
                }

                filename[long_filename_size] = '\0';
            }
            else
            {
                fat32_nameext_to_name(direntries[i].nameext, filename);
            }

            if (strncmp(filename, name, 11) == 0)
            {
                directory_entry_t *res = kmalloc(sizeof(directory_entry_t));
                memcpy(res, &direntries[i], sizeof(directory_entry_t));
                kfree(cluster_buf);
                return res;
            }
        }

        current_cluster = read_fat_entry(current_cluster, boot_sector, dev);
    }

    kfree(cluster_buf);
    return NULL;
}

static int modify_direntry_in_directory(const char *name, uint32_t directory_cluster_num, directory_entry_t *new_direntry, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t current_cluster = directory_cluster_num;
    uint8_t *cluster_buf = kmalloc(boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector);

    while (current_cluster < 0x0FFFFFF8)
    {
        read_cluster(current_cluster, cluster_buf, boot_sector, dev);
        directory_entry_t *direntries = (directory_entry_t *)cluster_buf;
        for (uint8_t i = 0; i < 16; i++)
        {
            if (direntries[i].name[0] == 0x00)
            {
                kfree(cluster_buf);
                return -RES_EUNKNOWN;
            }

            if ((uint8_t)direntries[i].name[0] == 0xE5)
            {
                continue;
            }

            if ((direntries[i].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
            {
                continue;
            }

            char filename[256];
            if (i >= 1 && ((direntries[i - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
            {
                size_t long_filename_size = 0;

                size_t j = i;
                while (j >= 1 && ((direntries[j - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
                {
                    j--;
                    for (uint8_t k = 0; k < 5; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name1[k]);
                    }
                    for (uint8_t k = 0; k < 6; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name2[k]);
                    }
                    for (uint8_t k = 0; k < 2; k++)
                    {
                        filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name3[k]);
                    }
                }

                filename[long_filename_size] = '\0';
            }
            else
            {
                fat32_nameext_to_name(direntries[i].nameext, filename);
            }

            if (strncmp(filename, name, 11) == 0)
            {
                memcpy(&direntries[i], new_direntry, sizeof(directory_entry_t));
                write_cluster(current_cluster, cluster_buf, boot_sector, dev);
                return 0;
            }
        }

        current_cluster = read_fat_entry(current_cluster, boot_sector, dev);
    }

    kfree(cluster_buf);
    return -RES_EUNKNOWN;
}

static directory_entry_t *find_entry_by_index(size_t index, uint32_t directory_cluster_num, char *filename /*256 bytes*/, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t current_cluster = directory_cluster_num;
    uint8_t *cluster_buf = kmalloc(boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector);

    size_t j = 0;
    while (current_cluster < 0x0FFFFFF8)
    {
        read_cluster(current_cluster, cluster_buf, boot_sector, dev);
        directory_entry_t *direntries = (directory_entry_t *)cluster_buf;
        for (uint8_t i = 0; i < 16; i++)
        {
            if (direntries[i].name[0] == 0x00)
            {
                kfree(cluster_buf);
                return NULL;
            }

            if ((uint8_t)direntries[i].name[0] == 0xE5)
            {
                index++;
                continue;
            }

            if ((direntries[i].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
            {
                index++;
                continue;
            }

            if (index == j + i)
            {
                if (i >= 1 && ((direntries[i - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
                {
                    size_t long_filename_size = 0;

                    size_t j = i;
                    while (j >= 1 && ((direntries[j - 1].attr & ATTR_LONG_NAME) == ATTR_LONG_NAME))
                    {
                        j--;
                        for (uint8_t k = 0; k < 5; k++)
                        {
                            filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name1[k]);
                        }
                        for (uint8_t k = 0; k < 6; k++)
                        {
                            filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name2[k]);
                        }
                        for (uint8_t k = 0; k < 2; k++)
                        {
                            filename[long_filename_size++] = utf16_to_ascii(((lfn_directory_entry_t *)&direntries[j])->name3[k]);
                        }
                    }

                    filename[long_filename_size] = '\0';
                }
                else
                {
                    fat32_nameext_to_name(direntries[i].nameext, filename);
                }

                directory_entry_t *res = kmalloc(sizeof(directory_entry_t));
                memcpy(res, &direntries[i], sizeof(directory_entry_t));
                kfree(cluster_buf);
                return res;
            }
        }

        current_cluster = read_fat_entry(current_cluster, boot_sector, dev);
        j += 16;
    }

    kfree(cluster_buf);
    return NULL;
}

static directory_entry_t *direntry_from_path(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = (directory_entry_t *)1;

    char *path_cpy = strdup(path);
    char *pch = strtok(path_cpy, "/");

    size_t current_cluster = boot_sector->bpb.root_cluster;
    while (pch != NULL)
    {
        direntry = find_entry_by_name(pch, current_cluster, boot_sector, dev);
        if (!direntry)
        {
            kfree(path_cpy);
            return NULL;
        }
        current_cluster = (((uint32_t)direntry->first_cluster_hi) << 16) | ((uint32_t)direntry->first_cluster_low);
        pch = strtok(NULL, "/");
        if (pch != NULL)
        {
            kfree(direntry);
        }
    }
    kfree(path_cpy);

    return direntry;
}

char *get_parent_directory(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    size_t len = strlen(path);
    if (len == 0)
    {
        return strdup("");
    }

    while (len > 0 && path[len - 1] == '/')
    {
        len--;
    }

    const char *last_slash = memrchr(path, '/', len);
    if (last_slash == NULL)
    {
        return strdup("");
    }

    size_t parent_len = last_slash - path + 1;

    char *parent_dir = (char *)kmalloc(parent_len + 1);
    if (parent_dir == NULL)
    {
        return NULL;
    }

    strncpy(parent_dir, path, parent_len);
    parent_dir[parent_len] = '\0';

    return parent_dir;
}

char *get_filename(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
    {
        return strdup(path);
    }
    else if (*(last_slash + 1) == '\0')
    {
        return strdup("");
    }
    else
    {
        return strdup(last_slash + 1);
    }
}

static uint32_t first_cluster_from_direntry(directory_entry_t *direntry, boot_sector_t *boot_sector)
{
    if ((uintptr_t)direntry == 1)
    {
        return boot_sector->bpb.root_cluster;
    }
    return (((uint32_t)direntry->first_cluster_hi) << 16) | ((uint32_t)direntry->first_cluster_low);
}

static uint32_t first_cluster_from_path(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    if (strcmp(path, "/") == 0) // TODO: find a more 'bulletproof' way to check (ignore whitespace, ...)
    {
        return boot_sector->bpb.root_cluster;
    }

    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (!direntry)
    {
        return -RES_EUNKNOWN;
    }

    return first_cluster_from_direntry(direntry, boot_sector);
}

static int modify_direntry(const char *path, directory_entry_t *new_direntry, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = (directory_entry_t *)1;

    char *new_path = get_parent_directory(path);
    if (new_path == NULL)
    {
        return -RES_EUNKNOWN;
    }
    char *pch = strtok(new_path, "/");

    size_t current_cluster = boot_sector->bpb.root_cluster;
    while (pch != NULL)
    {
        direntry = find_entry_by_name(pch, current_cluster, boot_sector, dev);
        if (!direntry)
        {
            kfree(new_path);
            return -RES_EUNKNOWN;
        }
        current_cluster = (((uint32_t)direntry->first_cluster_hi) << 16) | ((uint32_t)direntry->first_cluster_low);
        pch = strtok(NULL, "/");
        if (pch != NULL)
        {
            kfree(direntry);
        }
    }
    kfree(new_path);

    char *path_end = get_filename(path);
    if (path_end == NULL)
    {
        return -RES_EUNKNOWN;
    }
    
    int status = modify_direntry_in_directory(path_end, first_cluster_from_direntry(direntry, boot_sector), new_direntry, boot_sector, dev);
    if (status < 0)
    {
        return status;
    }

    kfree(path_end);
    if ((uintptr_t)direntry != 1)
    {
        kfree(direntry);
    }

    return 0;
}

static uint32_t find_last_cluster(uint32_t start_cluster, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t current_cluster = start_cluster;
    uint32_t next_cluster;

    while ((next_cluster = read_fat_entry(current_cluster, boot_sector, dev)) < 0x0FFFFFF8)
    {
        current_cluster = next_cluster;
    }

    return current_cluster;
}

static uint32_t allocate_new_cluster(uint32_t last_cluster, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t fat_size = boot_sector->bpb.FAT_size_32;
    uint8_t *fat_table = kmalloc(fat_size * boot_sector->bpb.bytes_per_sector);
    read_fat_device(dev, boot_sector->bpb.reserved_sector_count, fat_size, fat_table);

    uint32_t new_cluster = 2;
    for (; new_cluster < (fat_size * boot_sector->bpb.bytes_per_sector / 4); new_cluster++)
    {
        if (((uint32_t *)fat_table)[new_cluster] == 0)
        {
            ((uint32_t *)fat_table)[new_cluster] = 0x0FFFFFF8;
            break;
        }
    }

    if (new_cluster >= (fat_size * boot_sector->bpb.bytes_per_sector / 4))
    {
        kfree(fat_table);
        return 0x0FFFFFF8;
    }

    if (last_cluster != 0x0FFFFFF8)
    {
        ((uint32_t *)fat_table)[last_cluster] = new_cluster;
    }

    write_fat_device(dev, boot_sector->bpb.reserved_sector_count, fat_size, fat_table);
    kfree(fat_table);

    return new_cluster;
}

int new_direntry_in_cluster(const char *dir_path, directory_entry_t *new_direntry, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t current_cluster = first_cluster_from_path(dir_path, boot_sector, dev);
    if (current_cluster == (uint32_t)-1)
    {
        return -RES_EUNKNOWN;
    }

    uint32_t bytes_per_cluster = boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector;
    uint8_t *cluster_buf = kmalloc(bytes_per_cluster);

    if (!cluster_buf)
    {
        return -RES_NOMEM;
    }

    uint32_t old_cluster = current_cluster;
    while (current_cluster < 0x0FFFFFF8)
    {
        old_cluster = current_cluster;
        read_cluster(current_cluster, cluster_buf, boot_sector, dev);

        for (size_t offset = 0; offset < bytes_per_cluster; offset += sizeof(directory_entry_t))
        {
            directory_entry_t *entry = (directory_entry_t *)(cluster_buf + offset);

            if (entry->name[0] == 0x00 || (uint8_t)entry->name[0] == 0xE5)
            {
                memcpy(entry, new_direntry, sizeof(directory_entry_t));

                write_cluster(current_cluster, cluster_buf, boot_sector, dev);

                kfree(cluster_buf);
                return 0;
            }
        }

        current_cluster = read_fat_entry(current_cluster, boot_sector, dev);
    }

    uint32_t new_cluster = allocate_new_cluster(old_cluster, boot_sector, dev);
    if (new_cluster == (uint32_t)-1)
    {
        kfree(cluster_buf);
        return -RES_EUNKNOWN;
    }

    memset(cluster_buf, 0, bytes_per_cluster);
    memcpy(cluster_buf, new_direntry, sizeof(directory_entry_t));

    write_cluster(new_cluster, cluster_buf, boot_sector, dev);

    kfree(cluster_buf);
    return 0;
}

int read_fat32(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev, size_t offset, size_t size, uint8_t *buffer)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL || (uintptr_t)direntry == 1)
    {
        return -RES_EUNKNOWN;
    }
    if ((direntry->attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    direntry->last_access_date = get_fat32_date();
    if (modify_direntry(path, direntry, boot_sector, dev) < 0)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }
    kfree(direntry);

    uint32_t current_cluster = first_cluster_from_path(path, boot_sector, dev);
    if (current_cluster == (uint32_t)-1)
    {
        return -RES_EUNKNOWN;
    }

    uint32_t bytes_per_cluster = boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector;
    uint8_t *cluster_buf = kmalloc(bytes_per_cluster);

    uint32_t current_cluster_num = 0;
    size_t buffer_index = 0;

    while (current_cluster < 0x0FFFFFF8)
    {
        uint32_t byte_offset = current_cluster_num * bytes_per_cluster;
        if (offset >= byte_offset && offset < (byte_offset + bytes_per_cluster))
        {
            read_cluster(current_cluster, cluster_buf, boot_sector, dev);
            uint32_t cluster_offset = offset - byte_offset;
            size_t cpy_size = (size > (bytes_per_cluster - cluster_offset)) ? (bytes_per_cluster - cluster_offset) : size;

            memcpy((void *)((uintptr_t)buffer + buffer_index), (void *)((uintptr_t)cluster_buf + cluster_offset), cpy_size);
            buffer_index += cpy_size;

            if (buffer_index == size)
            {
                kfree(cluster_buf);
                return 0;
            }
        }
        else if (offset < byte_offset && (offset + size) >= (byte_offset + bytes_per_cluster))
        {
            read_cluster(current_cluster, cluster_buf, boot_sector, dev);
            memcpy((void *)((uintptr_t)buffer + buffer_index), (void *)((uintptr_t)cluster_buf), bytes_per_cluster);
            buffer_index += bytes_per_cluster;
        }
        else if ((offset + size) >= byte_offset && (offset + size) < (byte_offset + bytes_per_cluster))
        {
            read_cluster(current_cluster, cluster_buf, boot_sector, dev);
            memcpy((void *)((uintptr_t)buffer + buffer_index), cluster_buf, size - buffer_index);
            kfree(cluster_buf);
            return 0;
        }

        current_cluster = read_fat_entry(current_cluster, boot_sector, dev);
        current_cluster_num++;
    }

    kfree(cluster_buf);
    return 0;
}

int write_fat32(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev, size_t size, const uint8_t *buffer)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL || (uintptr_t)direntry == 1)
    {
        return -RES_EUNKNOWN;
    }
    if ((direntry->attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    size_t filesize = direntry->file_size;
    uint32_t first_cluster = first_cluster_from_direntry(direntry, boot_sector);

    uint32_t cluster_size = boot_sector->bpb.sectors_per_cluster * boot_sector->bpb.bytes_per_sector;
    uint32_t last_cluster = find_last_cluster(first_cluster, boot_sector, dev);
    uint8_t *cluster_buf = kmalloc(cluster_size);
    if (!cluster_buf)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    size_t offset_in_cluster = filesize % cluster_size;
    size_t space_in_cluster = cluster_size - offset_in_cluster;
    size_t size_left = size;
    size_t buf_index = 0;

    if (space_in_cluster > 0)
    {
        read_cluster(last_cluster, cluster_buf, boot_sector, dev);

        size_t to_copy = (size_left < space_in_cluster) ? size_left : space_in_cluster;
        memcpy(cluster_buf + offset_in_cluster, buffer, to_copy);

        write_cluster(last_cluster, cluster_buf, boot_sector, dev);

        buf_index += to_copy;
        size_left -= to_copy;
    }

    while (size_left > 0)
    {
        last_cluster = allocate_new_cluster(last_cluster, boot_sector, dev);
        if (last_cluster == (uint32_t)-1)
        {
            kfree(direntry);
            kfree(cluster_buf);
            return -RES_EUNKNOWN;
        }

        size_t to_copy = (size_left < cluster_size) ? size_left : cluster_size;
        memcpy(cluster_buf, buffer + buf_index, to_copy);
        memset(cluster_buf + to_copy, 0, cluster_size - to_copy);

        write_cluster(last_cluster, cluster_buf, boot_sector, dev);

        buf_index += to_copy;
        size_left -= to_copy;
    }

    kfree(cluster_buf);

    direntry->file_size += size;
    direntry->write_date = get_fat32_date();
    direntry->write_time = get_fat32_time();
    direntry->last_access_date = get_fat32_date();

    if (modify_direntry(path, direntry, boot_sector, dev) < 0)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    kfree(direntry);
    return 0;
}

#define FS_FILE 0x01
#define FS_DIRECTORY 0x02
#define FS_SYMLINK 0x03

#define MASK_READONLY 1 << 1
#define MASK_HIDDEN 1 << 2
#define MASK_SYSTEM 1 << 3

typedef struct
{
    size_t filesize;
    uint32_t mask;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t last_access_date;
    uint32_t flags;
} file_info_t;

file_info_t *stat_fat32(file_info_t *file_info, const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL)
    {
        return NULL;
    }

    if (strcmp(path, "/") != 0)
    {
        direntry->last_access_date = get_fat32_date();
        if (modify_direntry(path, direntry, boot_sector, dev) < 0)
        {
            kfree(direntry);
            return NULL;
        }
    }

    if ((uintptr_t)direntry == 1)
    {
        file_info->flags = FS_DIRECTORY;
        file_info->mask = MASK_SYSTEM;

        return file_info;
    }

    bool is_dir = (direntry->attr & ATTR_DIRECTORY) == ATTR_DIRECTORY;

    memset(file_info, 0x00, sizeof(file_info_t));

    file_info->creation_time = direntry->creation_time;
    file_info->creation_date = direntry->creation_date;
    file_info->write_time = direntry->write_time;
    file_info->write_date = direntry->write_date;
    file_info->last_access_date = direntry->last_access_date;
    file_info->mask = 0;

    if ((direntry->attr & ATTR_READ_ONLY) == ATTR_READ_ONLY)
    {
        file_info->mask |= MASK_READONLY;
    }

    if (direntry->attr & ATTR_HIDDEN)
    {
        file_info->mask |= MASK_HIDDEN;
    }

    if (direntry->attr & ATTR_SYSTEM)
    {
        file_info->mask |= MASK_SYSTEM;
    }

    if (is_dir)
    {
        file_info->flags |= FS_DIRECTORY;
    }
    else
    {
        file_info->flags |= FS_FILE;
        file_info->filesize = direntry->file_size;
    }

    kfree(direntry);

    return file_info;
}

size_t readdir_fat32(char *res, const char *path, size_t index, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    uint32_t cluster = first_cluster_from_path(path, boot_sector, dev);
    if (cluster == (uint32_t)-1)
    {
        return 0;
    }

    if (cluster != boot_sector->bpb.root_cluster)
    {
        index += 2; // skip . and ..
    }

    if (strcmp(path, "/") != 0)
    {
        directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
        if (direntry == NULL)
        {
            return -RES_EUNKNOWN;
        }
        direntry->last_access_date = get_fat32_date();
        if (modify_direntry(path, direntry, boot_sector, dev) < 0)
        {
            kfree(direntry);
            return 0;
        }
    }

    char filename[256];
    directory_entry_t *direntry = find_entry_by_index(index, cluster, filename, boot_sector, dev);
    if (!direntry)
    {
        return 0;
    }

    kfree(direntry);

    strcpy(res, filename);
    return strlen(filename);
}

int clear_fat32(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL || (uintptr_t)direntry == 1)
    {
        return -RES_EUNKNOWN;
    }
    if ((direntry->attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    uint32_t first_cluster = first_cluster_from_direntry(direntry, boot_sector);
    uint32_t current_cluster = first_cluster;
    uint32_t next_cluster = 0;

    while ((next_cluster = read_fat_entry(current_cluster, boot_sector, dev)) < 0x0FFFFFF8)
    {
        write_fat_entry(next_cluster, 0, boot_sector, dev);
        current_cluster = next_cluster;
    }
    write_fat_entry(first_cluster, 0x0FFFFFF8, boot_sector, dev);

    direntry->file_size = 0;
    direntry->write_date = get_fat32_date();
    direntry->write_time = get_fat32_time();
    direntry->last_access_date = get_fat32_date();

    if (modify_direntry(path, direntry, boot_sector, dev) < 0)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    kfree(direntry);
    return 0;
}

int write_mask_fat32(const char *path, uint32_t mask, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL || (uintptr_t)direntry == 1)
    {
        return -RES_EUNKNOWN;
    }

    uint8_t attr = 0;
    if ((mask & MASK_READONLY) == MASK_READONLY)
    {
        attr |= ATTR_READ_ONLY;
    }
    if ((mask & MASK_HIDDEN) == MASK_HIDDEN)
    {
        attr |= ATTR_HIDDEN;
    }
    if ((mask & MASK_SYSTEM) == MASK_SYSTEM)
    {
        attr |= ATTR_SYSTEM;
    }

    if (direntry->attr == attr)
    {
        kfree(direntry);
        return 0;
    }

    direntry->attr = attr;
    if (modify_direntry(path, direntry, boot_sector, dev) < 0)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    kfree(direntry);
    return 0;
}

int create_fat32(const char *path, uint32_t mask, uint32_t flags, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    file_info_t info;
    if (stat_fat32(&info, path, boot_sector, dev) != NULL)
    {
        return -RES_EUNKNOWN; // file exists
    }

    directory_entry_t new_direntry;

    char *filename = get_filename(path);
    if (!filename)
    {
        return -RES_EUNKNOWN;
    }

    if (name_to_fat32_nameext(filename, new_direntry.nameext) < 0)
    {
        return -RES_EUNKNOWN;
    }

    kfree(filename);

    uint8_t attr = 0;
    if ((mask & MASK_READONLY) == MASK_READONLY)
    {
        attr |= ATTR_READ_ONLY;
    }
    if ((mask & MASK_HIDDEN) == MASK_HIDDEN)
    {
        attr |= ATTR_HIDDEN;
    }
    if ((mask & MASK_SYSTEM) == MASK_SYSTEM)
    {
        attr |= ATTR_SYSTEM;
    }
    if (flags == FS_DIRECTORY)
    {
        attr |= ATTR_DIRECTORY;
    }

    uint32_t first_cluster = allocate_new_cluster(0x0FFFFFF8, boot_sector, dev);
    if (first_cluster == 0x0FFFFFF8)
    {
        return -RES_EUNKNOWN;
    }
    
    new_direntry.attr = attr;
    new_direntry.reserved = 0;
    new_direntry.creation_time_tenth = get_fat32_time_tenth();
    new_direntry.creation_time = get_fat32_time();
    new_direntry.creation_date = get_fat32_date();
    new_direntry.last_access_date = get_fat32_date();
    new_direntry.first_cluster_hi = (uint16_t)((first_cluster >> 16) & 0xFFFF);
    new_direntry.first_cluster_low = (uint16_t)(first_cluster & 0xFFFF);
    new_direntry.write_time = get_fat32_time();
    new_direntry.write_date = get_fat32_date();
    new_direntry.file_size = 0;

    char *dirpath = get_parent_directory(path);
    if (!dirpath)
    {
        return -RES_EUNKNOWN;
    }

    if (new_direntry_in_cluster(dirpath, &new_direntry, boot_sector, dev) < 0)
    {
        kfree(dirpath);
        return -RES_EUNKNOWN;
    }

    kfree(dirpath);
    return 0;
}

int delete_fat32(const char *path, boot_sector_t *boot_sector, virtual_blockdev_t *dev)
{
    directory_entry_t *direntry = direntry_from_path(path, boot_sector, dev);
    if (direntry == NULL || (uintptr_t)direntry == 1)
    {
        return -RES_EUNKNOWN;
    }
    if ((direntry->attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
    {
        kfree(direntry);
        return -RES_EUNKNOWN;
    }

    uint32_t first_cluster = first_cluster_from_direntry(direntry, boot_sector);
    uint32_t current_cluster = first_cluster;
    uint32_t next_cluster = 0;

    while ((next_cluster = read_fat_entry(current_cluster, boot_sector, dev)) < 0x0FFFFFF8)
    {
        write_fat_entry(next_cluster, 0, boot_sector, dev);
        current_cluster = next_cluster;
    }
    write_fat_entry(first_cluster, 0, boot_sector, dev);
    kfree(direntry);

    char *dirpath = get_parent_directory(path);
    if (!dirpath)
    {
        return -RES_EUNKNOWN;
    }

    uint32_t directory_cluster = first_cluster_from_path(dirpath, boot_sector, dev);
    if (directory_cluster == 0)
    {
        kfree(dirpath);
        return -RES_EUNKNOWN;
    }
    kfree(dirpath);

    char *filename = get_filename(path);
    if (!filename)
    {
        return -RES_EUNKNOWN;
    }

    directory_entry_t new_direntry;
    new_direntry.name[0] = 0xE5;

    if (modify_direntry_in_directory(filename, directory_cluster, &new_direntry, boot_sector, dev) < 0)
    {
        kfree(filename);
        return -RES_EUNKNOWN;
    }

    kfree(filename);
    
    return 0;
}

file_node_t *fat32_open(const char *path, uint8_t action, virtual_blockdev_t *bdev, void *data)
{
    if (!bdev || !data)
    {
        return NULL;
    }

    file_node_t *node = kmalloc(sizeof(file_node_t));
    if (!node)
    {
        return NULL;
    }

    if (action == OPEN_ACTION_CREATE)
    {
        if (create_fat32(path, 0, FS_FILE, (boot_sector_t *)data, bdev) < 0)
        {
            return NULL;
        }
    }
    else if (action == OPEN_ACTION_CLEAR)
    {
        if (clear_fat32(path, (boot_sector_t *)data, bdev) < 0)
        {
            return NULL;
        }
    }
    
    file_info_t info;
    if (!stat_fat32(&info, path, (boot_sector_t *)data, bdev))
    {
        return NULL;
    }

    node->filesize = info.filesize;
    node->mask = info.mask;
    node->creation_time = info.creation_time;
    node->creation_date = info.creation_date;
    node->write_time = info.write_time;
    node->write_date = info.write_date;
    node->last_access_date = info.last_access_date;
    node->flags = info.flags;
    node->_data = 0; // TODO: maybe cache cluster number here

    return node;
}

int fat32_close(file_node_t *node, virtual_blockdev_t *bdev, void *data)
{
    if (!node)
    {
        return -RES_EUNKNOWN;
    }

    (void)bdev;
    (void)data;

    kfree(node);

    return 0;
}

int fat32_read(file_node_t *node, size_t size, uint8_t *buf, virtual_blockdev_t *bdev, void *data)
{
    if (read_fat32(node->local_path, (boot_sector_t *)data, bdev, node->offset, size, buf) < 0)
    {
        return -RES_EUNKNOWN;
    }

    node->offset += size;
    return 0;
}

int fat32_write(file_node_t *node, size_t size, const uint8_t *buf, virtual_blockdev_t *bdev, void *data)
{
    if (write_fat32(node->local_path, (boot_sector_t *)data, bdev, size, buf) < 0)
    {
        return -RES_EUNKNOWN;
    }

    node->offset += size;
    return 0;
}

int fat32_readdir(file_node_t *node, int index, char *path, virtual_blockdev_t *bdev, void *data)
{
    char filename[MAX_PATH];
    if (readdir_fat32(filename, node->local_path, (size_t)index, (boot_sector_t *)data, bdev) == 0)
    {
        return -RES_EUNKNOWN;
    }

    strncpy(path, node->local_path, MAX_PATH);
    size_t len = strlen(path);
    strncpy((char *)((uintptr_t)path + len), filename, MAX_PATH-len);

    return 0;
}

int fat32_delete(file_node_t *node, virtual_blockdev_t *bdev, void *data)
{
    if (delete_fat32(node->local_path, (boot_sector_t *)data, bdev) < 0)
    {
        return -RES_EUNKNOWN;
    }

    return 0;
}

void *fat32_init(virtual_blockdev_t *bdev)
{
    if (!bdev)
    {
        return NULL;
    }

    return (void *)scan_fat(bdev->bdev->block_size, bdev);
}

int fat32_free(virtual_blockdev_t *bdev, void *data)
{
    (void)bdev;
    if (!data)
    {
        return -RES_EUNKNOWN;
    }

    free_fat((boot_sector_t *)data);
    return 0;
}

int fat32_test(virtual_blockdev_t *bdev)
{
    if (!bdev)
    {
        return -RES_ETEST;
    }

    boot_sector_t boot_sector;
    read_fat_device(bdev, 0, 1, (uint8_t *)&boot_sector);
    if (verify_boot_sector(&boot_sector, bdev->bdev->block_size) < 0)
    {
        return -RES_ETEST;
    }

    return 0;
}
