#ifndef __MM_BUDDY_H
#define __MM_BUDDY_H

#include <mm.h>
#include <mm/bitmap.h>
#include <mm/buddy.h>
#include <mm/buddy_alloc.h>

#include <utility/math.h>

typedef struct __buddy_freelist_node {
    struct __buddy_freelist_node *next, *prev;
    phys_addr_t page_addr;
} buddy_freelist_t;

typedef struct {
    // Maps bits corresponding to a pair of buddies. Each pair has a single bit.
    // The bit is 1 if only one of the buddies is allocated, and 0 if both are allocated or free.
    bitmap_t buddy_state_map;

    // freelists are maintained using the global_page_map.
    buddy_freelist_t *free_blocks[MAX_ORDER + 1];

    // Number of free blocks at each order (tracked for statistics purposes)
    u32_t free_blocks_count[MAX_ORDER + 1];

    phys_addr_t base_addr;
    phys_addr_t end_addr;

    size_t free_pages;
    size_t allocated_pages;
} buddy_t;


void buddy_allocator_init(buddy_t *allocator, void *bitmap_base, phys_addr_t base, phys_addr_t end);

phys_addr_t buddy_alloc_pages(buddy_t *allocator, u8_t order);

void buddy_free_pages(buddy_t *allocator, phys_addr_t pages);


#endif
