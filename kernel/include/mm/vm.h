#ifndef _MM_VM_H
#define _MM_VM_H

#include <mm.h>

// Mapping flags
#define VM_ALLOW_WRITE 0x1
#define VM_ALLOW_EXEC  0x2
// allow user mode access
#define VM_ALLOW_USER  0x4
// Include a guard page at the end of the mapping
#define VM_WRITE_GUARD 0x8

// Error codes
#define ERR_VM_UNMAPPED       0x1
#define ERR_VM_PRIVILEGE      0x2
#define ERR_VM_BOUNDARY       0x3
#define ERR_VM_ALREADY_MAPPED 0x4


static inline phys_addr_t read_cr3() {
    phys_addr_t val;
    asm volatile("mov %%cr3,%0\n\t" : "=r" (val));
    return val;
}

static inline void flush_tlb(virt_addr_t page){
    asm volatile("invlpg (%0)" :: "r" (page) : "memory");
}

// Checks error conditions.
// 1. If the page is not present it returns ERR_VM_UNMAPPED.
// 2. If the page is present but there is a privilege mismatch between the request and the pt_entry_t
// then it returns ERR_VM_PRIVILEGE
int vm_check_status(pt_entry_t entry, u8_t flags);
int vm_map_pages(phys_addr_t phys_base, u16_t pages, virt_addr_t virt_base, u8_t flags);
int vm_pt_init(page_table_t *pt, virt_addr_t path_offset, u8_t flags);


#endif
