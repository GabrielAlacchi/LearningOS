#include <mm/buddy_alloc.h>
#include <mm/boot_mmap.h>
#include <mm/vmzone.h>

#include <utility/strings.h>
#include <utility/math.h>
#include <mm/slab.h>
#include <mm/bitmap.h>

size_t buddy_bmp_size_bits(size_t num_pages) {
    // Round up the number of MAX_ORDER + 1 blocks there are
    // then multiply by MAX_BLOCK_BITS

    // Divide by 2^(MAX_ORDER + 1) rounding up (how many MAX_ORDER + 1 blocks are there)
    size_t num_max_blocks = round_up_shift_right(num_pages, MAX_ORDER + 1);

    return num_max_blocks * MAX_BLOCK_BITS;
}

static inline size_t __buddy_page_offset(size_t page_offset, u8_t order) {
    size_t alignment_mask = MASK_FOR_FIRST_N_BITS(order + 1);

    if (page_offset & alignment_mask) {
        // If page offset is not aligned with (order + 1) then it is the secondary buddy.
        return page_offset - (1 << order);        
    } else {
        return page_offset + (1 << order);
    }
}

buddy_prealloc_vector buddy_estimate_pool_size(size_t num_pages) {
    size_t bitmap_struct_bytes = buddy_bmp_size_bits(num_pages) >> 3;
    size_t num_max_blocks = round_up_shift_right(num_pages, MAX_ORDER);
    
    // Since the free list memory pool can grow as needed (since it has no physical contiguity requirement)
    // in the allocation model we can allocate a few more pages than needed to save space.
    
    // Add 3 * (MAX_ORDER + 1) free list nodes. This is an arbitrary number however it can help account
    // for memory boundaries which are not allocatable, and thus require smaller blocks to fit
    // the memory at initialization. We want a slight overestimate so we allocate enough memory up front. 
    // If we don't allocate enough memory up front it's more difficult to expand the free list before having
    // a fully initialized allocator.
    size_t freelist_nodes_estimate = num_max_blocks + 3 * (MAX_ORDER + 1);

    buddy_prealloc_vector prealloc;

    prealloc.bitmap_and_struct_pages = round_up_shift_right(bitmap_struct_bytes, PAGE_ORDER);
    size_t per_slab = OBJS_PER_SLAB(freelist_entry_t);

    prealloc.freelist_pool_slabs = (freelist_nodes_estimate + per_slab - 1) / per_slab, PAGE_ORDER;

    return prealloc;
}

static inline void __allocate_freelist_entry(buddy_allocator_t *allocator, size_t page_offset, u8_t order) {
    freelist_entry_t *entry = slab_alloc(&allocator->freelist_cache);
    entry->next_entry = allocator->freelists[order] == NULL ? NULL : allocator->freelists[order] - entry;
    entry->page_offset = page_offset;

    allocator->freelists[order] = entry;
}

void __populate_initial_freelists(buddy_allocator_t *allocator) {
    size_t cursor_page_offset = 0;
    size_t max_page_offset = page_offset_of(allocator->end_addr - allocator->base_addr);

    // We populate the list 1 block of MAX_ORDER at a time, but if we run into memory holes / boundaries
    // we can put smaller blocks along the way.
    while (cursor_page_offset < max_page_offset) {
        size_t inner_page_offset = 0;
        s8_t order = MAX_ORDER;

        // We try to allocate as close to MAX_ORDER as we can. At each order level we'll check if the 
        // block is usable. If not we allocate the largest possible block and advance the inner_page_offset
        // to the boundary of that block's buddy. Mathematically that buddy will not be usable, but we can
        // still try to fit even smaller blocks until order < 0, then stop.
        while (order >= 0 && inner_page_offset < (1 << MAX_ORDER)) {
            size_t curr_offset = inner_page_offset + cursor_page_offset;
            phys_addr_t block_start = allocator->base_addr + ((cursor_page_offset + inner_page_offset) << PAGE_ORDER);

            if (is_block_usable(block_start, 1 << (12 + order))) {
                // If the block is usable allocate a free block here
                __allocate_freelist_entry(allocator, curr_offset, order);

                allocator->free_space_bytes += 1 << (12 + order);

                // Advance the inner page offset by the size of the block.
                inner_page_offset += (1 << order);
            }

            --order;
        }

        cursor_page_offset += (1 << MAX_ORDER);
    }
}

void buddy_init(buddy_allocator_t *allocator, buddy_memory_pool pool, phys_addr_t base_addr, phys_addr_t end_addr) {
    const size_t region_size_pages = (end_addr - base_addr) >> PAGE_ORDER;

    // Use the base of the bitmap and struct pool for the allocator's internal structures.
    u8_t *bitmap_base = (u8_t*)pool.bitmap_and_struct_pool;
    memset(allocator, 0, sizeof(buddy_allocator_t));
    
    bmp_init(&allocator->buddy_state_map, bitmap_base, buddy_bmp_size_bits(region_size_pages));
    slab_cache_init(&allocator->freelist_cache, sizeof(freelist_entry_t), _Alignof(freelist_entry_t), 0, VMZONE_BUDDY_MEM);
    slab_cache_prealloc(&allocator->freelist_cache, pool.freelist_pool, pool.freelist_pool_slabs);
    
    allocator->base_addr = base_addr;
    allocator->end_addr = end_addr;
    
    for (u8_t order = 0; order <= MAX_ORDER; ++order) {
        // The first entry of the freelist pool will be NULL.
        allocator->freelists[order] = NULL;
    }

    __populate_initial_freelists(allocator);
}

size_t __find_or_split_block(buddy_allocator_t *allocator, u8_t target_order) {
    u8_t order = target_order + 1;

    // Find a free block.
    freelist_entry_t *free_block;
    do {
        free_block = allocator->freelists[order];
    } while (free_block == NULL && order++ < MAX_ORDER);

    // We didn't find anything, return PAGE_OFFSET_OOB to indicate this.
    if (order > MAX_ORDER) {
        return PAGE_OFFSET_OOB;
    }

    // Remove this block from the freelist of the corresponding order and flip its bitmap
    // bit (indicating that its been allocated).
    allocator->freelists[order] = free_block->next_entry == NULL ? NULL : free_block + free_block->next_entry;
    size_t bmp_index = buddy_bmp_index_of(free_block->page_offset, order);
    bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);

    // For each order from order - 1 to target_order + 1 (inclusive) we need to create 
    // a free list entry for the buddy of the page_offset of the free_block that we found.
    // These blocks represent the result of splitting the blocks at each level.
    // We also need to toggle the free bit at each of these levels.
    for (order = order - 1; order > target_order; --order) {
        // Page offset of the buddy of the block.
        size_t buddy_offset = free_block->page_offset + (1 << order);
        __allocate_freelist_entry(allocator, buddy_offset, order);

        bmp_index = buddy_bmp_index_of(free_block->page_offset, order);
        bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);
    }

    // The final level needs to be split. Instead of allocating a free block we can just
    // re-use the current one. More specifically if we just return the page_offset of the buddy at
    // target_order, then all that remains is to take the current free_block and move it to the list 
    // of target_order.

    free_block->next_entry = allocator->freelists[target_order] == NULL ? NULL : allocator->freelists[target_order] - free_block;
    allocator->freelists[target_order] = free_block;

    // We also need to flip the buddy state bit for target_order now that we've allocated one of the
    // two buddies.
    bmp_index = buddy_bmp_index_of(free_block->page_offset, target_order);
    bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);

    return free_block->page_offset + (1 << target_order);
}

phys_addr_t buddy_alloc_block(buddy_allocator_t *allocator, u8_t order) {
    freelist_entry_t *free_block = allocator->freelists[order];
    size_t page_offset;
    if (free_block != NULL) {
        page_offset = free_block->page_offset;

        // Remove the block from the free list
        allocator->freelists[order] = free_block->next_entry == NULL ? NULL : free_block + free_block->next_entry;
        
        // Make the block reclaimable and cache it for the next free / allocation
        __release_freelist_entry(allocator, free_block);

        // Toggle the bit for this block and its buddy.
        size_t bmp_index = buddy_bmp_index_of(page_offset, order);
        bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);
    } else {
        page_offset = __find_or_split_block(allocator, order);
    }

    // Memory accounting
    allocator->free_space_bytes -= (1 << (order + PAGE_ORDER));
    allocator->allocated_bytes += (1 << (order + PAGE_ORDER));

    return allocator->base_addr + (page_offset << PAGE_ORDER);
}

freelist_entry_t *__pop_freelist_entry(buddy_allocator_t *allocator, size_t page_offset, u8_t order) {
    freelist_entry_t *entry, *prev;

    prev = NULL;
    entry = allocator->freelists[order];

    while (entry != NULL && entry->page_offset != page_offset) {
        prev = entry;
        entry = entry->next_entry == NULL ? NULL : entry + entry->next_entry;
    }

    if (entry == NULL) {
        return NULL;
    }

    if (entry == allocator->freelists[order]) {
        allocator->freelists[order] = entry->next_entry == NULL ? NULL : entry + entry->next_entry;
    } else {
        prev->next_entry = entry->next_entry == NULL ? NULL : (entry + entry->next_entry) - prev;
    }

    return entry;
}

inline void __release_freelist_entry(buddy_allocator_t *allocator, freelist_entry_t *entry) {
    slab_free(&allocator->freelist_cache, entry);
}

int __can_coalesce(buddy_allocator_t *allocator, size_t coalesced_offset, u8_t order, size_t *bmp_index) {
    *bmp_index = buddy_bmp_index_of(coalesced_offset, order);
    u8_t buddy_pair_state = bmp_get_bit(&allocator->buddy_state_map, *bmp_index);

    size_t buddy_offset = __buddy_page_offset(coalesced_offset, order);
    size_t buddy_addr = allocator->base_addr + (buddy_offset << PAGE_ORDER);

    return buddy_pair_state == 1 && is_block_usable(buddy_addr, 1 << (PAGE_ORDER + order));
}

void buddy_free_block(buddy_allocator_t *allocator, phys_addr_t block_base, u8_t order) {
    size_t page_offset = (block_base - allocator->base_addr) >> PAGE_ORDER;
    size_t order_bytes = 1ul << (PAGE_ORDER + order);

    size_t bmp_index = buddy_bmp_index_of(page_offset, order);
    u8_t buddy_pair_state = bmp_get_bit(&allocator->buddy_state_map, bmp_index);

    size_t buddy_offset = __buddy_page_offset(page_offset, order);

    if (buddy_pair_state == 1 && is_block_usable(block_base + order_bytes, order_bytes)) {
        // We are able to coalesce this block and its buddy. Don't bother creating a freelist entry,
        // we'll find the entry of the buddy and merge with it.

        // This entry will not be removed so we don't decrement
        freelist_entry_t *buddy = __pop_freelist_entry(allocator, buddy_offset, order);
        bmp_set_bit(&allocator->buddy_state_map, bmp_index, 0);
        
        size_t coalesced_offset = MIN(page_offset, buddy_offset);
        size_t coalesced_order = order;

        while (++coalesced_order < MAX_ORDER && __can_coalesce(allocator, coalesced_offset, coalesced_order, &bmp_index)) {
            buddy_offset = __buddy_page_offset(coalesced_offset, coalesced_order);
            
            // Coalesce the block.
            freelist_entry_t *upper_buddy = __pop_freelist_entry(allocator, buddy_offset, coalesced_order);
            __release_freelist_entry(allocator, upper_buddy);

            bmp_set_bit(&allocator->buddy_state_map, bmp_index, 0);

            coalesced_offset = MIN(coalesced_offset, buddy_offset);
        }

        // Finally we've reached the highest possible coalescable order, we just need to move the freelist entry
        // to this order.
        buddy->next_entry = allocator->freelists[coalesced_order] == NULL ? NULL : allocator->freelists[coalesced_order] - buddy;
        buddy->page_offset = coalesced_offset;

        // Add the buddy block to the free list and toggle its bit.
        allocator->freelists[coalesced_order] = buddy;
        bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);
    } else {
        // Create a freelist entry for this block, we can't coalesce with the buddy afterwards.
        // And toggle the bit.
        __allocate_freelist_entry(allocator, page_offset, order);
        bmp_set_bit(&allocator->buddy_state_map, bmp_index, 1);
    }

    // Memory Accounting
    allocator->free_space_bytes += 1ul << (order + PAGE_ORDER);
    allocator->allocated_bytes -= 1ul << (order + PAGE_ORDER);
}

void buddy_shrink_block(buddy_allocator_t *allocator, phys_addr_t block_base, u8_t block_order, u8_t num_pages) {
    // We make no assumptions about the alignment of block_base,
    // get the block offset truncated to be the base of the block.
    size_t block_offset = trunc_n_bits((block_base - allocator->base_addr) >> PAGE_ORDER, block_order);

    u8_t block_size = 1 << block_order;
    u8_t split_order = block_order - 1;

    // This is essentially the unwound version of a recursive implementation.
    // The base case is when the num_pages requested is equal to 2^block_order. This means we have nothing
    // to shrink, so we don't do anything. 

    // Else we then split the block in to two sub blocks, creating a freelist entry if necessary, then recursively
    // shrink either the left or right block as needed.

    // In the "tailcall" the num_pages, split_order and block_offset variables are re-used in the "next frame".
    while (num_pages != (1 << (split_order + 1))) {
        // Bitmap index for the pair of buddies we'll be splitting.
        size_t bmp_index = buddy_bmp_index_of(block_offset, split_order);

        // We need to split the block into two blocks of size (split_order). The question is do we need to create a free list node or not?
        // If num_pages > 2^split_order then both sub blocks are going to be allocated, thus we don't need to create any free list entries,
        // and set the bitmap bit to 0 (since the bit is the XOR of the states), and we go to the right block for the next allocation.
        if (num_pages > (1 << split_order)) {
            block_offset += (1 << split_order);

            bmp_set_bit(&allocator->buddy_state_map, bmp_index, 0);

            // Num pages has to be adjusted for the left side block which contains the first (1 << split_order) pages of the
            // shrunk block.
            num_pages -= (1 << split_order);
        } else {
            // The right hand block of the split is going to be freed, so we need to create a freelist entry and toggle the bitmap bit for this pair of buddies.
            __allocate_freelist_entry(allocator, block_offset + (1 << split_order), split_order);

            bmp_set_bit(&allocator->buddy_state_map, bmp_index, 1);

            // We are going to continue splitting from the left hand side so we don't need to change block_offset.
            // num pages remains unchanged in this case since it's fully encapsulated in the left hand block.
        }

        --split_order;
    }

    // Accounting
    size_t bytes_freed = ((1ul << block_order) - num_pages) << PAGE_ORDER;
    allocator->free_space_bytes += bytes_freed;
    allocator->allocated_bytes -= bytes_freed;
}

void buddy_freelist_pool_expand(buddy_allocator_t *allocator, void *slab) {
    slab_cache_prealloc(&allocator->freelist_cache, slab, 1);
}
