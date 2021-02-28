#ifndef _MM_ADDR_TYPES_H
#define _MM_ADDR_TYPES_H

#include <types.h>

typedef void * virt_addr_t;
typedef size_t phys_addr_t;

#define PAGE_SIZE  0x1000
#define PAGE_ORDER 12

// I will never use 0x0 in any legitimate way so it can be used as an error / NULL address.
#define NULL 0x0

#define KERNEL_VMA 0xFFFFFFFF80000000

// The first 2GB of physical memory is mapped into the high 2GB of the address space
// like in Linux. This macro DRYs up the conversion from an address to this physical
// address for kernel functions.

#ifdef TESTSUITE
extern u8_t __test_physical_mem[];
#define KPHYS_ADDR(addr) (void*)(__test_physical_mem + (u64_t)addr)
#else
#define KPHYS_ADDR(addr) (void*)((u64_t)KERNEL_VMA + (u64_t)(addr))
#endif

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

// Custom Page Table Flags (bits 9-11 and 52-62 are free for use)

// These bits are used when cleaning up a processes' entire virtual address space.
// When allocating physical memory we need to remember the continguous number of pages
// of the allocation. The first entry of any PT will have these bits set to store this metadata 
// PT_PAGE_AHEAD indicates that there is another page table at addr + 0x1000 that was allocated
// as part of the same allocation. PT_PAGE_BEHIND does the same for addr - 0x1000. PT_EARLY_ALLOC
// indicates that this page table was allocated by reserve_physmem_pages and not by the buddy allocator.
#define PT_PAGE_AHEAD          1ul << 9
#define PT_PAGE_BEHIND         1ul << 10
#define PT_EARLY_ALLOC         1ul << 11

// These bits are used similarly to how PT_PAGE_AHEAD and PT_PAGE_BEHIND are used, except they convey information about the allocation
// of the mapped data pages and not the page tables themselves.
#define PT_DATA_PAGE_AHEAD      1ul << 54
#define PT_DATA_PAGE_BEHIND     1ul << 55

// Bit 56 indicates that the physical data pointed to by the current PT entry was allocated by 
// reserve_physmem_pages rather than the buddy allocator
#define PT_DATA_EARLY_ALLOC     1ul << 56

// This bit is set for PML4T entries whenever they are "global" in the sense that all kernel processes
// should share this same entry. They won't get deleted when a process is freed.
#define PT_KERNEL_GLOBAL        1ul << 57

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

// Offset at a particular height in the VM page table tree. Height = 3 is the PML4T,
// Height = 0 is the PT.
static inline size_t height_offset(virt_addr_t addr, u8_t height) {
    return ((size_t)addr >> (12 + height * 9)) & 0x1FF;
}

#endif
