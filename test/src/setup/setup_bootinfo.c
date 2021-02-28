#include <mm.h>
#include <multiboot2.h>
#include <mm/boot_mmap.h>
#include <suite.h>
#include <setup/setup_bootinfo.h>


phys_addr_t multiboot_info_ptr = 0x1000;


void setup_bootinfo() {
  // Create a mock boot info structure
  unsigned *size = KPHYS_ADDR(multiboot_info_ptr);

  struct multiboot_tag_mmap *mmap_tag = ((void*)size) + 8;
  mmap_tag->type = MULTIBOOT_TAG_TYPE_MMAP;
  mmap_tag->entry_size = sizeof(struct multiboot_mmap_entry);
  mmap_tag->size = sizeof(struct multiboot_tag_mmap) + 3 * mmap_tag->entry_size;
  struct multiboot_mmap_entry *mmap_entries = &mmap_tag->entries;
  mmap_entries[0].addr = 0;
  mmap_entries[0].len = MEMHOLE_BEGIN;
  mmap_entries[0].type = E820_USABLE_RAM;
  mmap_entries[1].addr = MEMHOLE_BEGIN;
  mmap_entries[1].len = MEMHOLE_END - MEMHOLE_BEGIN;
  mmap_entries[1].type = E820_RESERVED;
  mmap_entries[2].addr = MEMHOLE_END;
  mmap_entries[2].len = PHYS_MEM_SIZE - MEMHOLE_END;
  mmap_entries[2].type = E820_USABLE_RAM;

  struct multiboot_tag *end_tag = ((void*)mmap_tag) + ((mmap_tag->size + 7) & ~7);
  end_tag->type = MULTIBOOT_TAG_TYPE_END;
  end_tag->size = sizeof(struct multiboot_tag);

  *size = ((void*)end_tag - (void*)mmap_tag) + end_tag->size;
}
