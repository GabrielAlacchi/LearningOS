#include <suite.h>
#include <setup/setup_bootinfo.h>
#include <cmocka.h>
#include <time.h>
#include <assertions.h>

#include <mm.h>
#include <mm/vm.h>
#include <mm/slab.h>

#include <utility/math.h>

typedef struct __test_object_t {
    void *a, *b;
} test_obj_t;


#define TEST_OBJS_PER_SLAB ((sizeof(slab_t) - sizeof(slab_header_t)) / sizeof(test_obj_t))


void *vm_alloc_block(u8_t flags, u16_t vmzone) {
    static size_t alloc_idx = 0;

    // We'll use the physical mem aligned to the slab size.
    slab_t *slabs_base = aligndown(__test_physical_mem + 0x100000, SLAB_ORDER + PAGE_ORDER);
    
    return slabs_base + alloc_idx++;
}


void __validate_slab_objs(kmem_cache_t *cache, slab_t *slab) {
    u16_t first_obj = slab->header.first_free_idx;

    if (slab->header.free_count == 0) {
        return;
    }

    slab_object_t *obj = slab->__slab_data + first_obj * cache->obj_cell_size;
    
    u16_t objs = 0;

    do {
        ++objs;
        obj = obj->header.if_free.next_free;
    } while (obj != NULL);

    assert_int_equal(slab->header.free_count, objs);
}


void __validate_slab_list(kmem_cache_t *cache, slab_t *head, u16_t expected_num_slabs) {
    if (head == NULL) {
        assert_int_equal(0, expected_num_slabs);
        return;
    }

    slab_t *prev = NULL;
    u16_t actual_num_slabs = 0;

    while (head != NULL) {
        __validate_slab_objs(cache, head);
        assert_ptr_equal(prev, head->header.prev);

        prev = head;
        head = head->header.next;
        ++actual_num_slabs;
    }

    assert_int_equal(expected_num_slabs, actual_num_slabs);
}


void __validate_cache_lists(kmem_cache_t *cache) {
    assert_aligned(cache->free_slabs, sizeof(slab_t));
    assert_aligned(cache->partial_slabs, sizeof(slab_t));
    assert_aligned(cache->full_slabs, sizeof(slab_t));

    __validate_slab_list(cache, cache->free_slabs, cache->total_free_slabs);
    __validate_slab_list(cache, cache->partial_slabs, cache->total_partial_slabs);
    __validate_slab_list(cache, cache->full_slabs, cache->total_full_slabs);
}


void test_slab_cache_init(void **state) {
    kmem_cache_t cache;
    slab_cache_init(&cache, sizeof(test_obj_t), _Alignof(test_obj_t), 0);
    
    assert_int_equal(0, cache.align_padding);
    assert_int_equal(16, cache.obj_size);
    assert_int_equal(16, cache.obj_cell_size);
    assert_int_equal((sizeof(slab_t) - sizeof(slab_header_t)) % 16 + sizeof(slab_header_t), cache.slab_overhead);
    assert_int_equal((sizeof(slab_t) - sizeof(slab_header_t)) / 16, cache.objs_per_slab);

    assert_null(cache.partial_slabs);
    assert_null(cache.free_slabs);
    assert_null(cache.full_slabs);

    assert_int_equal(0, cache.allocated_objects);
    assert_int_equal(0, cache.total_free_slabs);
    assert_int_equal(0, cache.total_full_slabs);
    assert_int_equal(0, cache.total_partial_slabs);

    // Test reserving some memory
    slab_cache_reserve(&cache, cache.objs_per_slab * 3 + 1);

    assert_int_equal(4, cache.total_free_slabs);
    __validate_cache_lists(&cache);
}


void test_slab_alloc(void **state) {
    kmem_cache_t cache;
    slab_cache_init(&cache, sizeof(test_obj_t), _Alignof(test_obj_t), 0);

    test_obj_t *obj = slab_alloc(&cache);

    assert_int_equal(1, cache.total_partial_slabs);
    __validate_cache_lists(&cache);

    assert_int_equal(1, cache.allocated_objects);
    assert_int_equal(cache.objs_per_slab - 1, cache.partial_slabs->header.free_count);

    // Reserve a free slab
    slab_cache_reserve(&cache, 3 * cache.objs_per_slab);
    assert_int_equal(1, cache.total_partial_slabs);
    assert_int_equal(3, cache.total_free_slabs);

    // Should reuse the partial slab. Verify that this is the case.
    assert_ptr_not_equal(obj, slab_alloc(&cache));
    assert_int_equal(1, cache.total_partial_slabs);
    assert_int_equal(3, cache.total_free_slabs);

    __validate_slab_objs(&cache, cache.partial_slabs);

    u16_t objs_to_alloc = cache.partial_slabs->header.free_count;

    // Fill up the partial slab, and verify that it goes to full_slabs
    for (u16_t i = 0; i < objs_to_alloc; ++i) {
        assert_ptr_not_equal(obj, slab_alloc(&cache));
    }

    assert_int_equal(0, cache.total_partial_slabs);
    assert_int_equal(1, cache.total_full_slabs);
    assert_int_equal(cache.objs_per_slab, cache.allocated_objects);
    
    __validate_cache_lists(&cache);

    slab_alloc(&cache);

    assert_int_equal(1, cache.total_partial_slabs);
    assert_int_equal(2, cache.total_free_slabs);
    assert_int_equal(cache.objs_per_slab + 1, cache.allocated_objects);
    __validate_cache_lists(&cache);
}


void test_slab_free(void **state) {
    kmem_cache_t cache;
    slab_cache_init(&cache, sizeof(test_obj_t), _Alignof(test_obj_t), 0);

    test_obj_t *objects[TEST_OBJS_PER_SLAB];

    for (u16_t i = 0; i < cache.objs_per_slab; ++i) {
        objects[i] = slab_alloc(&cache);
    }
    
    time_t t;
    srand((unsigned) time(&t));

    // Free 5 random objects
    int frees[5];

    for (u8_t i = 0; i < 5; ++i) {
        int idx;
        u8_t reject_sample = 1;

        while(reject_sample) {
            idx = rand() % TEST_OBJS_PER_SLAB;

            reject_sample = 0;

            for (u8_t j = 0; j < i; ++j) {
                if (idx == frees[j]) {
                    reject_sample = 1;
                    break;
                }
            }
        }
        
        frees[i] = idx;
        slab_free(&cache, objects[idx]);
    }

    assert_int_equal(1, cache.total_partial_slabs);
    assert_int_equal(0, cache.total_full_slabs);
    assert_int_equal(5, cache.partial_slabs->header.free_count);

    assert_int_equal(cache.objs_per_slab - 5, cache.allocated_objects);

    __validate_cache_lists(&cache);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_slab_cache_init),
        cmocka_unit_test(test_slab_alloc),
        cmocka_unit_test(test_slab_free),
    };

    cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}
