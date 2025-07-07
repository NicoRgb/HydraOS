#include <kernel/multiboot2.h>
#include <kernel/string.h>
#include <kernel/dev/pci.h>
#include <kernel/dev/devm.h>
#include <kernel/kprintf.h>
#include <kernel/smm.h>
#include <kernel/isr.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmm.h>
#include <kernel/kernel.h>
#include <kernel/fs/vpt.h>
#include <kernel/fs/vfs.h>
#include <kernel/proc/task.h>
#include <kernel/proc/scheduler.h>
#include <kernel/pit.h>

extern driver_t e9_driver;
extern driver_t vga_driver;
extern driver_t ps2_driver;

boot_info_t boot_info;

static int process_boot_parameter(const char *key, char *value)
{
    if (strcmp(key, "klog") == 0)
    {
        char *colon_pos = strchr(value, ':');
        if (colon_pos == NULL) 
        {
            return -1;
        }

        *colon_pos = '\0';
        boot_info.tty_vendor = (uint16_t)atoi(value);

        char *second_value_str = colon_pos + 1;
        boot_info.tty_device = (uint16_t)atoi(second_value_str);
    }
    
    return 0;
}

static int process_boot_parameters(char *parameters)
{
    char *key_start = parameters;
    char *value_start = NULL;

    while (*parameters != '\0')
    {
        if (*parameters == '=')
        {
            *parameters = '\0';
            value_start = parameters + 1;
        }
        else if (*parameters == ' ' || *(parameters + 1) == '\0')
        {
            if (*parameters == ' ')
            {
                *parameters = '\0';
            }
            else if (*(parameters + 1) == '\0')
            {
                parameters++;
            }
            if (value_start)
            {
                if (process_boot_parameter(key_start, value_start) < 0)
                {
                    return -1;
                }
            }
            key_start = parameters + 1;
            value_start = NULL;
        }
        parameters++;
    }

    return 0;
}

static KRES process_mmap(struct multiboot_tag_mmap *multiboot2_mmap)
{
    if (!multiboot2_mmap)
    {
        return -RES_INVARG;
    }

    for (multiboot_memory_map_t *entry = multiboot2_mmap->entries;
         (uintptr_t)entry < (uintptr_t)multiboot2_mmap + multiboot2_mmap->size;
         entry = (multiboot_memory_map_t *)((uintptr_t)entry + multiboot2_mmap->entry_size))
    {
        switch (entry->type)
        {
        case MULTIBOOT_MEMORY_AVAILABLE:
            boot_info.total_memory = entry->addr + entry->len;

            boot_info.memory_map[boot_info.num_mmap_entries].addr = entry->addr;
            boot_info.memory_map[boot_info.num_mmap_entries].size = entry->len;
            boot_info.memory_map[boot_info.num_mmap_entries].type = MMAP_ENTRY_TYPE_AVAILABLE;
            if (entry->addr == 0)
            {
                boot_info.memory_map[boot_info.num_mmap_entries].addr += 4096;
                boot_info.memory_map[boot_info.num_mmap_entries].size -= 4096;
            }
            break;
        case MULTIBOOT_MEMORY_RESERVED:
            boot_info.memory_map[boot_info.num_mmap_entries].addr = entry->addr;
            boot_info.memory_map[boot_info.num_mmap_entries].size = entry->len;
            boot_info.memory_map[boot_info.num_mmap_entries].type = MMAP_ENTRY_TYPE_RESERVED;
            break;
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
            boot_info.memory_map[boot_info.num_mmap_entries].addr = entry->addr;
            boot_info.memory_map[boot_info.num_mmap_entries].size = entry->len;
            boot_info.memory_map[boot_info.num_mmap_entries].type = MMAP_ENTRY_TYPE_ACPI_RECLAIMABLE;
            break;
        case MULTIBOOT_MEMORY_NVS:
            boot_info.memory_map[boot_info.num_mmap_entries].addr = entry->addr;
            boot_info.memory_map[boot_info.num_mmap_entries].size = entry->len;
            boot_info.memory_map[boot_info.num_mmap_entries].type = MMAP_ENTRY_TYPE_RESERVED;
            break;
        case MULTIBOOT_MEMORY_BADRAM:
            boot_info.memory_map[boot_info.num_mmap_entries].addr = entry->addr;
            boot_info.memory_map[boot_info.num_mmap_entries].size = entry->len;
            boot_info.memory_map[boot_info.num_mmap_entries].type = MMAP_ENTRY_TYPE_RESERVED;
            break;
        }

        boot_info.num_mmap_entries++;
    }

    return RES_SUCCESS;
}

static KRES process_elf_sections(struct multiboot_tag_elf_sections *multiboot2_elf_sections)
{
    if (!multiboot2_elf_sections)
    {
        return -RES_INVARG;
    }

    if (multiboot2_elf_sections->num >= MAX_ELF_SECTIONS)
    {
        return -RES_OVERFLOW;
    }

    boot_info.num_elf_sections = multiboot2_elf_sections->num;
    boot_info.shndx = multiboot2_elf_sections->shndx;
    boot_info.symtabndx = (uint32_t)-1;

    const Elf64_Shdr *sections = (const Elf64_Shdr *)((uintptr_t)multiboot2_elf_sections + sizeof(struct multiboot_tag_elf_sections));

    for (uint32_t i = 0; i < multiboot2_elf_sections->num; i++)
    {
        const Elf64_Shdr *entry = &sections[i];
        memcpy(&boot_info.elf_sections[i], entry, sizeof(Elf64_Shdr));

        if (entry->sh_type == SHT_SYMTAB)
        {
            LOG_INFO("found kernel symbol table");
            boot_info.symtabndx = i;
        }
    }

    return RES_SUCCESS;
}

static KRES parse_multiboot2_structure(uint64_t multiboot2_struct_addr)
{
    KRES res = RES_SUCCESS;

    if (multiboot2_struct_addr & 7)
    {
        res = -RES_INVARG;
        goto exit;
    }

    memset(&boot_info, 0, sizeof(boot_info));

    struct multiboot_tag *tag;
    for (tag = (struct multiboot_tag *)(multiboot2_struct_addr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            LOG_INFO("found multiboot memory map");
            CHECK(process_mmap((struct multiboot_tag_mmap *)tag));
        }
        else if (tag->type == MULTIBOOT_TAG_TYPE_ELF_SECTIONS)
        {
            LOG_INFO("found multiboot elf sections");
            CHECK(process_elf_sections((struct multiboot_tag_elf_sections *)tag));
        }
        else if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE)
        {
            if (process_boot_parameters(((struct multiboot_tag_string *)tag)->string) < 0)
            {
                return -1;
            }
        }
    }

exit:
    return res;
}

extern int __kernel_start;
extern int __kernel_end;

page_table_t *kernel_pml4 = NULL;

void early_init(uint32_t multiboot_signature, uint64_t multiboot_information_structure)
{
    (void)multiboot_signature;
    klog_init(&klog_write_e9);
    memset(&boot_info, 0, sizeof(boot_info_t));

    if (IS_ERROR(parse_multiboot2_structure(multiboot_information_structure)))
    {
        PANIC("failed to parse multiboot2 information structure");
    }

    if (IS_ERROR(segmentation_init()))
    {
        PANIC("failed to initialize segmentation");
    }

    if (IS_ERROR(pmm_init(boot_info.memory_map, boot_info.num_mmap_entries, boot_info.total_memory))) // TODO: 64 bit address range
    {
        PANIC("failed to initialize page allocation");
    }

    for (uintptr_t i = (uintptr_t)&__kernel_start; i < (uintptr_t)&__kernel_end; i += PAGE_SIZE)
    {
        pmm_reserve((uint64_t *)i);
    }

    kernel_pml4 = pmm_alloc();
    if (!kernel_pml4)
    {
        PANIC("failed to allocate page");
    }

    uint64_t total_pages = boot_info.total_memory / PAGE_SIZE;
    for (uint64_t page = 0; page < total_pages; page++) // TODO: only map necessary
    {
        if (pml4_map(kernel_pml4, (void *)(page * PAGE_SIZE), (void *)(page * PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE) < 0)
        {
            PANIC("failed to map page");
        }
    }

    if (IS_ERROR(pml4_switch(kernel_pml4)))
    {
        PANIC("failed to switch pml4");
    }

    if (IS_ERROR(kmm_init(kernel_pml4, 0x1200000, 32 * PAGE_SIZE, 16))) // TODO: make dynamicly grow
    {
        PANIC("failed to initialize kernel heap");
    }

    if (IS_ERROR(pci_init()))
    {
        PANIC("failed to initialize pci");
    }

    if (IS_ERROR(interrupts_init()))
    {
        PANIC("failed to initialize interrupts");
    }

    if (IS_ERROR(pit_init(100)))
    {
        PANIC("failed to initialize pit");
    }

    enable_interrupts();

    LOG_INFO("early initialization complete");
}

void kmain()
{
    if (IS_ERROR(register_driver(&e9_driver)))
    {
        PANIC("failed to register driver");
    }

    if (IS_ERROR(register_driver(&vga_driver)))
    {
        PANIC("failed to register driver");
    }

    if (IS_ERROR(register_driver(&ps2_driver)))
    {
        PANIC("failed to register driver");
    }

    if (IS_ERROR(init_devices()))
    {
        PANIC("failed to initialize devices");
    }

    if (IS_ERROR(kprintf_init(get_device(boot_info.tty_vendor, boot_info.tty_device))))
    {
        PANIC("failed to initialize kprintf");
    }

    uint64_t i = 0;
    device_t *bdev = NULL;
    do
    {
        i++;
        bdev = get_device_by_index(i - 1);
        if (!bdev)
        {
            break;
        }
        
        if (bdev->type != DEVICE_BLOCK)
        {
            continue;
        }

        LOG_INFO("found disc %s with %ld sectors, id %ld", bdev->model, bdev->num_blocks, i - 1);

        if (scan_partition(bdev) < 0)
        {
            LOG_INFO("\x1b[31mfailed to scan partition table");
            continue;
        }

        for (int j = 0; j < 4; j++)
        {
            virtual_blockdev_t *part = get_virtual_blockdev(bdev, j);
            if (!part)
            {
                break;
            }
            LOG_INFO(" - parititon type: 0x%x", part->type);
            if (part->type == 0x83)
            {
                if (vfs_mount_blockdev(part) < 0)
                {
                    PANIC("failed to mount partition");
                }
                LOG_INFO("mounted");
            }
        }
    } while (i > 0);

    LOG_INFO("starting sysinit...");
    process_t *proc = process_create("0:/bin/sysinit");
    if (!proc)
    {
        PANIC("failed to load '0:/bin/sysinit'");
    }

    if (process_register(proc) < 0)
    {
        PANIC("failed to register process");
    }

    scheduler_init();

    syscall_init();
    execute_next_process();

    PANIC("failed to launch '0:/bin/sysinit'");
}
