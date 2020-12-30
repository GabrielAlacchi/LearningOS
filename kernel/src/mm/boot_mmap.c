#include <mm.h>
#include <mm/boot_mmap.h>
#include <driver/vga.h>
#include <utility/math.h>
#include <utility/strings.h>


physmem_region_t *regions = NULL;

const inline mm_boot_mmap_entry *get_boot_mmap() {
    return (const mm_boot_mmap_entry *)BOOT_MMAP_LOCATION;
}

const u64_t boot_mmap_length() {
    static u64_t cached_length = 0;

    if (cached_length > 0) {
        return cached_length;
    }

    const mm_boot_mmap_entry *base = get_boot_mmap();
    const mm_boot_mmap_entry *cursor = base;

    while (cursor->region_type != 0) {
        ++cursor;
    }

    return cursor - base;
}

void print_boot_mmap() {
    const mm_boot_mmap_entry *base = get_boot_mmap();
    const u64_t size = boot_mmap_length();

    char buf[20];

    println("Base Address   | Region Length    | Region Type");

    for (const mm_boot_mmap_entry *entry = base; entry - base < size; ++entry) {
        ptr_to_hex(entry->base_address, buf);
        puts(buf);

        puts(" | ");

        itoa((s64_t)entry->region_length, buf, 10);
        ljust(buf, 16, ' ');

        puts(buf);

        puts(" | ");

        switch (entry->region_type) {
        case E820_USABLE_RAM:
            println("Free Space");
            break;
        case E820_RESERVED:
            println("Reserved");
            break;
        case E820_ACPI_RECLAIMABLE:
            println("ACPI Reclaimable");
            break;
        case E820_ACPI_NVS:
            println("ACPI NVS Memory");
            break;
        case E820_BAD_MEM:
            println("Bad Memory");
            break;
        default:
            println("Unknown Type");
            break;
        }
    }
}

const phys_addr_t get_kernel_load_limit() {
    const u16_t* kernel_memspec = (u16_t *)KERNEL_MEM_LIMIT_LOCATION;
    return (phys_addr_t)((u64_t)*kernel_memspec);
}

phys_addr_t clamp_up_to_page(phys_addr_t addr) {
    if ((u64_t)addr & 0xFFF) {
        return addr + 0x1000 - ((u64_t)addr & 0xFFF);
    }

    return addr;
}

phys_addr_t clamp_down_to_page(phys_addr_t addr) {
    return (phys_addr_t)((u64_t)addr & ~0xFFF);
}

physmem_region_t *load_physmem_regions() {
    if (regions != NULL) {
        return regions;
    }

    // The first region will be loaded
    phys_addr_t kernel_start = get_kernel_load_limit();
    kernel_start = clamp_up_to_page(kernel_start);

    const mm_boot_mmap_entry *base = get_boot_mmap();
    const u64_t size = boot_mmap_length();

    // Start creating a linked list of regions after the base
    physmem_region_t *current_region = (physmem_region_t *)(base + size);
    memset(regions, 0, sizeof(physmem_region_t));

    // Create a regions linked list after the base
    regions = current_region;

    for (const mm_boot_mmap_entry *entry = base; entry - base < size; ++entry) {
        if (entry->region_type == E820_USABLE_RAM) {
            // If the region ends after the kernel starts
            if (entry->base_address + entry->region_length > kernel_start) {
                current_region->free_start = clamp_up_to_page(MAX(entry->base_address, kernel_start));
                current_region->region_end = clamp_down_to_page(entry->base_address + entry->region_length);

                current_region->next_region = current_region + 1;
                current_region = current_region->next_region;

                memset(current_region, 0, sizeof(physmem_region_t));
            }
        }
    }

    // We've allocated an extra region that we don't need, let's zero it out
    physmem_region_t *last_region = current_region - 1;
    last_region->next_region = NULL;

    return regions;
}

virt_addr_t reserve_physmem_region(u64_t num_pages) {
    u64_t reservation_size = num_pages * PAGE_SIZE;

    physmem_region_t *region = load_physmem_regions();

    // Greedily check to see if the region has enough free space.
    // We're not doing any best fit since allocations will most likely be predictable
    // in size.
    while (region && region->region_end - region->free_start <= reservation_size) {
        region = region->next_region;
    }

    if (region != NULL) {
        phys_addr_t start = region->free_start;
        region->free_start += reservation_size;

        return KPHYS_ADDR(start);
    }

    return NULL;
}
