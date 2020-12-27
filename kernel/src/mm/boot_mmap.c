#include <mm/boot_mmap.h>

const inline mm_boot_mmap_entry * get_boot_mmap() {
    return (const mm_boot_mmap_entry *)BOOT_MMAP_LOCATION;
}

const u64_t boot_mmap_length() {
    const mm_boot_mmap_entry *base = get_boot_mmap();
    const mm_boot_mmap_entry *cursor = base;

    while (cursor->region_type != 0) {
        ++cursor;
    }

    return cursor - base;
}
