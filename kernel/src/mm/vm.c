#include <utility/math.h>
#include <utility/strings.h>
#include <mm/vm.h>

#define PAGE_ADDRESS_MASK ((MASK_FOR_FIRST_N_BITS(40)) << 12)

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

static inline void __map_phys_page(page_table_t *pt, size_t offset, phys_addr_t phys_addr, u8_t flags)
{
    pt_entry_t entry = PT_PRESENT;

    if (flags & VM_ALLOW_WRITE)
    {
        entry |= PT_WRITABLE;
    }

    if (flags & VM_ALLOW_USER)
    {
        entry |= PT_USER_ACCESSIBLE;
    }

    if (!(flags & VM_ALLOW_EXEC))
    {
        entry |= PT_NO_EXECUTE;
    }

    entry |= (phys_addr & PAGE_ADDRESS_MASK);

    pt->entries[offset] = entry;
}

// Follow the offsets of virt_addr to a provided depth. Depth=1 yields page directory pointer. Depth=2, yields the page
// directory table, and depth=3 yields the page table. Along the traversal it will check the status against the given flags
// in case there are any errors.
static page_table_t *__traverse_with_status(
    page_table_t *pml4t,
    virt_addr_t virt_addr,
    u8_t flags,
    u8_t *depth,
    int *error
 ) {
    page_table_t *current_table = pml4t;
    u8_t current_shift = 12 + 27;

    while (*depth <= 3 && *depth > 0)
    {
        size_t current_offset = ((size_t)virt_addr >> current_shift) & 0x1FF;
        pt_entry_t entry = current_table->entries[current_offset];
        
        *error = vm_check_status(entry, flags);
        if (*error) {
            return current_table;
        }

        current_table = (page_table_t *)kphys_addr_for_entry(entry);
        current_shift -= 9;

        --(*depth);
    }

    return current_table;
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

int vm_pt_init(page_table_t *pt, virt_addr_t path_offset, u8_t flags) {
    page_table_t *pml4t = (page_table_t *)KPHYS_ADDR(read_cr3());

    int err;
    u8_t depth = 3;
    page_table_t *parent_table = __traverse_with_status(pml4t, path_offset, flags, &depth, &err);

    size_t offset;

    if (err == 0) {
        return ERR_VM_ALREADY_MAPPED;
    }

    switch (depth) {
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
