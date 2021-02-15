#include <utility/math.h>
#include <utility/strings.h>
#include <mm/boot_mmap.h>
#include <mm/vm.h>
#include <mm/vmzone.h>
#include <mm/phys_alloc.h>

#define PAGE_ADDRESS_MASK ((MASK_FOR_FIRST_N_BITS(40)) << 12)
#define MASK_UNRESERVED_BITS (~(PAGE_ADDRESS_MASK | (1ul << 63) | MASK_FOR_FIRST_N_BITS(9)))

void vm_init()
{
    vmzone_init();

    page_table_t *pml4t = KPHYS_ADDR(read_cr3());

    vmspace_init(pml4t, VM_ALLOC_EARLY);
}

phys_addr_t _alloc_page_tables(u8_t num_pages, u8_t flags)
{
    phys_addr_t alloc_base;

    if (flags & VM_ALLOC_EARLY)
    {
        alloc_base = reserve_physmem_region(num_pages);
    }
    else
    {
        alloc_base = phys_alloc(num_pages);
    }

    page_table_t *page_tables = KPHYS_ADDR(alloc_base);
    memset(page_tables, 0, num_pages << PAGE_ORDER);

    for (size_t i = 0; i < num_pages; ++i)
    {
        // The first entry stores metadata about the allocation of the actual page table
        pt_entry_t first_entry = 0;

        if (flags & VM_ALLOC_EARLY)
        {
            first_entry |= PT_EARLY_ALLOC;
        }

        if (i > 0)
        {
            // Indicate that there is a page table at phys_addr - 4096 that was allocated together
            // with this one.
            first_entry |= PT_PAGE_BEHIND;
        }

        if (i < num_pages - 1)
        {
            // Indicate that there is a page table at phys_addr + 4096 that was allocated together
            // with this one.
            first_entry |= PT_PAGE_AHEAD;
        }

        page_tables[i].entries[0] = first_entry;
    }

    return alloc_base;
}

int vm_check_status(pt_entry_t entry, u8_t flags)
{
    if (!(entry & PT_PRESENT))
    {
        return ERR_VM_UNMAPPED;
    }

    if ((flags & VM_ALLOW_WRITE) && !(entry & PT_WRITABLE))
    {
        return ERR_VM_PRIVILEGE;
    }

    if ((flags & VM_ALLOW_USER) && !(entry & PT_USER_ACCESSIBLE))
    {
        return ERR_VM_PRIVILEGE;
    }

    return 0;
}

// Check if the requested virtual memory range can be mapped in exactly one pt.
static inline int __check_vm_region_boundary(virt_addr_t virt_base, u16_t pages, u8_t flags)
{
    if (flags & VM_WRITE_GUARD)
    {
        ++pages;
    }

    if (pages > 512)
    {
        // If the number of pages exceeds 512 this condition is guaranteed.
        return ERR_VM_BOUNDARY;
    }

    virt_addr_t virt_end = virt_base + (pages << PAGE_ORDER);

    // Check if all offsets to the left of the page table offset are the same in both the base and end addresses.
    // These offsets comprise of 27 bits and they start at bit (12 + 9).
    size_t offset_mask = 0x8FFFFFFul << (12 + 9);

    if (((size_t)virt_base & offset_mask) == ((size_t)virt_end & offset_mask))
    {
        return 0;
    }

    return ERR_VM_BOUNDARY;
}

pt_entry_t vm_pt_entry_create(phys_addr_t phys_addr, u8_t flags) {
    pt_entry_t entry = PT_PRESENT;

    if (flags & VM_ALLOW_WRITE) {
        entry |= PT_WRITABLE;
    }

    if (flags & VM_ALLOW_USER) {
        entry |= PT_USER_ACCESSIBLE;
    }

    if (!(flags & VM_ALLOW_EXEC)) {
        entry |= PT_NO_EXECUTE;
    }

    if (flags & VM_HUGEPAGE) {
        entry |= PT_HUGEPAGE;
    }

    return entry | (phys_addr & PAGE_ADDRESS_MASK);
}

// Map a page table entry to a physical address with page protection flags
static inline void __map_phys_page(page_table_t *pt, size_t offset, phys_addr_t phys_addr, u8_t flags)
{
    pt_entry_t entry = vm_pt_entry_create(phys_addr, flags);

    // Preserve bits that are not reserved
    pt->entries[offset] &= MASK_UNRESERVED_BITS;
    pt->entries[offset] |= entry;
}

// Follow the offsets of virt_addr to a provided depth. Depth=1 yields page directory pointer. Depth=2, yields the page
// directory table, and depth=3 yields the page table. Along the traversal it will check the status against the given flags
// in case there are any errors.
static page_table_t *__traverse_with_status(
    page_table_t *pml4t,
    virt_addr_t virt_addr,
    u8_t flags,
    u8_t *depth,
    int *error)
{
    page_table_t *current_table = pml4t;
    u8_t current_shift = 12 + 27;

    while (*depth <= 3 && *depth > 0)
    {
        size_t current_offset = ((size_t)virt_addr >> current_shift) & 0x1FF;
        pt_entry_t entry = current_table->entries[current_offset];

        *error = vm_check_status(entry, flags);
        if (*error)
        {
            return current_table;
        }

        current_table = (page_table_t *)kphys_addr_for_entry(entry);
        current_shift -= 9;

        --(*depth);
    }

    return current_table;
}

// Finds the page table for the provided virtual address and allocates any missing pages tables along the way.
static page_table_t *__find_or_allocate_pt(
    page_table_t *pml4t,
    virt_addr_t virt_addr,
    u8_t flags,
    int *error
) 
{
    u8_t height = 3;
    int traversal_error = 0;
    page_table_t *pt_or_parent = __traverse_with_status(pml4t, virt_addr, flags, &height, &traversal_error);

    if (traversal_error == ERR_VM_UNMAPPED) {
        while (height > 0) {
            size_t offset = height_offset(virt_addr, height);
            phys_addr_t new_page_table = _alloc_page_tables(1, flags);
            __map_phys_page(pt_or_parent, offset, new_page_table, flags);

            pt_or_parent = KPHYS_ADDR(new_page_table);

            --height;
        }
    } else if (traversal_error > 0) {
        *error = traversal_error;
        return NULL;
    }

    *error = 0;
    return pt_or_parent;
}

static page_table_t *__find_pt_or_null(
    page_table_t *pml4t,
    virt_addr_t virt_addr
) {
    page_table_t *table = pml4t;

    u8_t current_shift = PAGE_ORDER + 27;
    for (u8_t i = 0; i < 3; ++i) {
        size_t offset = (((size_t)virt_addr) >> current_shift) & 0x1FF;
        pt_entry_t entry = table->entries[offset];

        if (!(entry & PT_PRESENT)) {
            return NULL;
        }

        table = kphys_addr_for_entry(entry);
        current_shift -= 9;
    }

    return table;
}

// This function will assume that the page tables are mapped and exist down to the PT level.
// If not it will return an error code, else it will return 0.
//
// For simplicity sake this mapping will refuse to map a group of pages which do not map to the same
// page table. Thus the max it can map at any given time is 512 pages assuming perfect alignment of the
// base address. To map across many PTs make multiple calls.
//
// If this condition is not met ERR_VM_BOUNDARY will be returned.
int vm_map_pages(phys_addr_t phys_base, u16_t pages, virt_addr_t virt_base, u8_t flags)
{
    if (__check_vm_region_boundary(virt_base, pages, flags))
    {
        return ERR_VM_BOUNDARY;
    }

    page_table_t *pml4t = (page_table_t *)KPHYS_ADDR(read_cr3());

    int err;
    u8_t depth = 3;
    page_table_t *pt = __traverse_with_status(pml4t, virt_base, flags, &depth, &err);

    size_t start_offset = pt_offset(virt_base);

    for (size_t page = 0; page < pages; ++page)
    {
        size_t bytes_offset = page << 12;
        __map_phys_page(pt, start_offset + page, phys_base + bytes_offset, flags);
        flush_tlb(virt_base + bytes_offset);
    }

    if (flags & VM_WRITE_GUARD)
    {
        flags &= ~(VM_ALLOW_WRITE);
        size_t bytes_offset = pages << 12;
        __map_phys_page(pt, start_offset + pages, phys_base + bytes_offset, flags);
        flush_tlb(virt_base + bytes_offset);
    }

    return 0;
}

/* Given an allocated page for a page table (pt) it will initialize a page table along the path provided in the
   path_offset virtual address with the provided page protection flags. */
int vm_pt_init(page_table_t *pt, virt_addr_t path_offset, u8_t flags) {
    page_table_t *pml4t = (page_table_t *)KPHYS_ADDR(read_cr3());
    return vm_space_pt_init(pml4t, pt, path_offset, flags, 3);
}

/* Given a pointer to the 4th level PT, and an allocated page for a page table (pt) it will initialize a 
   page table along the path provided in the path_offset virtual address with the provided page protection flags.
   
   When max_depth is set to 1 the PT must be a page directory pointer table, 2 corresponds to anything before a page directory
   table and 3 corresponds to a page table. */
int vm_space_pt_init(page_table_t *pml4t, page_table_t *pt, virt_addr_t path_offset, u8_t flags, u8_t max_depth) {
    int err;
    u8_t height = 3;
    page_table_t *parent_table = __traverse_with_status(pml4t, path_offset, flags, &height, &err);

    size_t offset;
    // If height is 3 we reached a PDPT (depth 1)
    size_t reached_depth = 4 - height;

    if (err == 0 || reached_depth > max_depth)
    {
        return ERR_VM_ALREADY_MAPPED;
    }

    switch (height)
    {
    case 0:
        return ERR_VM_ALREADY_MAPPED;
    case 1:
        offset = pdt_offset(path_offset);
        break;
    case 2:
        offset = pdpt_offset(path_offset);
        break;
    case 3:
        offset = pml4t_offset(path_offset);
        break;
    }

    phys_addr_t phys_addr_for_pt = phys_addr_for_kphys(pt);
    __map_phys_page(parent_table, offset, phys_addr_for_pt, flags);
    memset(pt, 0, sizeof(page_table_t));

    return 0;
}

phys_addr_t __alloc_phys_block(u8_t num_pages, u8_t flags) {
    if (flags & VM_ALLOC_EARLY) {
        reserve_physmem_region(num_pages);
    } else {
        phys_alloc(num_pages);
    }
}

void __free_phys_block(pt_entry_t base_entry, u8_t num_pages) {
    phys_addr_t block_base = phys_addr_for_entry(base_entry);

    if (base_entry & PT_DATA_EARLY_ALLOC) {
        
    } else {
        phys_free(block_base, num_pages);
    }
}

void __shrink_phys_block(pt_entry_t base_entry, u8_t block_size, u8_t target_size) {
    phys_addr_t block_base = phys_addr_for_entry(base_entry);

    if (base_entry & PT_DATA_EARLY_ALLOC) {

    } else {
        phys_block_shrink(base_entry, block_size, target_size);
    }
}

// Get the value of the flags for the page table which indicate how the mapped page of physical memory was allocated
size_t __data_alloc_flags(size_t index_in_alloc, u8_t alloc_size, u8_t flags) {
    size_t alloc_flags = 0;

    if (flags & VM_ALLOC_EARLY) {
        alloc_flags |= PT_DATA_EARLY_ALLOC;
    }
    
    if (index_in_alloc > 0) {
        alloc_flags |= PT_DATA_PAGE_BEHIND;
    }

    if (index_in_alloc < (alloc_size - 1)) {
        alloc_flags |= PT_DATA_PAGE_AHEAD;
    }

    return alloc_flags;
}

// Extend a contiguous virtual memory allocation in a zone.
// (Used for allocating heaps and other contiguous regions)
virt_addr_t vmzone_extend(u8_t pages, u8_t flags, u16_t vmzone) {
    page_table_t *pml4t = (page_table_t *)KPHYS_ADDR(read_cr3());

    vmzone_t *zone = vmzone_info(vmzone);
    if (!(zone->flags & VMZFLAG_CONTIGUOUS)) {
        return NULL;
    }

    virt_addr_t cursor = zone->cursor_addr;

    int err;
    page_table_t *pt = __find_or_allocate_pt(pml4t, cursor, flags, &err);

    // Allocate a block of the current order
    phys_addr_t phys_block = __alloc_phys_block(pages, flags);
    size_t next_pt_offset = pt_offset(cursor);

    for (size_t i = 0; i < pages; ++i) {
        pt->entries[next_pt_offset] |= __data_alloc_flags(i, pages, flags);
        __map_phys_page(pt, next_pt_offset++, phys_block, flags);

        phys_block += 1ul << PAGE_ORDER;
        cursor += 1ul << PAGE_ORDER;

        if (next_pt_offset >= 512) {
            // We've crossed a pt boundary, allocate another page table.
            pt = __find_or_allocate_pt(pml4t, cursor, flags, &err);
            next_pt_offset = 0;
        }
    }

    virt_addr_t original_cursor = zone->cursor_addr;
    zone->cursor_addr = cursor;

    return original_cursor;
}

int vmzone_shrink(u8_t pages, u16_t vmzone) {
    page_table_t *pml4t = (page_table_t *)KPHYS_ADDR(read_cr3());

    vmzone_t *zone = vmzone_info(vmzone);
    if (!(zone->flags & VMZFLAG_CONTIGUOUS)) {
        return ERR_VM_CONTIGUOUS;
    }

    // Cursor is the start of the first unmapped page, go back by one page.
    virt_addr_t cursor = zone->cursor_addr;
    cursor -= PAGE_SIZE;

    page_table_t *pt = __find_pt_or_null(pml4t, cursor);
    if (pt == NULL) {
        return ERR_VM_UNMAPPED;
    }

    size_t next_offset = pt_offset(cursor);

    u8_t current_block_size = 0;

    // Don't clear the PTs just remove PT_PRESENT
    for (u8_t i = 0; i < pages; ++i) {
        pt->entries[next_offset] &= ~PT_PRESENT;
        current_block_size += 1;
        
        if (!(pt->entries[next_offset] & PT_DATA_PAGE_BEHIND)) {
            __free_phys_block(pt->entries[next_offset], current_block_size);
            current_block_size = 0;
        }

        cursor -= PAGE_SIZE;

        if (next_offset == 0) {
            // We're crossing a page table boundary
            page_table_t *pt = __find_pt_or_null(pml4t, cursor);
            if (pt == NULL) {
                return ERR_VM_UNMAPPED;
            }

            next_offset = 511;
        } else {
            --next_offset;
        }
    }

    // Start of the first unmapped page.
    zone->cursor_addr = cursor + PAGE_SIZE;

    if (current_block_size > 0) {
        // Figure out how big the current block is and shrink it by current_block_size
        u8_t actual_block_size = current_block_size;
        pt_entry_t current_entry;
       
        do {
            actual_block_size += 1;
            current_entry = pt->entries[next_offset];

            cursor -= PAGE_SIZE;

            if (next_offset == 0 && current_entry & PT_DATA_PAGE_BEHIND) {
                // We're crossing a page table boundary
                page_table_t *pt = __find_pt_or_null(pml4t, cursor);
                if (pt == NULL) {
                    return ERR_VM_UNMAPPED;
                }

                next_offset = 511;
            } else {
                --next_offset;
            }
        } while (current_entry & PT_DATA_PAGE_BEHIND);
        
        __shrink_phys_block(current_entry, actual_block_size, actual_block_size - current_block_size);
    }

    return 0;
}