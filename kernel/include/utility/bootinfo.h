/* Utilities for getting information from multiboot */
#ifndef __BOOT_INFO_H
#define __BOOT_INFO_H

#include <multiboot2.h>
#include <mm.h>

const virt_addr_t boot_tag_by_type(u32_t type);

#endif
