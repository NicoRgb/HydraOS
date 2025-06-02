#include <kernel/proc/elf.h>
#include <kernel/proc/task.h>
#include <kernel/kmm.h>
#include <kernel/string.h>

static uint64_t elf_get_entry(Elf64_Ehdr *header)
{
    return header->e_entry;
}

const char elf_signature[] = {0x7f, 'E', 'L', 'F'};

static bool elf_valid_signature(const char *buffer)
{
    return memcmp(buffer, elf_signature, sizeof(elf_signature)) == 0;
}

static bool elf_valid_class(Elf64_Ehdr *header)
{
    return header->e_ident[EI_CLASS] == ELFCLASS64;
}

static bool elf_valid_encoding(Elf64_Ehdr *header)
{
    return header->e_ident[EI_DATA] == ELFDATA2LSB;
}

static bool elf_is_executable(Elf64_Ehdr *header)
{
    return header->e_type == ET_EXEC && header->e_entry >= PROCESS_VADDR;
}

static bool elf_has_program_header(Elf64_Ehdr *header)
{
    return header->e_phoff != 0;
}

Elf64_Ehdr *elf_header(elf_file_t *elf_file)
{
    return elf_file->file_content;
}

Elf64_Shdr *elf_sheader(Elf64_Ehdr *header)
{
    return (Elf64_Shdr *)((uintptr_t)header + header->e_shoff);
}

Elf64_Phdr *elf_pheader(Elf64_Ehdr *header)
{
    if (!elf_has_program_header(header))
    {
        return NULL;
    }

    return (Elf64_Phdr *)((uintptr_t)header + header->e_phoff);
}

Elf64_Shdr *elf_section(Elf64_Ehdr *header, int index)
{
    return &elf_sheader(header)[index];
}

Elf64_Phdr *elf_program_header(Elf64_Ehdr *header, int index)
{
    return &elf_pheader(header)[index];
}

char *elf_str_table(Elf64_Ehdr *header)
{
    return (char *)header + elf_section(header, header->e_shstrndx)->sh_offset;
}

int elf_validate_loaded(Elf64_Ehdr *header)
{
    return (elf_valid_signature((char *)header) && elf_valid_class(header) && elf_valid_encoding(header) && elf_has_program_header(header) && elf_is_executable(header)) ? RES_SUCCESS : -RES_EUNKNOWN;
}

int elf_process_load(elf_file_t *elf_file)
{
    Elf64_Ehdr *header = elf_header(elf_file);

    int status = elf_validate_loaded(header);
    if (status < 0)
    {
        return status;
    }

    return RES_SUCCESS;
}

uint64_t elf_entry(elf_file_t *file)
{
    return elf_get_entry(elf_header(file));
}

elf_file_t *elf_load(const char *path)
{
    elf_file_t *res = kmalloc(sizeof(elf_file_t));
    if (!res)
    {
        return NULL;
    }

    memset(res, 0, sizeof(elf_file_t));

    res->node = vfs_open(path, OPEN_ACTION_READ);
    if (!res->node)
    {
        elf_free(res);
        return NULL;
    }

    res->file_content = kmalloc(res->node->filesize);
    if (!res->file_content)
    {
        elf_free(res);
        return NULL;
    }

    if (vfs_read(res->node, res->node->filesize, res->file_content) < 0)
    {
        elf_free(res);
        return NULL;
    }

    if (elf_process_load(res) < 0)
    {
        elf_free(res);
        return NULL;
    }

    return res;
}

void elf_free(elf_file_t *file)
{
    if (!file)
    {
        return;
    }

    if (file->file_content)
    {
        kfree(file->file_content);
    }

    if (file->node)
    {
        vfs_close(file->node);
    }

    kfree(file);
}

static int load_phdr(elf_file_t *elf_file, Elf64_Phdr *ph, process_t *proc, uint64_t *data_pages_index, process_t *original)
{
    if (!ph)
    {
        return -RES_INVARG;
    }

    if (ph->p_type != PT_LOAD)
    {
        return 0;
    }

    int status = vfs_seek(elf_file->node, ph->p_offset, SEEK_TYPE_SET);
    if (status < 0)
    {
        return status;
    }

    for (size_t i = 0; i < (ph->p_memsz + (PAGE_SIZE - 1)) / PAGE_SIZE; i++)
    {
        proc->data_pages[*data_pages_index] = pmm_alloc();
        if (!proc->data_pages[*data_pages_index])
        {
            return -RES_NOMEM;
        }

        if (ph->p_memsz > ph->p_filesz)
        {
            if (original)
            {
                memcpy(proc->data_pages[*data_pages_index], original->data_pages[*data_pages_index], PAGE_SIZE);
            }
            else
            {
                memset(proc->data_pages[*data_pages_index], 0, PAGE_SIZE);
            }
        }
        else
        {
            size_t remainder = ph->p_memsz - i * PAGE_SIZE;
            if (remainder > PAGE_SIZE)
            {
                remainder = PAGE_SIZE;
            }

            status = vfs_read(elf_file->node, remainder, proc->data_pages[*data_pages_index]);
            if (status < 0)
            {
                return status;
            }
        }

        int flags = PAGE_PRESENT | PAGE_USER;
        if ((ph->p_flags & PF_W) == PF_W)
        {
            flags |= PAGE_WRITABLE;
        }
        if ((ph->p_flags & PF_X) != PF_X)
        {
            flags |= PAGE_NO_EXECUTE;
        }

        status = pml4_map(proc->pml4, (void *)(ph->p_vaddr + i * PAGE_SIZE), proc->data_pages[*data_pages_index], flags);
        if (status < 0)
        {
            return status;
        }

        *data_pages_index += 1;
    }

    return 0;
}

int elf_load_and_map(process_t *proc, elf_file_t *elf_file)
{
    Elf64_Ehdr *header = elf_header(elf_file);
    Elf64_Phdr *phdrs = elf_pheader(header);

    proc->num_data_pages = 0;
    for (int i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }
        proc->num_data_pages += (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
    }

    proc->data_pages = kmalloc(proc->num_data_pages * sizeof(void *));
    if (!proc->data_pages)
    {
        return -RES_NOMEM;
    }

    uint64_t data_pages_index = 0;
    for (Elf64_Half i = 0; i < header->e_phnum; i++)
    {
        int status = load_phdr(elf_file, &phdrs[i], proc, &data_pages_index, NULL);
        if (status < 0)
        {
            return status;
        }
    }

    return 0;
}

int elf_load_and_map_copy(process_t *proc, elf_file_t *elf_file, process_t *original) // copys the data sections (useful for forking
{
    Elf64_Ehdr *header = elf_header(elf_file);
    Elf64_Phdr *phdrs = elf_pheader(header);

    proc->num_data_pages = 0;
    for (int i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }
        proc->num_data_pages += (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
    }

    proc->data_pages = kmalloc(proc->num_data_pages * sizeof(void *));
    if (!proc->data_pages)
    {
        return -RES_NOMEM;
    }

    uint64_t data_pages_index = 0;
    for (Elf64_Half i = 0; i < header->e_phnum; i++)
    {
        int status = load_phdr(elf_file, &phdrs[i], proc, &data_pages_index, original);
        if (status < 0)
        {
            return status;
        }
    }

    return 0;
}
