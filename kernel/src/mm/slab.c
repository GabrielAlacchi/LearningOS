#include <mm.h>
#include <mm/slab.h>
#include <mm/vm.h>
#include <utility/math.h>


// Remove a slab from its doubly linked list
static inline void __unlink_slab(slab_t *slab, slab_t **list_head_ref) {
    if (slab->header.prev != NULL) {
        slab->header.prev->header.next = slab->header.next;
    } else {
        // This slab was the head so mutate the list head to update it.
        *list_head_ref = slab->header.next;
    }

    if (slab->header.next != NULL) {
        slab->header.next->header.prev = slab->header.prev;
    }
}

// Add a slab to the head of a doubly linked list
static inline void __link_slab(slab_t *slab, slab_t **list_head) {
    slab->header.next = *list_head;
    slab->header.prev = NULL;

    if (*list_head != NULL) {
        (*list_head)->header.prev = slab;
    }

    *list_head = slab;
}


void slab_cache_init(kmem_cache_t *cache, u16_t obj_size, u16_t obj_align, u16_t cache_id, u16_t vmzone) {
    cache->obj_size = obj_size;
    cache->obj_cell_size = (obj_size + obj_align - 1) & ~(obj_align - 1);
    cache->allocated_objects = 0;
    cache->align_padding = cache->obj_cell_size - cache->obj_size;
    cache->total_free_slabs = 0;
    cache->total_free_objects = 0;
    cache->total_full_slabs = 0;
    cache->total_partial_slabs = 0;
    cache->cache_id = cache_id;
    cache->vmzone = vmzone;

    cache->objs_per_slab = (sizeof(slab_t) - sizeof(slab_header_t)) / cache->obj_cell_size;
    cache->slab_overhead = sizeof(slab_t) - (cache->objs_per_slab * cache->obj_cell_size);

    cache->free_slabs = NULL;
    cache->full_slabs = NULL;
    cache->partial_slabs = NULL;
}

static void __slab_init(const kmem_cache_t *const cache, slab_t *slab) {
    // At initialization, the first free object will be at cell 0
    slab->header.first_free_idx = 0;
    slab->header.free_count = cache->objs_per_slab;
    slab->header.cache_id = cache->cache_id;

    slab_object_t *obj;

    for (u16_t i = 0; i < cache->objs_per_slab - 1; ++i) {
        obj = slab->__slab_data + i * cache->obj_cell_size;
        obj->header.if_free.next_free = (void*)obj + cache->obj_cell_size;
        obj = obj->header.if_free.next_free;
    }

    obj->header.if_free.next_free = NULL;
}

// Create a new slab of memory given some pages.
static inline slab_t *__slab_create(const kmem_cache_t *const cache) {    
    slab_t *new_slab = vm_alloc_block(VM_ALLOW_WRITE, VMZONE_KERNEL_SLAB);
    __slab_init(cache, new_slab);
    return new_slab;
}

// Reserve enough slabs up front for the provided number of objects.
void slab_cache_reserve(kmem_cache_t *cache, u16_t num_objects) {
    u16_t num_slabs = (num_objects + cache->objs_per_slab - 1) / cache->objs_per_slab;

    for (u16_t i = 0; i < num_slabs; ++i) {
        // Allocate a new slab of memory
        slab_t *new_slab = __slab_create(cache);
        __link_slab(new_slab, &cache->free_slabs);
        cache->total_free_objects += cache->objs_per_slab;
    }

    cache->total_free_slabs += num_slabs;
}

void slab_cache_prealloc(kmem_cache_t *cache, void *pages, u8_t num_slabs) {
    for (u8_t i = 0; i < num_slabs; ++i) {
        slab_t *slab = ((slab_t *)pages) + i;
        __slab_init(cache, slab);
        __link_slab(slab, &cache->free_slabs);
        cache->total_free_objects += cache->objs_per_slab;
    }
}

// Allocate an object.
void *slab_alloc(kmem_cache_t *cache) {
    slab_t *slab = cache->partial_slabs;
    
    if (slab == NULL) {
        slab = cache->free_slabs;

        if (slab == NULL) {
            slab = cache->partial_slabs = __slab_create(cache);
            cache->total_partial_slabs += 1;
        } else {
            // Remove the slab from the free_slabs
            __unlink_slab(slab, &cache->free_slabs);
            __link_slab(slab, &cache->partial_slabs);

            cache->total_free_slabs -= 1;
            cache->total_partial_slabs += 1;
        }

        slab->header.prev = NULL;
        slab->header.next = NULL;
    }

    slab_object_t *obj = slab->__slab_data + slab->header.first_free_idx * cache->obj_cell_size;
    slab->header.first_free_idx = ((u8_t*)obj->header.if_free.next_free - slab->__slab_data) / cache->obj_cell_size;
    slab->header.free_count -= 1;

    if (slab->header.free_count == 0) {
        // Slab is full add it to full slabs
        __unlink_slab(slab, &cache->partial_slabs);
        __link_slab(slab, &cache->full_slabs);

        cache->total_partial_slabs -= 1;
        cache->total_full_slabs += 1;
    }

    ++cache->allocated_objects;
    --cache->total_free_objects;
    return obj;
}

// Free an object.
void *slab_free(kmem_cache_t *cache, void *ptr) {
    // Find the slab by rounding down
    slab_t *slab = (slab_t *)aligndown(ptr, SLAB_ORDER + 12);

    // Free the object by settings its next free pointer to the next free object, and then setting
    // the next free in the header of the slab to its index.
    ((slab_object_t*)ptr)->header.if_free.next_free = slab->header.free_count == 0 ? 
        NULL :
        // Add first_free_idx * the cell size to the base of the object memory.
        slab->__slab_data + slab->header.first_free_idx * cache->obj_cell_size;
    slab->header.first_free_idx = ((u8_t*)ptr - slab->__slab_data) / cache->obj_cell_size;

    if (!(slab->header.free_count++)) {
        // The slab was full then we freed some objects, so now we must move the object to the partial list.
        __unlink_slab(slab, &cache->full_slabs);
        __link_slab(slab, &cache->partial_slabs);

        cache->total_full_slabs -= 1;
        cache->total_partial_slabs += 1;
    } else if (slab->header.free_count == cache->objs_per_slab) {
        // The slab is now fully empty, we can move it to the free list.
        __unlink_slab(slab, &cache->partial_slabs);
        __link_slab(slab, &cache->free_slabs);

        cache->total_full_slabs -= 1;
        cache->total_partial_slabs += 1;
    }

    cache->allocated_objects -= 1;
    cache->total_free_objects += 1;
}
