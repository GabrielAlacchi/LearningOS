#include <suite.h>
#include <setup/setup_bootinfo.h>
#include <cmocka.h>

#include <mm.h>
#include <mm/buddy_alloc.h>


// Sanity check for the buddy's state bitmap. Every bit should be hit twice while scanning
// all possible block_offsets and orders.
static void test_buddy_bit_mapping(void **state) {
    size_t total_pages = 1024;
    size_t total_bits = buddy_bmp_size_bits(total_pages);

    u8_t *zero = malloc((total_bits + 7) / 8);
    memset(zero, 0, (total_bits + 7) / 8);

    u8_t *buffer = malloc((total_bits + 7) / 8);

    bitmap_t bmp;
    bmp_init(&bmp, buffer, total_bits);

    for (size_t page_offset = 0; page_offset < total_pages; ++page_offset) {
        for (u8_t order = 0; order <= MAX_ORDER; ++order) {
            // If the order is valid for this page.
            if (page_offset % (1ul << order) == 0) {
                size_t index = buddy_bmp_index_of(page_offset, order);
                bmp_toggle_bit(&bmp, index);
            }
        }
    }

    // All bits should be 0 since they should have been toggled twice.
    assert_int_equal(0, memcmp(buffer, zero, (total_bits + 7) / 8));

    free(zero);
    free(buffer);
}


__attribute__((aligned(1 << (SLAB_ORDER + PAGE_ORDER))))
slab_t freelist_pool[4];


buddy_prealloc_vector vector;
buddy_memory_pool pool;


static void check_free_integrity(buddy_allocator_t *allocator) {
    size_t free_bytes = 0;
    char buf[10];

    size_t order_counts[MAX_ORDER + 1];
    size_t freelist_entries = 0;

    // Verify integrity by counting free bytes with the free list and ensuring they make sense
    for (u8_t order = 0; order <= MAX_ORDER; ++order) {
        order_counts[order] = 0;

        freelist_entry_t *free = allocator->freelists[order];

        while (free != NULL) {
            ++freelist_entries;
            ++order_counts[order];
            free_bytes += 1ul << (order + PAGE_ORDER);

            free = free->next_entry == NULL ? NULL : free + free->next_entry;
        }
    }

    assert_int_equal(allocator->freelist_cache.allocated_objects, freelist_entries);
    assert_int_equal(allocator->free_space_bytes, free_bytes);
}


static int setup_buddy_pool(void **state) {
    vector = buddy_estimate_pool_size((PHYS_MEM_SIZE - 0x1000) >> PAGE_ORDER);

    pool.bitmap_and_struct_pool = malloc(vector.bitmap_and_struct_pages << PAGE_ORDER);
    pool.freelist_pool = freelist_pool;
    pool.freelist_pool_slabs = vector.freelist_pool_slabs;
    return 0;
}


static int teardown_buddy_pool(void **state) {
    free(pool.bitmap_and_struct_pool);
    return 0;
}


static void test_buddy_init(void **state) {
    buddy_allocator_t allocator;
    buddy_init(&allocator, pool, 0x1000, PHYS_MEM_SIZE);
    size_t bits = buddy_bmp_size_bits((PHYS_MEM_SIZE - 0x1000) >> PAGE_ORDER);
    
    assert_int_equal((bits + 7) >> 3, allocator.buddy_state_map.size_bytes);
    assert_int_equal(0x1000, allocator.base_addr);
    assert_int_equal(PHYS_MEM_SIZE, allocator.end_addr);
    assert_int_equal(PHYS_MEM_SIZE - MEMHOLE_SIZE - 0x2000, allocator.free_space_bytes);

    check_free_integrity(&allocator);
}


static void test_buddy_allocations(void **state) {
    buddy_allocator_t allocator;
    buddy_init(&allocator, pool, 0x1000, PHYS_MEM_SIZE);

    phys_addr_t allocs[6];
    u8_t alloc_orders[6] = { 7, 5, 4, 3, 1, 0 };

    allocs[0] = buddy_alloc_block(&allocator, 7);
    check_free_integrity(&allocator);

    allocs[1] = buddy_alloc_block(&allocator, 5);
    check_free_integrity(&allocator);

    allocs[2] = buddy_alloc_block(&allocator, 4);
    check_free_integrity(&allocator);

    allocs[3] = buddy_alloc_block(&allocator, 3);
    check_free_integrity(&allocator);

    allocs[4] = buddy_alloc_block(&allocator, 1);
    check_free_integrity(&allocator);

    allocs[5] = buddy_alloc_block(&allocator, 0);
    check_free_integrity(&allocator);

    size_t alloc_pages = 0;
    for (u8_t i = 0; i < 6; ++i) {
        alloc_pages += (1ul << alloc_orders[i]);
    }

    assert_int_equal(alloc_pages << 12, allocator.allocated_bytes);

    buddy_free_block(&allocator, allocs[5], 0);
    check_free_integrity(&allocator);
    
    buddy_free_block(&allocator, allocs[2], 4);
    check_free_integrity(&allocator);
    
    buddy_free_block(&allocator, allocs[0], 7);
    check_free_integrity(&allocator);
    
    buddy_free_block(&allocator, allocs[3], 3);
    check_free_integrity(&allocator);
    
    buddy_free_block(&allocator, allocs[4], 1);
    check_free_integrity(&allocator);

    buddy_free_block(&allocator, allocs[1], 5);
    check_free_integrity(&allocator);

    assert_int_equal(0, allocator.allocated_bytes);
}


static void test_block_splitting_and_coalescing(void **state) {
    buddy_allocator_t allocator;
    buddy_init(&allocator, pool, 0x1000, PHYS_MEM_SIZE);

    u8_t order_0_free = 0, order_1_free = 0;
    freelist_entry_t *freelist = allocator.freelists[0];
    
    while (freelist != NULL) {
        ++order_0_free;
        freelist = freelist->next_entry == NULL ? NULL : freelist + freelist->next_entry;
    }

    for (u8_t i = 0; i < order_0_free; ++i) {
        buddy_alloc_block(&allocator, 0);
    }

    freelist = allocator.freelists[1];
    while (freelist != NULL) {
        ++order_1_free;
        freelist = freelist->next_entry == NULL ? NULL : freelist + freelist->next_entry;
    }

    for (u8_t i = 0; i < order_1_free; ++i) {
        buddy_alloc_block(&allocator, 1);
    }

    assert_null(allocator.freelists[0]);
    assert_null(allocator.freelists[1]);

    u8_t next_order = 1;
    while (allocator.freelists[next_order] == NULL) {
        ++next_order;
    }

    size_t block_offset = allocator.freelists[next_order]->page_offset;
    size_t block_end = block_offset + (1 << next_order);

    size_t returned_offset = buddy_alloc_block(&allocator, 0);
    check_free_integrity(&allocator);

    assert_int_not_equal(block_offset, allocator.freelists[next_order]->page_offset);
    assert_in_range(returned_offset >> PAGE_ORDER, block_offset, block_end);

    // We expect the block has been split.
    for (u8_t order = 0; order < next_order; ++order) {
        // Except there to be a block within block_offset to block_end at this order at the head of the freelists.
        assert_non_null(allocator.freelists[order]);
        assert_in_range(allocator.freelists[order]->page_offset, block_offset, block_end);
    }

    // The block should coalesce.
    buddy_free_block(&allocator, returned_offset, 0);

    for (u8_t order = 0; order < next_order; ++order) {
        assert_null(allocator.freelists[order]);
    }

    assert_non_null(allocator.freelists[next_order]);
    assert_int_equal(allocator.freelists[next_order]->page_offset, block_offset);
}


static void test_block_shrinking(void **state) {
    buddy_allocator_t allocator;
    buddy_init(&allocator, pool, 0x1000, PHYS_MEM_SIZE);
    phys_addr_t block_base = buddy_alloc_block(&allocator, 7);
    size_t block_offset = (block_base - allocator.base_addr) >> PAGE_ORDER;

    buddy_shrink_block(&allocator, block_base, 7, 33);

    freelist_entry_t *freelist = allocator.freelists[6];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 6), freelist->page_offset);

    freelist = allocator.freelists[5];
    assert_not_in_range(freelist->page_offset, block_offset, block_offset + (1ul << 7));

    freelist = allocator.freelists[4];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 5) + (1ul << 4), freelist->page_offset);

    freelist = allocator.freelists[3];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 5) + (1ul << 3), freelist->page_offset);

    freelist = allocator.freelists[2];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 5) + (1ul << 2), freelist->page_offset);

    freelist = allocator.freelists[1];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 5) + (1ul << 1), freelist->page_offset);

    freelist = allocator.freelists[0];
    assert_non_null(freelist);
    assert_int_equal(block_offset + (1ul << 5) + 1, freelist->page_offset);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_buddy_bit_mapping),
        cmocka_unit_test_setup_teardown(test_buddy_init, setup_buddy_pool, teardown_buddy_pool),
        cmocka_unit_test_setup_teardown(test_buddy_allocations, setup_buddy_pool, teardown_buddy_pool),
        cmocka_unit_test_setup_teardown(test_block_splitting_and_coalescing, setup_buddy_pool, teardown_buddy_pool),
        cmocka_unit_test_setup_teardown(test_block_shrinking, setup_buddy_pool, teardown_buddy_pool),
    };

    cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}
