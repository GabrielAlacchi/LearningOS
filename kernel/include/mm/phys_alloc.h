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

phys_addr_t phys_alloc(u8_t order);
void phys_free(phys_addr_t block_addr, u8_t order);

#endif
