#ifndef __MM_DEFS_H
#define __MM_DEFS_H

#include <types.h>

// I will never use 0x0 in any legitimate way so it can be used as an error / NULL address.
#define NULL 0x0

#define KERNEL_VMA 0xFFFFFFFF80000000
#define PAGE_ORDER    12
#define PAGE_SIZE     4096

typedef void * virt_addr_t;
typedef size_t phys_addr_t;

#endif
