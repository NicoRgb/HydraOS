#include <kernel/vmm.h>
#include <kernel/string.h>

page_table_t *current_page_table = NULL;

static inline void flush_tlb(void *addr)
{
    __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

int pml4_map(page_table_t *pml4, void *virt, void *phys, uint64_t flags)
{
    if ((uintptr_t)virt % PAGE_SIZE != 0 || (uintptr_t)phys % PAGE_SIZE != 0)
    {
        return -RES_INVARG;
    }

    uint64_t virt_addr = (uint64_t)virt;
    uint64_t phys_addr = (uint64_t)phys;

    uint16_t pml4_index = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_index = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_index = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_index = (virt_addr >> 12) & 0x1FF;

    uint64_t entry = pml4->entries[pml4_index];
    page_table_t *pdpt = (page_table_t *)(entry & ~0xFFF);
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        pdpt = (page_table_t *)pmm_alloc();
        if (!pdpt)
        {
            return -RES_NOMEM;
        }
        memset(pdpt, 0, PAGE_SIZE);
        pml4->entries[pml4_index] = (uint64_t)pdpt | (PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE);
    }

    entry = pdpt->entries[pdpt_index];
    page_table_t *pd = (page_table_t *)(entry & ~0xFFF);
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        pd = (page_table_t *)pmm_alloc();
        if (!pd)
        {
            return -RES_NOMEM;
        }
        memset(pd, 0, PAGE_SIZE);
        pdpt->entries[pdpt_index] = (uint64_t)pd | (PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE);
    }

    entry = pd->entries[pd_index];
    page_table_t *pt = (page_table_t *)(entry & ~0xFFF);
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        pt = (page_table_t *)pmm_alloc();
        if (!pt)
        {
            return -RES_NOMEM;
        }
        memset(pt, 0, PAGE_SIZE);
        pd->entries[pd_index] = (uint64_t)pt | (PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE);
    }

    pt->entries[pt_index] = phys_addr | flags;

    if (current_page_table == pml4)
    {
        flush_tlb(virt);
    }

    return 0;
}

int pml4_map_range(page_table_t *pml4, void *virt, void *phys, size_t num, uint64_t flags)
{
    if ((uintptr_t)virt % PAGE_SIZE != 0 || (uintptr_t)phys % PAGE_SIZE != 0)
    {
        return -RES_INVARG;
    }

    for (uintptr_t i = 0; i < num; i++)
    {
        int status = pml4_map(pml4, (void *)((uintptr_t)virt + i * PAGE_SIZE), (void *)((uintptr_t)phys + i * PAGE_SIZE), flags);
        if (status < 0)
        {
            return status;
        }
    }

    return 0;
}

uint64_t pml4_get_phys(page_table_t *pml4, void *virt, bool user)
{
    uint64_t virt_addr = (uint64_t)virt;

    uint16_t pml4_index = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_index = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_index = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_index = (virt_addr >> 12) & 0x1FF;

    uint64_t entry = pml4->entries[pml4_index];
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        return 0;
    }

    page_table_t *pdpt = (page_table_t *)(entry & ~0xFFF);
    entry = pdpt->entries[pdpt_index];
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        return 0;
    }

    page_table_t *pd = (page_table_t *)(entry & ~0xFFF);
    entry = pd->entries[pd_index];
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        return 0;
    }

    page_table_t *pt = (page_table_t *)(entry & ~0xFFF);
    entry = pt->entries[pt_index];
    if ((entry & PAGE_PRESENT) != PAGE_PRESENT)
    {
        return 0;
    }
    if ((entry & PAGE_USER) != PAGE_USER && user)
    {
        return 0;
    }

    uint64_t phys_addr = (entry & ~0xFFF) | (virt_addr & 0xFFF);

    return phys_addr;
}

int pml4_switch(page_table_t *pml4)
{
    current_page_table = pml4;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4));

    return 0;
}

void *page_align_address_lower(void *addr)
{
    uintptr_t _addr = (uintptr_t)addr;
    _addr -= _addr % PAGE_SIZE;
    return (void *)_addr;
}

void *page_align_address_higer(void *addr)
{
    uintptr_t _addr = (uintptr_t)addr;
    _addr += PAGE_SIZE - (_addr % PAGE_SIZE);
    return (void *)_addr;
}
