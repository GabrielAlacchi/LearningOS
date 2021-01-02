#ifndef _MM_ADDR_TYPES_H
#define _MM_ADDR_TYPES_H

#include <types.h>

typedef void *virt_addr_t;
typedef size_t phys_addr_t;

#define PAGE_SIZE  0x1000
#define PAGE_ORDER 12

// I will never use 0x0 in any legitimate way so it can be used as an error / NULL address.
#define NULL 0x0

#define KERNEL_VMA 0xFFFFFFFF80000000

// The first 2GB of physical memory is mapped into the high 2GB of the address space
// like in Linux. This macro DRYs up the conversion from an address to this physical
// address for kernel functions.
#define KPHYS_ADDR(addr) (void*)((u64_t)KERNEL_VMA + (u64_t)(addr))
#define ENTRY_ADDR_MASK ~(((u64_t)0xFFF << 52) + ((u64_t)0xFFF))

#define KERNEL_PML4T KPHYS_ADDR(0x1000)

#define PT_PRESENT             1ul
#define PT_WRITABLE            1ul << 1
#define PT_USER_ACCESSIBLE     1ul << 2
#define PT_WRITE_THROUGH       1ul << 3
#define PT_DISABLE_CACHE       1ul << 4
#define PT_ACCESSED            1ul << 5
#define PT_DIRTY               1ul << 6
#define PT_HUGEPAGE            1ul << 7
#define PT_GLOBAL              1ul << 8
#define PT_NO_EXECUTE          1ul << 63

typedef u64_t pt_entry_t;

typedef struct __attribute__((packed)) {
    pt_entry_t entries[512];
} page_table_t;

// Maps the first 2GB of physical addresses to a virtual address
virt_addr_t phys_to_kvirt(phys_addr_t phys_addr);
phys_addr_t virt_to_phys(virt_addr_t virt_addr);


// Initialize the memory management subsystem. Will initialize
// 1. NXE bit in EFER MSR to allow execution protection on instruction fetches.
// 2. Physical Allocator
void mm_init();


static inline virt_addr_t kphys_addr_for_entry(pt_entry_t entry) {
    // Mask out bits 12-51
    return KPHYS_ADDR((entry & ENTRY_ADDR_MASK));
}

static inline phys_addr_t phys_addr_for_kphys(virt_addr_t kphys_addr) {
    if (kphys_addr < (virt_addr_t)KERNEL_VMA) {
        return NULL;
    }

    return (size_t)kphys_addr - (size_t)KERNEL_VMA;
}

static inline phys_addr_t phys_addr_for_entry(pt_entry_t entry) {
    return (phys_addr_t)(entry & ENTRY_ADDR_MASK);
}

static inline size_t pml4t_offset(virt_addr_t addr) {
    return ((size_t)addr >> (12 + 27)) & 0x1FF;
}

static inline size_t pdpt_offset(virt_addr_t addr) {
    return ((size_t)addr >> (12 + 18)) & 0x1FF;
}

static inline size_t pdt_offset(virt_addr_t addr) {
    return ((size_t)addr >> (12 + 9)) & 0x1FF;
}

static inline size_t pt_offset(virt_addr_t addr) {
    return ((size_t)addr >> 12) & 0x1FF;
}

#endif
