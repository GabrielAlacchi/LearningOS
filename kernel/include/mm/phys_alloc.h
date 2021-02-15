#ifndef __PHYS_ALLOC_H
#define __PHYS_ALLOC_H

#include <mm/boot_mmap.h>
#include <mm/buddy_alloc.h>

// We use entry 511 of the PML4T and the first branch of the PDPT.
// This address space is reserved for the allocator's internal virtual structures.
#define FREELIST_POOL_VM_BASE 0xFFFFFF8000000000

buddy_allocator_t *get_allocator();

// Initializes the physical page allocator
void phys_alloc_init();

// Allocate a physical block of size num_pages. 
// Returns NULL if num_pages > 2^MAX_ORDER. The caller
// should remember the size of the allocation for freeing the memory later on.
phys_addr_t phys_alloc(u8_t num_pages);

// Free a physical block of size num_pages.
void phys_free(phys_addr_t block_addr, u8_t num_pages);

// Shrinks a physical block to target_size.
void phys_block_shrink(phys_addr_t block_addr, u8_t block_size, u8_t target_size);

#endif
