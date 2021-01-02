#ifndef __BUDDY_ALLOC_H
#define __BUDDY_ALLOC_H

#include <mm.h>
#include <mm/bitmap.h>
#include <types.h>
#include <utility/math.h>

#define MAX_ORDER 7
#define PAGE_OFFSET_OOB 0xFFFFFFFFFFFFFFFF

// Number of bits in the bitmap needed to represent the state of pairs of buddies in a block of
// MAX_ORDER + 1. 2^(N+1) - 1
#define MAX_BLOCK_BITS ((1 << (MAX_ORDER + 1)) - 1)

#define FREELIST_PRESENT 0x1

// Free list nodes will be allocated in a virtually contiguous array of memory.
// Pages are allocated and vm mapped early into the bootstrapping phase of the
// physical allocation subsystem of the kernel. Enough pages are initialized so
// there's more than enough space for free list nodes, but if ever more pages are
// needed they can be mapped.
typedef struct {
    // The next entry is referred to by an index into the array. By convention index 0
    // will be untouchable and will be a null entry. So index 0 refers to a null ptr (as one would expect).
    u32_t next_entry;
    
    // Page offset is the number of pages from base_addr of the allocator that this free list node represents
    u64_t page_offset : 24;
    u8_t flags;
} freelist_entry_t;

typedef struct buddy_allocator {
    // Maps bits corresponding to a pair of buddies. Each pair has a single bit.
    // The bit is 1 if only one of the buddies is allocated, and 0 if both are allocated or free.
    bitmap_t buddy_state_map;

    // We maintain a single free list for all orders
    freelist_entry_t *freelists[MAX_ORDER + 1];

    freelist_entry_t *freelist_pool;

    // Cache a free list entry to avoid searching when a free follows an allocation.
    // When clearing a node from the free list the allocator will set this to point to that now freed
    // entry.
    freelist_entry_t *available_freelist_entry;

    // This index is used when finding an empty freelist node. The allocator will search to the right of
    // this index and eventually wrap around. This allows us to be very fast in the beginning when the pool isnt
    // full by constantly moving right and getting free blocks in a single iteration.
    u32_t freelist_pool_scanner;
    
    // The number of unallocated freelist nodes left in the pool
    u32_t freelist_space_left;

    // Size measured in sizeof(freelist_entry_t)
    size_t freelist_pool_size;

    phys_addr_t base_addr;
    phys_addr_t end_addr;

    size_t free_space_bytes;
    size_t allocated_bytes;
} buddy_allocator_t;

// The size estimate that occurs when preallocing will occur in two stages.
// We'd like to allocate the bitmap and buddy_allocator_t structs separately
// and the freelist pool separately.
typedef struct {
    u16_t bitmap_and_struct_pages;
    u16_t freelist_pool_pages;
} buddy_prealloc_vector;

// Memory pool for internal buddy allocator structures
typedef struct {
    virt_addr_t bitmap_and_struct_pool;
    virt_addr_t freelist_pool;
    size_t freelist_pool_pages;
} buddy_memory_pool;

static inline size_t page_offset_of(phys_addr_t address) {
    return address >> PAGE_ORDER;
}

// Determines the index in the bitmap representing the state of a pair of buddies
// at the provided page offset and order.
// Any address within a block of size order + 1 is acceptable, whether its
// in the primary or secondary buddy of that higher order block or not.
static inline size_t buddy_bmp_index_of(size_t page_offset, u8_t order) {
    size_t max_block_offset = page_offset >> (MAX_ORDER + 1);

    size_t mask_test = MASK_FOR_FIRST_N_BITS((MAX_ORDER + 1));

    // offset of the page within the current block of order (MAX_ORDER + 1).
    // We are keeping bits 0 to MAX_ORDER when masking this way, and discarding the rest.
    size_t max_block_remainder = page_offset & mask_test;

    // What we do is we keep bits order + 1 to MAX_ORDER of the number. Then we also turn on
    // the orderth bit. Doing this provides us with a unique index for any given pair of buddies
    // at a specified order. If the order specified is 0 and the page_offset is 0 then the final
    // index will be 1. We want it 0 indexed so we subtract by 1 afterwards.
    size_t bit_offset = (max_block_remainder & ~MASK_FOR_FIRST_N_BITS(order)) | (1 << order);

    // The offset of our MAX_ORDER + 1 block times the number of bits needed to represent
    // each "super block" gives us the number of bits needed to represent all previous pairs of 
    // buddies in those previous super blocks. Finally we add our offset - 1 to get the final bit
    // index in the bitmap.
    return (max_block_offset * MAX_BLOCK_BITS) + bit_offset - 1; 
}

size_t buddy_bmp_size_bits(size_t num_pages);

buddy_prealloc_vector buddy_estimate_pool_size(size_t num_pages);

buddy_allocator_t *buddy_init(buddy_memory_pool pool, phys_addr_t start_addr, phys_addr_t end_addr);
phys_addr_t buddy_alloc_block(buddy_allocator_t *allocator, u8_t order);
void buddy_free_block(buddy_allocator_t *allocator, phys_addr_t block_base, u8_t order);

#endif
