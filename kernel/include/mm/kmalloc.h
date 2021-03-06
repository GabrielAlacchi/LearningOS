#ifndef __MM_KMALLOC_H
#define __MM_KMALLOC_H

#include <mm.h>
#include <mm/slab.h>

#define MAX_KMALLOC_SIZE 2040

// The KMalloc API is used for allocating objects of size up to approximately a half page. Any larger allocations should be serviced
// using a different method.

// These are sizes which have very low fragmentation when used with slab allocation given that the slab header is 24 bytes,
// and these sizes are aligned by 8.
static u16_t kmalloc_sizes[] = { 8, 16, 24, 48, 96, 120, 240, 480, 1016, 2040 };

// Initialize the caches for objects of each malloc size. Called in the mm_init process.
void kmalloc_init();

// KMalloc takes a request of size < max_kmalloc_size and returns a newly allocated object, or NULL if OOM.
void *kmalloc(u16_t size);

void kfree(void *ptr);

#endif
