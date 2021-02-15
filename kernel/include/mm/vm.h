#ifndef _MM_VM_H
#define _MM_VM_H

#include <mm.h>
#include <mm/vmzone.h>

// Mapping flags
#define VM_ALLOW_WRITE 1
#define VM_ALLOW_EXEC 1 << 1
// allow user mode access
#define VM_ALLOW_USER 1 << 2
// Include a guard page at the end of the mapping
#define VM_WRITE_GUARD 1 << 3

// Allocate physical memory with reserve_physmem_region
#define VM_ALLOC_EARLY 1 << 4

// Allocate a huge page
#define VM_HUGEPAGE 1 << 7

// Error codes
#define ERR_VM_UNMAPPED       0x1
#define ERR_VM_PRIVILEGE      0x2
#define ERR_VM_BOUNDARY       0x3
#define ERR_VM_ALREADY_MAPPED 0x4
#define ERR_VM_CONTIGUOUS     0x5

static inline phys_addr_t read_cr3()
{
    phys_addr_t val;
    asm volatile("mov %%cr3,%0\n\t"
                 : "=r"(val));
    return val;
}

static inline void flush_tlb(virt_addr_t page)
{
    asm volatile("invlpg (%0)" ::"r"(page)
                 : "memory");
}

// Functions that are private within the subsystem of virtual memory management.
phys_addr_t _alloc_page_tables(u8_t num_pages, u8_t flags);

// Checks error conditions.
// 1. If the page is not present it returns ERR_VM_UNMAPPED.
// 2. If the page is present but there is a privilege mismatch between the request and the pt_entry_t
// then it returns ERR_VM_PRIVILEGE
int vm_check_status(pt_entry_t entry, u8_t flags);
int vm_map_pages(phys_addr_t phys_base, u16_t pages, virt_addr_t virt_base, u8_t flags);
int vm_pt_init(page_table_t *pt, virt_addr_t path_offset, u8_t flags);
int vm_space_pt_init(page_table_t *pml4t, page_table_t *pt, virt_addr_t path_offset, u8_t flags, u8_t max_depth);
pt_entry_t vm_pt_entry_create(phys_addr_t phys_addr, u8_t flags);

void vm_init();

// === Allocation API ===

// Extend a contiguous virtual memory allocation in a zone.
// (Used for allocating heaps and other contiguous regions)
virt_addr_t vmzone_extend(u8_t pages, u8_t flags, u16_t vmzone);

// Shrink is the opposite of extend, it will free the provided number of pages
// from the contiguous zone
int vmzone_shrink(u8_t pages, u16_t vmzone);

// Allocate a fixed number of pages in a zone.
// Useful for one off allocations of a given size like creating a new stack for
// a kernel process.
virt_addr_t vmalloc(u8_t pages, u8_t flags, u16_t vmzone);

// Free the block of virtual memory starting from addr
int vm_free(virt_addr_t addr);

// Directly map pages to a given base address so that they are virtually contiguous.
// If pages is not a power of 2 then they are not necessarily physically contiguous.
// This is great for setting up user space processes which are linked in any which way.
// For example, we know we need to map N pages for the ".text" section at some address
// specified in the ELF file, this lets us do just that.
virt_addr_t vmalloc_direct(u8_t pages, u8_t flags, virt_addr_t base_address);

#endif
