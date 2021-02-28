#include <mm.h>
#include <mm/boot_mmap.h>
#include <driver/vga.h>

#include <utility/bootinfo.h>
#include <utility/math.h>
#include <utility/strings.h>

#define PHYSMEM_REGION_POOL_SIZE


// Defined in kernel.ld linker script.
extern phys_addr_t __KERNEL_PHYSICAL_START;
extern phys_addr_t __KERNEL_PHYSICAL_END;

// TODO: Use SLAB allocation for this as well.
physmem_region_t region_pool[12];

static inline u32_t __num_mmap_entries(const struct multiboot_tag_mmap *mmap_tag) {
    return (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size;
}

void print_boot_mmap() {
    const struct multiboot_tag_mmap *mmap_tag = boot_tag_by_type(MULTIBOOT_TAG_TYPE_MMAP);

    char buf[20];

    kprintln("Base Address   | Region Length    | Region Type");

    const u32_t num_entries = __num_mmap_entries(mmap_tag);

    for (const struct multiboot_mmap_entry *entry = mmap_tag->entries; entry - mmap_tag->entries < num_entries; ++entry) {
        ptr_to_hex(entry->addr, buf);
        kputs(buf);

        kputs(" | ");

        itoa((s64_t)entry->len, buf, 10);
        ljust(buf, 16, ' ');

        kputs(buf);

        kputs(" | ");

        switch (entry->type) {
        case E820_USABLE_RAM:
            kprintln("Free Space");
            break;
        case E820_RESERVED:
            kprintln("Reserved");
            break;
        case E820_ACPI_RECLAIMABLE:
            kprintln("ACPI Reclaimable");
            break;
        case E820_ACPI_NVS:
            kprintln("ACPI NVS Memory");
            break;
        case E820_BAD_MEM:
            kprintln("Bad Memory");
            break;
        default:
            kprintln("Unknown Type");
            break;
        }
    }
}

static inline phys_addr_t clamp_up_to_page(phys_addr_t addr) {
    return (addr + PAGE_SIZE - 1) & (~MASK_FOR_FIRST_N_BITS(PAGE_ORDER));
}

static inline phys_addr_t clamp_down_to_page(phys_addr_t addr) {
    return (phys_addr_t)((u64_t)addr & ~0xFFF);
}

physmem_region_t *load_physmem_regions() {
    static physmem_region_t *regions = NULL;

    if (regions != NULL) {
        return regions;
    }

    // The first region will be loaded
    phys_addr_t kernel_start = (phys_addr_t)&__KERNEL_PHYSICAL_START;
    phys_addr_t kernel_end = (phys_addr_t)&__KERNEL_PHYSICAL_END;

    kernel_start = clamp_up_to_page(kernel_start);

    const struct multiboot_tag_mmap *mmap_tag = boot_tag_by_type(MULTIBOOT_TAG_TYPE_MMAP);

    // Start creating a linked list of regions after the base
    physmem_region_t *current_region = region_pool;
    memset(current_region, 0, sizeof(physmem_region_t));

    const u32_t num_entries = __num_mmap_entries(mmap_tag);

    // Create a regions linked list after the base
    regions = current_region;

    for (const struct multiboot_mmap_entry *entry = mmap_tag->entries; entry - mmap_tag->entries < num_entries; ++entry) {
        if (entry->type == E820_USABLE_RAM) {
            // Avoid the first page of memory.
            const phys_addr_t region_base = MAX(entry->addr, PAGE_SIZE);
            const phys_addr_t region_end = entry->addr + entry->len;

            if (region_base >= kernel_start && region_base <= kernel_end) {
                current_region->free_start = clamp_up_to_page(kernel_start);
            } else {
                current_region->free_start = region_base;
            }

            current_region->region_end = region_end;
            current_region->next_region = current_region + 1;
            current_region = current_region->next_region;

            memset(current_region, 0, sizeof(physmem_region_t));
        }
    }

    // We've allocated an extra region that we don't need, let's zero it out
    physmem_region_t *last_region = current_region - 1;
    last_region->next_region = NULL;

    return regions;
}

phys_addr_t reserve_physmem_region(u64_t num_pages) {
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

        return start;
    }

    return NULL;
}

const int is_block_usable(phys_addr_t block_base, size_t num_bytes) {
    const physmem_region_t *region = load_physmem_regions();
    phys_addr_t block_end = block_base + num_bytes;

    // Check if the block is fully contained in any free physmem region
    while (region) {
        if (region->free_start <= block_base && block_end <= region->region_end) {
            return 1;
        }
        
        region = region->next_region;
    }

    return 0;
}