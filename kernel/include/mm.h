#ifndef _MM_ADDR_TYPES_H
#define _MM_ADDR_TYPES_H

#include <types.h>

typedef void *virt_addr_t;
typedef size_t phys_addr_t;

#define PAGE_SIZE 0x1000

#define KERNEL_PML4T KPHYS_ADDR(0x1000)

// I will never use 0x0 in any legitimate way so it can be used as an error / NULL address.
#define NULL 0x0

// The first 2GB of physical memory is mapped into the high 2GB of the address space
// like in Linux. This macro DRYs up the conversion from an address to this physical
// address for kernel functions.
#define KPHYS_ADDR(addr) (void*)((u64_t)0xFFFFFFFF80000000 + (u64_t)(addr))
#define ENTRY_ADDR_MASK ~(((u64_t)0xFFF << 52) + ((u64_t)0xFFF))

#define PT_PRESENT             1
#define PT_WRITABLE            1 << 1
#define PT_USER_ACCESSIBLE     1 << 2
#define PT_WRITE_THROUGH       1 << 3
#define PT_DISABLE_CACHE       1 << 4
#define PT_ACCESSED            1 << 5
#define PT_DIRTY               1 << 6
#define PT_HUGEPAGE            1 << 7
#define PT_GLOBAL              1 << 8
#define PT_NO_EXECUTE          1 << 63

typedef u64_t pt_entry_t;

typedef struct __attribute__((packed)) {
    pt_entry_t entries[512];
} page_table_t;

// Maps the first 2GB of physical addresses to a virtual address
virt_addr_t phys_to_kvirt(phys_addr_t phys_addr);
phys_addr_t virt_to_phys(virt_addr_t virt_addr);

static inline virt_addr_t kphys_addr_for_entry(pt_entry_t entry) {
    // Mask out bits 12-51
    return KPHYS_ADDR((entry & ENTRY_ADDR_MASK));
}

static inline phys_addr_t phys_addr_for_entry(pt_entry_t entry) {
    return (phys_addr_t)(entry & ENTRY_ADDR_MASK);
}

#endif
