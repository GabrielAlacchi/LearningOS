#include <driver/vga.h>
#include <mm/phys_alloc.h>
#include <mm/buddy_alloc.h>
#include <mm/boot_mmap.h>
#include <mm/vm.h>
#include <utility/strings.h>


#define FREELIST_EXPANSION_THRESHOLD (MAX_ORDER + 1)


static buddy_allocator_t *_allocator = NULL;

typedef struct {
    phys_addr_t start;
    phys_addr_t end;
} _alloc_bounds_t;

buddy_allocator_t *get_allocator() {
    return _allocator;
}

void __find_allocatable_region(_alloc_bounds_t *bounds) {
    physmem_region_t *region = load_physmem_regions();
    // Get the first region after 1MB
    while (region && region->free_start < 0x100000) {
        region = region->next_region;
    }

    bounds->start = region->free_start;
    
    while (region->next_region != NULL) {
        region = region->next_region;
    }

    bounds->end = region->region_end;
}

buddy_memory_pool __prealloc_buddy_pool(_alloc_bounds_t *bounds) {
    char buf[16];

    buddy_memory_pool pool;

    size_t num_pages = (bounds->end - bounds->start) >> PAGE_ORDER;
    buddy_prealloc_vector pool_size_vector = buddy_estimate_pool_size(num_pages);

    puts("Preallocating ");
    itoa(pool_size_vector.bitmap_and_struct_pages, buf, 10);
    puts(buf);
    println(" pages for the bitmap and structures");

    phys_addr_t bmp_phys = reserve_physmem_region(pool_size_vector.bitmap_and_struct_pages);
    pool.bitmap_and_struct_pool = KPHYS_ADDR(bmp_phys);

    puts("Preallocating ");
    itoa(pool_size_vector.freelist_pool_pages, buf, 10);
    puts(buf);
    println(" pages for the freelist");
    println("Preallocation 2 pages for page tables for the freelist pool");

    // Allocate freelist_pool_pages in the VMZONE_BUDDY_MEM zone to store the free list structures.
    pool.freelist_pool = vmzone_extend(pool_size_vector.freelist_pool_pages, VM_ALLOW_WRITE | VM_ALLOC_EARLY, VMZONE_BUDDY_MEM);
    pool.freelist_pool_pages = pool_size_vector.freelist_pool_pages;

    return pool;
}

void phys_alloc_init() {
    println("Initializing Physical Allocator...");

    _alloc_bounds_t bounds;
    __find_allocatable_region(&bounds);

    buddy_memory_pool pool = __prealloc_buddy_pool(&bounds);

    // We need to recompute the allocatable region since our preallocations changed what's available.
    println("Recomputing allocatable region");
    __find_allocatable_region(&bounds);

    println("Initializing buddy allocator");
    _allocator = buddy_init(pool, bounds.start, bounds.end);
}

static inline void __freespace_check() {
    if (_allocator->freelist_space_left < FREELIST_EXPANSION_THRESHOLD) {
        println("Expanding freelist pool by one page");
        vmzone_extend(1, VM_ALLOW_WRITE | VM_ALLOC_EARLY, VMZONE_BUDDY_MEM);

        buddy_freelist_pool_expand(_allocator, 1);
    }
}

phys_addr_t phys_alloc(u8_t num_pages) {
    if (num_pages > (1 << MAX_ORDER)) {
        return NULL;
    }

    u8_t alloc_order = bit_order(num_pages);

    phys_addr_t block_base = buddy_alloc_block(_allocator, alloc_order);
    buddy_shrink_block(_allocator, block_base, alloc_order, num_pages);
    __freespace_check();

    return block_base;
}

void phys_free(phys_addr_t block_addr, u8_t num_pages) {
    phys_block_shrink(block_addr, num_pages, 0);
}

void phys_block_shrink(phys_addr_t block_addr, u8_t block_size, u8_t target_size) {
    s8_t order = (s8_t)bit_order(block_size);

    for (; order >= 0; --order) {
        if ((block_size >> order) & 1) {
            // The allocation contains a block of this order. We either need to keep it, free it or shrink it depending on target_size
            if (target_size == 0) {
                buddy_free_block(_allocator, block_addr, order);
            } else if (target_size <= (1 << order)) {
                // Shrink the block since our target size is smaller than the block
                buddy_shrink_block(_allocator, block_addr, order, target_size);

                // We've exhausted the target size, by setting it to 0 all subsequent blocks will be freed.
                target_size = 0;
            } else {
                // The target size is larger than the current block. Keep the block in place and subtract its size from the target size
                target_size -= 1 << order;
            }

            block_addr += (1 << (order + PAGE_ORDER));
        }
    }

    __freespace_check();
}
