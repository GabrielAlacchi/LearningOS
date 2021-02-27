#ifndef _MM_BOOT_MAP_H
#define _MM_BOOT_MAP_H

#include <types.h>
#include <mm.h>
#include <multiboot2.h>

#include <driver/vga.h>

#define E820_USABLE_RAM 1
#define E820_RESERVED 2
#define E820_ACPI_RECLAIMABLE 3
#define E820_ACPI_NVS 4
#define E820_BAD_MEM 5

/*  Loaded using the BIOS int 0x15 routine in the bootloader.
    Region types:
        Type 1: Usable (normal) RAM
        Type 2: Reserved - unusable
        Type 3: ACPI reclaimable memory
        Type 4: ACPI NVS memory
        Type 5: Area containing bad memory

    Refer to https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_EAX_.3D_0xE820
    for more details
*/

typedef struct __physmem_region {
    phys_addr_t free_start;
    phys_addr_t region_end;
    struct __physmem_region *next_region;
} physmem_region_t;

void print_boot_mmap();
phys_addr_t get_kernel_load_limit();

// Determine the available regions that can be pre-allocated for 
// certain drivers/systems which need fixed size pieces of physical 
// memory. The remainder will go to the buddy allocator for virtual memory allocation.

physmem_region_t *load_physmem_regions();
phys_addr_t reserve_physmem_region(u64_t num_pages);
int is_block_usable(phys_addr_t block_start, size_t num_bytes);

#endif
