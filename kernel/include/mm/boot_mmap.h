#ifndef _MM_BOOT_MAP_H
#define _MM_BOOT_MAP_H

#include <types.h>

// The location was set in the bootloader, see bootloader/boot.asm
// We've identity mapped the first 2MB of memory for the kernel so it should
// be accessible in kernel space.
#define BOOT_MMAP_LOCATION 0x6000

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
typedef struct __attribute__((packed)) {
    u64_t base_address;
    u64_t region_length;
    u32_t region_type;
    u32_t acpi_reserved;
} mm_boot_mmap_entry;

const mm_boot_mmap_entry *get_boot_mmap();
const u64_t boot_mmap_length();

#endif
