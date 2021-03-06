#include <mm/slab.h>
#include <mm/kmalloc.h>

#define NUM_CACHE_SIZES (sizeof(kmalloc_sizes) / sizeof(u16_t))
#define NUM_SLABS_RESERVED 3

// Maps from all possible values of (alloc_size + 7) / 8 to a cache to use for that alloc size.
// This is much faster than looping over all cache sizes on every allocation and this fits snugly into a page
// and thus can be cached by HW quite easily.
u8_t __cache_idx_map[MAX_KMALLOC_SIZE / 8];
kmem_cache_t __caches[NUM_CACHE_SIZES];

void kmalloc_init() {
    for (u16_t i = 0; i < NUM_CACHE_SIZES; ++i) {
        kmem_cache_t *cache = &__caches[i];
        slab_cache_init(cache, kmalloc_sizes[i], 8, i);
        slab_cache_reserve(cache, cache->objs_per_slab * NUM_SLABS_RESERVED);
    }

    u16_t prev_size_eights = 0;

    for (u16_t i = 0; i < NUM_CACHE_SIZES; ++i) {
        u16_t size_in_eights = kmalloc_sizes[i] / 8;
        
        for (u16_t j = prev_size_eights + 1; j <= size_in_eights; ++j) {
            // Map any allocation with (size / 8) between the previous size in eights and the current size in eights to
            // the i'th cache.
            __cache_idx_map[j] = i;
        }

        prev_size_eights = size_in_eights;
    }
}

void *kmalloc(u16_t size) {
    if (size > MAX_KMALLOC_SIZE) {
        return NULL;
    }

    u8_t cache = __cache_idx_map[(size + 7) >> 3];
    return slab_alloc(__caches + cache);
}

void kfree(void *ptr) {
    u16_t cache_idx = cache_id_for_alloc(ptr);

    if (cache_idx < NUM_CACHE_SIZES) {
        slab_free(__caches + cache_idx, ptr);
    }
}
