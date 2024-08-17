#include <suite.h>
#include <setup/setup_bootinfo.h>
#include <cmocka.h>

#include <mm.h>
#include <mm/buddy.h>
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


buddy_t _allocator;
void *bitmap_base;


static void check_free_integrity(buddy_t *allocator) {
    size_t free_pages = 0;

    size_t order_counts[MAX_ORDER + 1];
    size_t freelist_entries = 0;

    // Verify integrity by counting free bytes with the free list and ensuring they make sense
    for (u8_t order = 0; order <= MAX_ORDER; ++order) {
        order_counts[order] = 0;

        buddy_freelist_t *free = allocator->free_blocks[order];

        while (free != NULL) {
            ++freelist_entries;
            ++order_counts[order];
            free_pages += 1ul << order;

            free = free->next;
        }
    }

    for (u8_t i = 0; i <= MAX_ORDER; ++i) {
        assert_int_equal(order_counts[i], allocator->free_blocks_count[i]);
    }

    assert_int_equal(allocator->free_pages, free_pages);
}


static int setup_buddy(void **state) {
    size_t bits = buddy_bmp_size_bits((PHYS_MEM_SIZE - 0x100000) >> PAGE_ORDER);
    bitmap_base = malloc((bits + 7) >> 3);

    buddy_allocator_init(&_allocator, bitmap_base, 0x100000, PHYS_MEM_SIZE);
    return 0;
}


static int teardown_buddy(void **state) {
    free(bitmap_base);
    return 0;
}


static void test_buddy_init(void **state) {    
    size_t bits = buddy_bmp_size_bits((PHYS_MEM_SIZE - 0x100000) >> PAGE_ORDER);
    assert_int_equal((bits + 7) >> 3, _allocator.buddy_state_map.size_bytes);
    assert_int_equal(0x100000, _allocator.base_addr);
    assert_int_equal(PHYS_MEM_SIZE, _allocator.end_addr);
    assert_int_equal(0, _allocator.allocated_pages);
    assert_int_equal((PHYS_MEM_SIZE - 0x100000) >> PAGE_ORDER, _allocator.free_pages);

    check_free_integrity(&_allocator);
}


static void test_buddy_allocations(void **state) {
    phys_addr_t allocs[6];
    u8_t alloc_orders[6] = { 7, 5, 4, 3, 1, 0 };

    allocs[0] = buddy_alloc_pages(&_allocator, 7);
    check_free_integrity(&_allocator);

    allocs[1] = buddy_alloc_pages(&_allocator, 5);
    check_free_integrity(&_allocator);

    allocs[2] = buddy_alloc_pages(&_allocator, 4);
    check_free_integrity(&_allocator);

    allocs[3] = buddy_alloc_pages(&_allocator, 3);
    check_free_integrity(&_allocator);

    allocs[4] = buddy_alloc_pages(&_allocator, 1);
    check_free_integrity(&_allocator);

    allocs[5] = buddy_alloc_pages(&_allocator, 0);
    check_free_integrity(&_allocator);

    size_t alloc_pages = 0;
    for (u8_t i = 0; i < 6; ++i) {
        alloc_pages += (1ul << alloc_orders[i]);
    }

    assert_int_equal(alloc_pages, _allocator.allocated_pages);

    buddy_free_pages(&_allocator, allocs[5]);
    check_free_integrity(&_allocator);
    
    buddy_free_pages(&_allocator, allocs[2]);
    check_free_integrity(&_allocator);
    
    buddy_free_pages(&_allocator, allocs[0]);
    check_free_integrity(&_allocator);
    
    buddy_free_pages(&_allocator, allocs[3]);
    check_free_integrity(&_allocator);
    
    buddy_free_pages(&_allocator, allocs[4]);
    check_free_integrity(&_allocator);

    buddy_free_pages(&_allocator, allocs[1]);
    check_free_integrity(&_allocator);

    assert_int_equal(0, _allocator.allocated_pages);
}


static void test_block_splitting_and_coalescing(void **state) {
    u8_t order_0_free = 0, order_1_free = 0;

    // Deplete every block of orders 0 to MAX_ORDER - 1
    for (u8_t order = 0; order < MAX_ORDER; ++order) {
        for (size_t i = 0; i < _allocator.free_blocks_count[order]; ++i) {
            buddy_alloc_pages(&_allocator, order);
        }

        assert_null(_allocator.free_blocks[order]);
    }

    check_free_integrity(&_allocator);

    size_t first_block = _allocator.free_blocks[MAX_ORDER]->page_addr;
    size_t block_end = first_block + (PAGE_SIZE << MAX_ORDER);

    size_t returned_block = buddy_alloc_pages(&_allocator, 0);
    check_free_integrity(&_allocator);

    assert_int_not_equal(first_block, _allocator.free_blocks[MAX_ORDER]->page_addr);
    assert_in_range(returned_block, first_block, block_end);

    // We expect the block has been split.
    for (u8_t order = 0; order < MAX_ORDER; ++order) {
        // Except there to be a block within block_offset to block_end at this order at the head of the freelists.
        assert_non_null(_allocator.free_blocks[order]);
        assert_in_range(_allocator.free_blocks[order]->page_addr, first_block, block_end);
    }

    // The block should coalesce.
    buddy_free_pages(&_allocator, returned_block);

    for (u8_t order = 0; order < MAX_ORDER; ++order) {
        assert_null(_allocator.free_blocks[order]);
    }

    assert_non_null(_allocator.free_blocks[MAX_ORDER]);
    assert_int_equal(_allocator.free_blocks[MAX_ORDER]->page_addr, first_block);

    check_free_integrity(&_allocator);
    
    // Same scenario but this time we'll cherry pick order 3 to block coalescing above that order.
    returned_block = buddy_alloc_pages(&_allocator, 0);

    assert_int_not_equal(first_block, _allocator.free_blocks[MAX_ORDER]->page_addr);
    assert_in_range(returned_block, first_block, block_end);

    buddy_alloc_pages(&_allocator, 3);
    buddy_free_pages(&_allocator, returned_block);

    check_free_integrity(&_allocator);

    for (u8_t order = 0; order < 3; ++order) {
        assert_null(_allocator.free_blocks[order]);
    }

    for (u8_t order = 3; order < MAX_ORDER; ++order) {
        assert_int_equal(1, _allocator.free_blocks_count[3]);
    }
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_buddy_bit_mapping),
        cmocka_unit_test_setup_teardown(test_buddy_init, setup_buddy, teardown_buddy),
        cmocka_unit_test_setup_teardown(test_buddy_allocations, setup_buddy, teardown_buddy),
        cmocka_unit_test_setup_teardown(test_block_splitting_and_coalescing, setup_buddy, teardown_buddy),
    };

    cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}
