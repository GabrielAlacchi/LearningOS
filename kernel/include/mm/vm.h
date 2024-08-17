#ifndef __MM_VM_H
#define __MM_VM_H

#include <mm.h>
#include <mm/page.h>
#include <mm/vmzone.h>

// Mapping flags
#define VM_ALLOW_WRITE 1
#define VM_ALLOW_EXEC 1 << 1
// allow user mode access
#define VM_ALLOW_USER 1 << 2

// Allocate a huge page
#define VM_HUGEPAGE 1 << 7

// Error codes
#define ERR_VM_UNMAPPED       0x1
#define ERR_VM_PRIVILEGE      0x2
#define ERR_VM_BOUNDARY       0x3
#define ERR_VM_ALREADY_MAPPED 0x4
#define ERR_VM_CONTIGUOUS     0x5

#define VM_REGION_HEAP  0xFFFFFF0000000000
#define VM_REGION_PT    0xFFFFFF8000000000

// Defines a zone of virtual memory which is used to allocate fixed size blocks of virtual address space
// using a freelist. The underlying physical memory needs to come from a separate source.
// This is used to allocate page tables.
typedef struct {
    // Should be aligned to a PDPT entry (1GB).
    virt_addr_t zone_base;
    // Also should be 1GB aligned.
    virt_addr_t zone_end;

    page_table_t *first_free_pt;
    virt_addr_t first_free_block;

    virt_addr_t last_free_block;
    page_table_t *last_free_pt;

    virt_addr_t next_empty_pt;

    // The page directory pointer table which is shared by all kernel address spaces.
    page_table_t *shared_pdpt;
    
    // When free blocks = 1 we should expand the zone
    size_t free_blocks;
    u16_t block_size;
} vmblock_alloc_t;

static inline phys_addr_t read_cr3() {
    phys_addr_t val;
    asm volatile("movq %%cr3,%0"
                 : "=r"(val));
    return val;
}

static inline void write_cr3(phys_addr_t pml4t) {
    asm volatile("movq %0,%%cr3" : : "r" (pml4t));
}

static inline void flush_tlb(virt_addr_t page)
{
    asm volatile("invlpg (%0)" ::"r"(page)
                 : "memory");
}

static inline page_table_t *pt_from_entry(pt_entry_t entry) {
    size_t page_phys_addr = entry & ENTRY_ADDR_MASK;
    page_info_t *page = page_info(page_phys_addr);
    return page->pt_virt;
}

static inline page_info_t *page_for_entry(pt_entry_t entry) {
    size_t page_idx = (entry & ENTRY_ADDR_MASK) >> PAGE_ORDER;
    return global_page_map + page_idx;
}

void vm_init();

// Map the given pages of physical memory to the given virtual address with page protection flags.
int vm_map_pages(page_table_t *pml4t, phys_addr_t phys_base, size_t pages, virt_addr_t virt_base, u8_t flags);
virt_addr_t vm_block_claim(vmblock_alloc_t *allocator, const phys_addr_t block_address, u8_t flags);
void vm_block_release(vmblock_alloc_t *allocator, virt_addr_t block_address);

// Functions that are private within the subsystem of virtual memory management.
phys_addr_t _alloc_page_tables(u8_t num_pages, u8_t flags);

// Checks error conditions.
// 1. If the page is not present it returns ERR_VM_UNMAPPED.
// 2. If the page is present but there is a privilege mismatch between the request and the pt_entry_t
// then it returns ERR_VM_PRIVILEGE
int vm_check_status(pt_entry_t entry, u8_t flags);
int vm_pt_init(page_table_t *pt, virt_addr_t path_offset, u8_t flags);
int vm_space_pt_init(page_table_t *pml4t, page_table_t *pt, virt_addr_t path_offset, u8_t flags, u8_t max_depth);
pt_entry_t vm_pt_entry_create(phys_addr_t phys_addr, u8_t flags);

// === Allocation API ===

// Extend a contiguous virtual memory allocation in a zone.
// (Used for allocating heaps and other contiguous regions)
virt_addr_t vmzone_extend(u8_t pages, u8_t flags, u16_t vmzone);

// Shrink is the opposite of extend, it will free the provided number of pages
// from the contiguous zone
int vmzone_shrink(u8_t pages, u16_t vmzone);

void _prepare_block_pt(const vmzone_t *zone, virt_addr_t base_pt_addr, page_table_t *pt);

// Allocate a block in a VMZFLAG_ALLOC_BLOCK virtual memory zone.
virt_addr_t vm_alloc_block(u8_t flags, u16_t vmzone);

// Free a block in a VMZFLAG_ALLOC_BLOCK virtual memory zone.
int vm_free_block(virt_addr_t addr, u16_t vmzone);

// Directly map pages to a given base address so that they are virtually contiguous.
// If pages is not a power of 2 then they are not necessarily physically contiguous.
// This is great for setting up user space processes which are linked in any which way.
// For example, we know we need to map N pages for the ".text" section at some address
// specified in the ELF file, this lets us do just that.
virt_addr_t vmalloc_direct(u8_t pages, u8_t flags, virt_addr_t base_address);

#endif
