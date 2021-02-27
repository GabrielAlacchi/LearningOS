#include <utility/bootinfo.h>

extern phys_addr_t multiboot_info_ptr;

const virt_addr_t boot_tag_by_type(u32_t type) {
  unsigned *sz = (unsigned *)KPHYS_ADDR(multiboot_info_ptr);
  struct multiboot_tag *tag = KPHYS_ADDR(multiboot_info_ptr) + 8;

  while (tag->type != type) {
    if (tag->type == MULTIBOOT_TAG_TYPE_END) {
      return NULL;
    }

    tag = (struct multiboot_tag *)(((void*)tag) + ((tag->size + 7) & ~7));
  }

  return tag;
}
