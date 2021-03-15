#ifndef __MM_PAGE_H
#define __MM_PAGE_H

#include <types.h>
#include <mm/slab.h>

#define PAGE_UNUSABLE 1
#define PAGE_KERNEL   (1 << 1)
#define PAGE_READONLY (1 << 2)
#define PAGE_BUDDY    (1 << 3)
#define PAGE_FREELIST (1 << 4)

// Every Physical Page in the System has a corresponding page info structure
// managed by the kernel. This is used for reference counting and allocation tracking.
// It is used to track allocations which are backed by the buddy allocator (or direct region reservation allocations).
typedef struct __page {
    u16_t flags;
    u16_t refcount;

    union {
        // Slab header if this page is a slab page.
        slab_header_t slab_header;

        // Used for keeping tabs on blocks that have been allocated by a buddy allocator.
        struct __buddy_alloc_info {
            // Pointer to the base page of the block.
            struct __page *block_base;
            // A larger block may have one of its pages freed, but the whole block isnt ready to be freed unless
            // free_count = 0 in the parent block. This is ignored for non-base blocks.
            u16_t free_count;
        } buddy_alloc_info;

        // Used for keeping tabs on blocks that are allocated in the general region (0x1000 -> 4MB) and between the end of the kernel
        // and the beginning of the highmem buddy allocator. This is just a simple freelist allocator that can return one page at a time.
        struct __freelist_info {
            struct __page *next_free;
        } freelist_info;
    };
} page_info_t;

// Beginning and last (exclusive) elements of the global page map.
page_info_t *global_page_map, *global_page_map_end;
phys_addr_t last_kernel_page;

void init_global_page_map();

static inline page_info_t *page_info(const phys_addr_t addr) {
    return &global_page_map[addr >> PAGE_ORDER];
}

static inline phys_addr_t page_address_from_info(const page_info_t *page) {
    return (page - global_page_map) << PAGE_ORDER;
}

// Update the page flags by ORing the current flags with the provided flags atomically.
void set_page_flags_atomic(page_info_t *page, const u16_t flags);

// Update the page flags by ANDing with the inverse of the provided mask atomically.
void unset_page_flags_atomic(page_info_t *page, const u16_t flags);

// Increment the reference counter for a page.
u16_t reference_page(page_info_t *page);

// Decrement the reference counter for a page.
u16_t drop_page_reference(page_info_t *page);

#endif
