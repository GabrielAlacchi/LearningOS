#ifndef __MM_VMZONE_H
#define __MM_VMZONE_H

/*
VMZONEs are predefined parts of virtual memory that are reserved for use by the kernel for different purposes.
A VM zone is defined by a range of PDPT entries within a given PDPT table. They are shared by all processes,
each PML4T will point to the same space in these zones.
*/

#include <types.h>
#include <mm.h>
#include <mm/vm.h>

// Memory that can remain mapped in user space, it's not sensitive in any way and not susceptible
// to meltdown. In some cases it actually has to be kept in the user space mapping so an interrupt
// can find the correct code. Some kernel sections may even be mapped into this memory range.
#define KERNEL_NORMAL_MEM    0xFFFFFF0000000000

// Memory that needs to be unmapped when going to user space.
#define KERNEL_SENSITIVE_MEM 0xFFFFFF8000000000

// Both KERNEL_NORMAL_MEM and KERNEL_SENSITIVE_MEM regions are shared with all processes

// Zone which can only allocate contiguous blocks (like a HEAP).
// If 1 only vmzone_extend is allowed. if 0 only direct mapping and vmalloc are allowed.
#define VMZFLAG_CONTIGUOUS    1
#define VMZFLAG_ALLOW_EXECUTE 1 << 1

// Has the zone been initialized? Are there page tables for the first 512 pages of the region?
// No physical actually have to be mapped before this flag is set, there just needs to exist a
// tree of page tables covering the first part of the region.
#define VMZFLAG_INITIALIZED   1 << 2

// Zone for heap for small object allocations (kmalloc)
#define VMZONE_KERNEL_HEAP 0x0

// Zone for storing larger kernel allocations
#define VMZONE_KERNEL_ALLOC 0x1

// Zone for storing safe data that needs to be mapped in with user space (even if its not accessible normally).
// Of course with meltdown this data can be read but it's not sensitive.
// Kernel stacks need to be stored here so the TSS points to a valid stack when switching from user to kernel,
// without needing a full CR3 change.
#define VMZONE_USER_SHARED 0x2

// Zone for storing buddy allocator structures
#define VMZONE_BUDDY_MEM 0x3

#define VMZONE_NUMBER_OF_ZONES 4

typedef struct
{
    virt_addr_t start_address;
    virt_addr_t end_address;
    virt_addr_t cursor_addr;
    
    u8_t vm_flags;
    u16_t flags;
} vmzone_t;

void vmzone_init();
vmzone_t *vmzone_info(u16_t vmzone);

// Initializes a new virtual memory space with 3rd level page tables
// defined for a zone, and the 2GB kernel memory mapping.
void vmspace_init(page_table_t *pml4t, u8_t flags);

#endif
