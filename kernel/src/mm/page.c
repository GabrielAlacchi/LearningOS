#include <mm.h>
#include <mm/boot_mmap.h>
#include <mm/page.h>
#include <cpu/atomic.h>
#include <utility/bootinfo.h>
#include <utility/strings.h>


extern phys_addr_t __KERNEL_RODATA_START;
extern phys_addr_t __KERNEL_RODATA_END;
extern phys_addr_t __KERNEL_PHYSICAL_START;
extern phys_addr_t __KERNEL_PHYSICAL_END;


void init_global_page_map() {
    size_t mem_start = 0, mem_end;
    
    const struct multiboot_tag_mmap *mmap = boot_tag_by_type(MULTIBOOT_TAG_TYPE_MMAP);
    size_t num_entries = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;

    for (u8_t i = 0; i < num_entries; ++i) {
        if (mmap->entries[i].type == E820_USABLE_RAM) {
            mem_end = mmap->entries[i].addr + mmap->entries[i].len;
        }
    }

    size_t physmem_pages = (mem_end - mem_start) >> PAGE_ORDER;
    size_t gpm_size = physmem_pages * sizeof(page_info_t);

    size_t start_of_global_map = (size_t)&__KERNEL_PHYSICAL_END;
    start_of_global_map = (start_of_global_map + _Alignof(page_info_t) - 1) & ~(_Alignof(page_info_t) - 1);

    global_page_map = KPHYS_ADDR(start_of_global_map);
    memset(global_page_map, 0, gpm_size);

    global_page_map_end = global_page_map + physmem_pages;

    // The first page of the system is unusable
    global_page_map[0].flags = PAGE_UNUSABLE;

    for (u8_t i = 0; i < num_entries; ++i) {
        const struct multiboot_mmap_entry *entry = &mmap->entries[i];
    
        if (entry->type != E820_USABLE_RAM && entry->addr < mem_end) {
            size_t end_addr = MAX(0x100000, entry->addr);

            const size_t num_pages = (end_addr - entry->addr) >> PAGE_ORDER;
            const size_t start_idx = entry->addr >> PAGE_ORDER;

            for (size_t offset = 0; offset < num_pages; ++offset) {
                global_page_map[start_idx + offset].flags = PAGE_UNUSABLE;
            }
        }
    }
    
    const size_t kernel_start_idx = (size_t)&__KERNEL_PHYSICAL_START >> PAGE_ORDER;
    const size_t gpm_end_idx = (start_of_global_map + gpm_size + PAGE_SIZE - 1) >> PAGE_ORDER;

    for (size_t idx = kernel_start_idx; idx <= gpm_end_idx; ++idx) {
        global_page_map[idx].flags |= PAGE_KERNEL;
        global_page_map[idx].flags |= PAGE_UNUSABLE;

        const size_t page_addr = idx << PAGE_ORDER;
        if ((size_t)&__KERNEL_RODATA_START < page_addr && (size_t)&__KERNEL_RODATA_END > page_addr) {
            global_page_map[idx].flags |= PAGE_READONLY;
        }
    }

    last_kernel_page = gpm_end_idx << PAGE_ORDER;
}

void set_page_flags_atomic(page_info_t *page, const u16_t flags) {
    u16_t current_flags = page->flags;

    while (1) {
        u16_t returned_flags = cmpxchgw(&page->flags, current_flags, current_flags | flags);
        if (returned_flags == current_flags) {
            return;
        }

        // If C&S failed try again with the returned value.
        current_flags = returned_flags;
    }
}

void unset_page_flags_atomic(page_info_t *page, const u16_t flags) {
    u16_t current_flags = page->flags;

    while (1) {
        u16_t returned_flags = cmpxchgw(&page->flags, current_flags, current_flags & (~flags));
        if (returned_flags == current_flags) {
            return;
        }

        // If C&S failed try again with the returned value.
        current_flags = returned_flags;
    }
}

u16_t reference_page(page_info_t *page) {
    return counter16_increment_atomic(&page->refcount, 1);
}

u16_t drop_page_reference(page_info_t *page) {
    u16_t new_ref_count = counter16_increment_atomic(&page->refcount, -1);

    if (new_ref_count == 0 && page->flags & PAGE_BUDDY) {
        struct __buddy_alloc_info *block_base_alloc_info = &page->buddy_alloc_info.block_base->buddy_alloc_info;

        // TODO free this block with the buddy allocator if the result is 0
        counter16_increment_atomic(&block_base_alloc_info->free_count, -1);
    }

    return new_ref_count;
}
