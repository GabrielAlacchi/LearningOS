#include <mm/buddy.h>
#include <mm/page.h>


// Stores the max order allocatable by alignment given a page offset in a block.
u8_t alignment_order_cache[1 << MAX_ORDER];


// Assumes the base page is free, it's the caller's responsibility to verify this.
static inline u8_t __largest_usable_order(phys_addr_t block_base) {
    const page_info_t *base_page = page_info(block_base);

    for (u16_t i = 1; i < (1 << MAX_ORDER); ++i) {
        if (unlikely(base_page[i].flags & PAGE_UNUSABLE)) {
            return log2(i);
        }
    }
}

static inline u16_t __usable_size_in_maxblock(phys_addr_t block_base, phys_addr_t end_addr) {
    const page_info_t *base_page = page_info(block_base);
    u16_t i;

    for (i = 0; i < (1 << MAX_ORDER); ++i) {
        phys_addr_t block_addr = block_base + ((phys_addr_t)i << PAGE_ORDER);

        if (unlikely(base_page[i].flags & PAGE_UNUSABLE || block_addr >= end_addr)) {
            return i;
        }
    }

    return i;
}


static u8_t __max_order_by_alignment(phys_addr_t addr) {
    for (u8_t order = 0; order <= MAX_ORDER; ++order) {
        if (addr & MASK_FOR_FIRST_N_BITS(order + PAGE_ORDER)) {
            return order - 1;
        }
    }

    return MAX_ORDER;
}

static void __add_freelist_to_head(buddy_t *allocator, page_info_t *block_base_page, u8_t order) {
    buddy_freelist_t *node = &block_base_page->buddy_freelist;

    node->page_addr = page_address_from_info(block_base_page);
    node->next = allocator->free_blocks[order];
    node->prev = NULL;

    if (allocator->free_blocks[order] != NULL) {
        allocator->free_blocks[order]->prev = node;
    }

    allocator->free_blocks[order] = node;
    allocator->free_blocks_count[order] += 1;

    size_t page_offset = (node->page_addr - allocator->base_addr) >> PAGE_ORDER;
    size_t bmp_index = buddy_bmp_index_of(page_offset, order);
    bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);
}


static void __initialize_page_map(buddy_t *allocator) {
    // Set page addr for the entire allocator region.
    for (size_t addr = allocator->base_addr; addr < allocator->end_addr; addr += PAGE_SIZE) {
        page_info_t *page = page_info(addr);
        page->buddy_freelist.page_addr = addr;
        page->flags |= PAGE_BUDDY;
    }

    for (u16_t i = 0; i < (1 << MAX_ORDER); ++i) {
        alignment_order_cache[i] = __max_order_by_alignment(i * PAGE_SIZE);
    }

    // Figure out the largest allocatable order for each page of the system. This will also give us
    // the ability to construct the initial freelists
    phys_addr_t cursor = allocator->base_addr;

    // This is slow but is a one time cost
    while (cursor < allocator->end_addr) {
        u16_t usable_pages = __usable_size_in_maxblock(cursor, allocator->end_addr);
        page_info_t *block_page = page_info(cursor);

        s16_t block_max_usable[1 << MAX_ORDER];

        for (u16_t i = 0; i < (1 << MAX_ORDER); ++i) {
            if (unlikely(i >= usable_pages)) {
                block_max_usable[i] = -1;
                block_page[i].max_usable_order = -1;
            } else {
                u8_t max_physically_usable = log2(usable_pages - i);
                block_page[i].max_usable_order = MIN(alignment_order_cache[i], max_physically_usable);
                block_max_usable[i] = MIN(alignment_order_cache[i], max_physically_usable);
            }
        }

        cursor += (PAGE_SIZE << MAX_ORDER);
    }

    // Using page->max_usable_order we can determine the initial freelists
    // The goal is to be greedy and get the largest possible blocks at each stage.
    cursor = allocator->base_addr;

    while (cursor < allocator->end_addr) {
        page_info_t *page = page_info(cursor);
        
        if (unlikely(page->max_usable_order < 0)) {
            // Go to the next max order block.
            cursor = (cursor + (1ul << (MAX_ORDER + PAGE_ORDER))) & ~MASK_FOR_FIRST_N_BITS(MAX_ORDER + PAGE_ORDER);
            continue;
        }

        __add_freelist_to_head(allocator, page, page->max_usable_order);

        allocator->free_pages += 1ul << page->max_usable_order;
        cursor += PAGE_SIZE << page->max_usable_order;
    }
}


void buddy_allocator_init(buddy_t *allocator, void *bitmap_base, phys_addr_t base, phys_addr_t end) {
    memset(allocator, 0, sizeof(buddy_t));

    allocator->base_addr = base;
    allocator->end_addr = end;

    size_t num_pages = (end - base) >> PAGE_ORDER;
    size_t num_bits = buddy_bmp_size_bits(num_pages);

    bmp_init(&allocator->buddy_state_map, (u8_t*)bitmap_base, num_bits);
    allocator->allocated_pages = 0;
    
    __initialize_page_map(allocator);
}


void __write_alloc_info(const phys_addr_t block_base, u8_t order) {
    page_info_t *pages = page_info(block_base);

    pages[0]._block_used_count = 1 << order;

    for (u16_t i = 0; i < (1 << order); ++i) {
        reference_page(&pages[i]);
        pages[i]._block_order = order;
    }
}


static void __unlink_freelist_node(buddy_t *allocator, buddy_freelist_t *node, u8_t order) {
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        // Node is the head.
        allocator->free_blocks[order] = node->next;
    }

    allocator->free_blocks_count[order] -= 1;
}


static freelist_entry_t *__remove_freelist_head(buddy_t *allocator, u8_t order) {
    buddy_freelist_t *head = allocator->free_blocks[order];
    allocator->free_blocks[order] = head->next;

    if (head->next != NULL) {
        head->next->prev = NULL;
    }

    allocator->free_blocks_count[order] -= 1;

    size_t page_offset = (head->page_addr - allocator->base_addr) >> PAGE_ORDER;
    size_t bmp_index = buddy_bmp_index_of(page_offset, order);
    bmp_toggle_bit(&allocator->buddy_state_map, bmp_index);

    return head;
}


freelist_entry_t *__find_and_split(buddy_t *allocator, u8_t target_order) {
    s8_t order = target_order + 1;

    buddy_freelist_t *free_block;
    do {
        free_block = allocator->free_blocks[order];
    } while (free_block == NULL && order++ < MAX_ORDER);

    if (unlikely(order > MAX_ORDER)) {
        return NULL;
    }

    // The node that we'll be splitting.
    buddy_freelist_t *parent_node = __remove_freelist_head(allocator, order);
    
    page_info_t *parent_block_base = page_info(parent_node->page_addr);
    
    // For each order from order - 1 to target_order + 1 (inclusive) we need to create 
    // a free list entry for the buddy of the page_offset of the free_block that we found.
    // These blocks represent the result of splitting the blocks at each level.
    // We also need to toggle the free bit at each of these levels.
    for (order = order - 1; order >= target_order; --order) {
        page_info_t *new_block = parent_block_base + (1 << order);
        __add_freelist_to_head(allocator, new_block, order);
    }

    return parent_node;
}


phys_addr_t buddy_alloc_pages(buddy_t *allocator, u8_t order) {
    buddy_freelist_t *free_node;

    if (allocator->free_blocks[order] != NULL) {
        // Remove the head of the freelist at the given order.
        free_node = __remove_freelist_head(allocator, order);
    } else {
        free_node = __find_and_split(allocator, order);
    }

    __write_alloc_info(free_node->page_addr, order);

    // Memory Accounting
    allocator->allocated_pages += 1 << order;
    allocator->free_pages -= 1 << order;

    return free_node->page_addr;
}

static phys_addr_t __buddy_address(const phys_addr_t addr, const u8_t order) {
    if (!(addr & MASK_FOR_FIRST_N_BITS(PAGE_ORDER + order + 1))) {
        return addr + (PAGE_SIZE << order);
    } else {
        return addr - (PAGE_SIZE << order);    
    }
}

static inline int __is_block_usable(buddy_t *allocator, const phys_addr_t base, const u8_t order) {
    return base < allocator->end_addr && order <= page_info(base)->max_usable_order;
}

static int __can_coalesce(buddy_t *allocator, const phys_addr_t addr, const u8_t order) {
    size_t page_offset = (addr - allocator->base_addr) >> PAGE_ORDER;
    size_t bmp_index = buddy_bmp_index_of(page_offset, order);

    u8_t buddy_pair_state = bmp_get_bit(&allocator->buddy_state_map, bmp_index);

    return buddy_pair_state == 1 && __is_block_usable(allocator, __buddy_address(addr, order), order);
}

void buddy_free_pages(buddy_t *allocator, phys_addr_t pages) {
    page_info_t *page = page_info(pages);
    u8_t order = page->_block_order;

    u8_t target_order = order;
    phys_addr_t coalesced_addr = pages;

    while (target_order < MAX_ORDER && __can_coalesce(allocator, pages, target_order)) {
        phys_addr_t buddy_addr = __buddy_address(coalesced_addr, target_order);
        page_info_t *buddy = page_info(buddy_addr);
        __unlink_freelist_node(allocator, &buddy->buddy_freelist, target_order);
        size_t page_offset = (coalesced_addr - allocator->base_addr) >> PAGE_ORDER;
        size_t bmp_index = buddy_bmp_index_of(page_offset, target_order);

        // Since we're coalescing both the buddy and the current are free and thus the state is 0
        bmp_set_bit(&allocator->buddy_state_map, bmp_index, 0);
        ++target_order;

        coalesced_addr = MIN(buddy_addr, coalesced_addr);
    }

    page = page_info(coalesced_addr);
    __add_freelist_to_head(allocator, page, target_order);

    allocator->free_pages += 1 << order;
    allocator->allocated_pages -= 1 << order;
}
