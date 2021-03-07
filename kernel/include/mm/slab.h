#ifndef __MM_SLAB_H
#define __MM_SLAB_H

#include <types.h>
#include <mm.h>
#include <utility/math.h>


#define SLAB_ORDER 1
#define OBJS_PER_SLAB(type) ((sizeof(slab_t) - sizeof(slab_header_t)) / sizeof(type))

typedef struct __slab_object {
    union {
        struct {
            struct __slab_object *next_free;
        } if_free;
        struct {
            u8_t __first_data_bytes[8];
        } if_used;
    } header;

    u8_t _data[0];
} slab_object_t;


typedef struct {
    // The next slab
    struct __slab *prev, *next;
    
    // The first free object
    u16_t first_free_idx;
    
    // Number of free objects
    u16_t free_count;

    // A user supplied Cache ID which can be used by a higher level allocator like kmalloc which uses 
    // multiple caches to correlate back to the correct cache. The SLAB subsystem ignores this value and just copies it
    // wherever.
    u16_t cache_id;

    // Since most allocated objects need to be 8 byte aligned anyways we can reserve 32 bits for future purposes at the end of the struct
    // to have it be a multiple of 8. 
    u16_t reserved;
} slab_header_t;


typedef struct __slab {
    slab_header_t header;
    u8_t __slab_data[(1 << (12 + SLAB_ORDER)) - sizeof(slab_header_t)];
} slab_t;


typedef struct __kmem_slab_cache {
    u16_t align_padding;
    u16_t obj_size;
    u16_t obj_cell_size; // obj_size + align_padding
    u16_t total_free_slabs;
    u16_t total_partial_slabs;
    u16_t total_full_slabs;
    u32_t allocated_objects;
    u32_t total_free_objects;
    u16_t objs_per_slab;
    u16_t cache_id;
    u16_t vmzone;

    // How many bytes is wasted for header info + extra padding per slab (not including internal padding for alignment).
    u16_t slab_overhead;

    slab_t *free_slabs;
    slab_t *partial_slabs;
    slab_t *full_slabs;
} kmem_cache_t;

// Initialize an object cache
void slab_cache_init(kmem_cache_t *cache, u16_t obj_size, u16_t obj_align, u16_t cache_id, u16_t vmzone);

// Reserve enough slabs up front for the provided number of objects.
void slab_cache_reserve(kmem_cache_t *cache, u16_t num_objects);

// Given preallocated memory, initialize @num_slabs slab(s).
void slab_cache_prealloc(kmem_cache_t *cache, void *pages, u8_t num_slabs);

// Allocate an object.
void *slab_alloc(kmem_cache_t *cache);

// Free an object.
void *slab_free(kmem_cache_t *cache, void *ptr);

static inline u16_t cache_id_for_alloc(void *ptr) {
    slab_t *slab = aligndown(ptr, SLAB_ORDER + PAGE_ORDER);
    return slab->header.cache_id;
}

#endif
