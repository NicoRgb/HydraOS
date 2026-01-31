#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// credit: https://jvernay.fr/en/blog/buddy-allocator/implementation/

static inline uint8_t ilog2_floor(uint64_t x)
{
    assert(x > 0);
    return 63 - __builtin_clzll(x);
}

static inline uint8_t ilog2_ceil(uint64_t x)
{
    assert(x > 0);
    uint8_t l = ilog2_floor(x);
    return ((1ULL << l) == x) ? l : (l + 1);
}


typedef struct
{
    uint64_t offset;
    uint64_t size;
} buddy_t;

typedef struct
{
    uint8_t allocated : 1;
    uint8_t in_freelist : 1;
    uint8_t freelist_index : 6;
} subrange_info_t;

typedef struct
{
    uint16_t head_index;
    uint16_t tail_index;
} freelist_t;

typedef struct
{
    uint16_t prev_index;
    uint16_t next_index;
} freelist_link_t;

typedef struct
{
    uint64_t min_alloc_size;
    uint64_t max_alloc_size;
    uint64_t total_size;
} buddy_allocator_params_t;

typedef struct _buddy_allocator
{
    buddy_allocator_params_t params;

    uint8_t min_alloc_log2;
    uint8_t max_alloc_log2;

    uint16_t num_subranges;
    subrange_info_t *subrange_infos;

    uint8_t num_freelists;
    freelist_t *freelists;
    freelist_link_t *links;
} buddy_allocator_t;

static void buddy_add_freelist(buddy_allocator_t *b, uint16_t subrange_idx, subrange_info_t *subrange_info)
{
	freelist_link_t* link = &b->links[subrange_idx];
	freelist_t* freelist = &b->freelists[subrange_info->freelist_index];

	assert(link->prev_index == UINT16_MAX);
	assert(link->next_index == UINT16_MAX);

	uint16_t old_head = freelist->head_index;
	freelist->head_index = subrange_idx;
	if (old_head == UINT16_MAX)
    {
		assert(freelist->tail_index == UINT16_MAX);
		freelist->tail_index = subrange_idx;
	}
	else
    {
		link->next_index = old_head;
		b->links[old_head].prev_index = subrange_idx;
	}
}

static void buddy_remove_freelist(buddy_allocator_t *b, uint16_t subrange_idx, subrange_info_t *subrange_info)
{
	freelist_link_t* link = &b->links[subrange_idx];
	freelist_t* freelist = &b->freelists[subrange_info->freelist_index];

	if (link->prev_index == UINT16_MAX)
    {
		assert(freelist->head_index == subrange_idx);
		freelist->head_index = link->next_index;
	}
	else
    {
		b->links[link->prev_index].next_index = link->next_index;
	}

	if (link->next_index == UINT16_MAX)
    {
		assert(freelist->tail_index == subrange_idx);
		freelist->tail_index = link->prev_index;
	}
	else
    {
		b->links[link->next_index].prev_index = link->prev_index;
	}

	link->prev_index = UINT16_MAX;
	link->next_index = UINT16_MAX;
}

static void validate_parameters(buddy_allocator_params_t params)
{
    assert((params.min_alloc_size & (params.min_alloc_size - 1)) == 0);
    assert((params.max_alloc_size & (params.max_alloc_size - 1)) == 0);
    assert(params.max_alloc_size >= params.min_alloc_size);
    assert((params.total_size % params.max_alloc_size) == 0);
}

uint64_t get_buddy_allocator_metadata_size(buddy_allocator_params_t params)
{
    validate_parameters(params);

    uint64_t subranges_size = (params.total_size / params.min_alloc_size) * sizeof(subrange_info_t);
    uint64_t links_size = (params.total_size / params.min_alloc_size) * sizeof(freelist_link_t);
    uint64_t freelists_size = (1 + ilog2_ceil(params.max_alloc_size / params.min_alloc_size)) * sizeof(freelist_t);

    return subranges_size + links_size + freelists_size + sizeof(buddy_allocator_t);
}

buddy_allocator_t *buddy_allocator_init(buddy_allocator_params_t params, void *metadata)
{
    validate_parameters(params);

    buddy_allocator_t *b = (buddy_allocator_t *)metadata;

    b->num_subranges = params.total_size / params.min_alloc_size;
    b->subrange_infos = (subrange_info_t *)((uintptr_t)metadata + sizeof(buddy_allocator_t));

    b->num_freelists = 1 + ilog2_ceil(params.max_alloc_size / params.min_alloc_size);
    b->freelists = (freelist_t *)((uintptr_t)metadata + sizeof(buddy_allocator_t) + b->num_subranges * sizeof(subrange_info_t));
    b->links = (freelist_link_t *)((uintptr_t)metadata + sizeof(buddy_allocator_t) + b->num_subranges * sizeof(subrange_info_t) + b->num_freelists * sizeof(freelist_t));

    memset(b->subrange_infos, 0x00, b->num_subranges * sizeof(subrange_info_t));
    memset(b->freelists, 0xFF, b->num_freelists * sizeof(freelist_t));
    memset(b->links, 0xFF, b->num_subranges * sizeof(freelist_link_t));

    b->min_alloc_log2 = ilog2_ceil(params.min_alloc_size);
    b->max_alloc_log2 = ilog2_ceil(params.max_alloc_size);

    uint16_t index_step = (params.max_alloc_size / params.min_alloc_size);
    uint16_t max_index = b->num_subranges - index_step;
    for (uint16_t index = 0; index <= max_index; index += index_step)
    {
        b->links[index].prev_index = (index == 0 ? UINT16_MAX : index - index_step);
        b->links[index].next_index = (index == max_index) ? UINT16_MAX : index + index_step;

        b->subrange_infos[index].freelist_index = b->num_freelists - 1;
        b->subrange_infos[index].in_freelist = true;
    }

    b->freelists[b->num_freelists - 1].head_index = 0;
    b->freelists[b->num_freelists - 1].tail_index = max_index;

    return b;
}

buddy_t buddy_alloc(buddy_allocator_t* b, uint64_t size)
{
    uint8_t size_log2 = ilog2_ceil(size);
    if (size_log2 > b->max_alloc_log2)
    {
        return (buddy_t) { 0, 0 };
    }
    if (size_log2 < b->min_alloc_log2)
    {
        size_log2 = b->min_alloc_log2;
    }

    uint8_t desired_freelist = size_log2 - b->min_alloc_log2;

    uint8_t freelist = desired_freelist;
    uint16_t subrange = UINT16_MAX;
    for (; freelist < b->num_freelists; freelist += 1)
    {
        subrange = b->freelists[freelist].head_index;
        if (subrange != UINT16_MAX)
        {
            break;
        }
    }
    if (subrange == UINT16_MAX)
    {
        // out of memory
        return (buddy_t) { 0, 0 };
    }

    subrange_info_t* subrange_info = &b->subrange_infos[subrange];
    assert(subrange_info->allocated == false);
    assert(subrange_info->in_freelist == true);

    subrange_info->in_freelist = false;
    buddy_remove_freelist(b, subrange, subrange_info);

    while (freelist > desired_freelist)
    {
        freelist -= 1;
        uint16_t buddy = subrange ^ (1u << freelist);

        subrange_info_t* buddy_info = &b->subrange_infos[buddy];

        assert(buddy_info->allocated == false);
        assert(buddy_info->in_freelist == false);

        buddy_info->in_freelist = true;
        buddy_info->freelist_index = freelist;
        buddy_add_freelist(b, buddy, buddy_info);
    }

    subrange_info->allocated = true;
    subrange_info->freelist_index = freelist;

    buddy_t res;
    res.offset = (uint64_t)subrange << b->min_alloc_log2;
    res.size = (uint64_t)1 << (b->min_alloc_log2 + freelist);
    return res;
}

uint64_t buddy_free(buddy_allocator_t* b, uint64_t offset)
{
    assert((offset & ((1ULL << b->min_alloc_log2) - 1)) == 0);

    uint16_t subrange = (uint16_t)(offset >> b->min_alloc_log2);
    subrange_info_t* info = &b->subrange_infos[subrange];
    uint8_t freelist = info->freelist_index;
    uint8_t old_freelist = freelist;

    assert(info->allocated == true);
    assert(info->in_freelist == false);

    info->allocated = false;

    for (; freelist < b->num_freelists - 1; freelist += 1)
    {
        uint16_t buddy = subrange ^ (1u << freelist);

        subrange_info_t* buddy_info = &b->subrange_infos[buddy];

        if (!buddy_info->in_freelist || buddy_info->freelist_index < freelist)
        {
            break;
        }
        
        assert(buddy_info->allocated == false);
        assert(buddy_info->freelist_index == freelist);

        buddy_info->in_freelist = false;
        buddy_remove_freelist(b, buddy, buddy_info);

        subrange &= UINT16_MAX << (freelist + 1);
    }

    info = &b->subrange_infos[subrange];
    assert(info->allocated == false);
    
    info->in_freelist = true;
    info->freelist_index = freelist;
    buddy_add_freelist(b, subrange, info);

    return (uint64_t)1 << (b->min_alloc_log2 + old_freelist);
}

uint64_t buddy_get_size(buddy_allocator_t* b, uint64_t offset)
{
    assert((offset & ((1ULL << b->min_alloc_log2) - 1)) == 0);

    uint16_t subrange = (uint16_t)(offset >> b->min_alloc_log2);
    subrange_info_t* info = &b->subrange_infos[subrange];
    uint8_t freelist = info->freelist_index;

    return (uint64_t)1 << (b->min_alloc_log2 + freelist);
}
