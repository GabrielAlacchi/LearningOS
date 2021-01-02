#include <driver/vga.h>
#include <mm/phys_alloc.h>
#include <mm/buddy_alloc.h>
#include <mm/boot_mmap.h>
#include <mm/vm.h>
#include <utility/strings.h>


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

    // We need 2 extra pages to create page tables for the virtual memory mapping.
    phys_addr_t freelist_phys = reserve_physmem_region(pool_size_vector.freelist_pool_pages + 2);

    // We'll use the first page for the PDT which will be attached to the PDPT which is at PML4T[511]
    // which the bootloader initializes when creating the KERNEL VMA map to the final 2GB of virtual memory.
    page_table_t *pdt = KPHYS_ADDR(freelist_phys);

    // The next page will be our page table for the freelist virtual pool.
    page_table_t *pt = pdt + 1;

    vm_pt_init(pdt, (virt_addr_t)FREELIST_POOL_VM_BASE, VM_ALLOW_WRITE);
    vm_pt_init(pt, (virt_addr_t)FREELIST_POOL_VM_BASE, VM_ALLOW_WRITE);

    vm_map_pages(
        freelist_phys + (2 << PAGE_ORDER),
        pool_size_vector.freelist_pool_pages,
        (virt_addr_t)FREELIST_POOL_VM_BASE,
        VM_ALLOW_WRITE | VM_WRITE_GUARD);

    // 2 pages after the pdt is the start of the free list pool pages.
    pool.freelist_pool = (void *)FREELIST_POOL_VM_BASE;
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

phys_addr_t phys_alloc(u8_t order) {
    return buddy_alloc_block(_allocator, order);
}

void phys_free(phys_addr_t block_addr, u8_t order) {
    buddy_free_block(_allocator, block_addr, order);
}
