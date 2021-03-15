#ifndef __MM_PAGE_ALLOC_H
#define __MM_PAGE_ALLOC_H

#include <mm.h>

#define PFA_GENERAL 0x0
#define PFA_DMA     0x1

// DMA zone stretches from 4MB to 16MB
#define DMA_ZONE_BEGIN 0x400000
#define DMA_ZONE_END   0x1000000

// Initialize the page frame allocator(s). This function is assumed to be called after init_global_page_map
void pfa_init();

// Allocate and free a single page from the general reserved region.
phys_addr_t pfa_alloc_page();

// Allocate a block of pages from the buddy allocator for either DMA or GENERAL use
phys_addr_t pfa_alloc_block(u8_t order, u8_t flags);

// These functions should not be called externally, use drop_page_reference defined in page.h instead to free a page.
// These functions will be called if refcount = 0 in the drop reference routine.
void _pfa_free_page(phys_addr_t addr);
void _pfa_free_block(phys_addr_t block_base);

#endif
