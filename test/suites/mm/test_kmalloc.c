#include <suite.h>
#include <cmocka.h>
#include <mm/kmalloc.h>

#include <time.h>


extern u8_t __cache_idx_map[MAX_KMALLOC_SIZE / 8];
extern kmem_cache_t __caches[];


void *vm_alloc_block(u8_t flags, u16_t vmzone) {
    static size_t alloc_idx = 0;

    // We'll use the physical mem aligned to the slab size.
    slab_t *slabs_base = aligndown(__test_physical_mem + 0x100000, SLAB_ORDER + PAGE_ORDER);
    
    return slabs_base + alloc_idx++;
}


static void test_kmalloc_init(void **state) {
    kmalloc_init();

    for (u8_t i = 0; i < sizeof(kmalloc_sizes) / sizeof(u16_t); ++i) {
        assert_int_equal(kmalloc_sizes[i], __caches[i].obj_cell_size);
    }

    // Ensure allocations are mapped correctly
    for (u16_t alloc_size = 1; alloc_size <= MAX_KMALLOC_SIZE; ++alloc_size) {
        u16_t first_fit = 0;
        
        while (alloc_size > kmalloc_sizes[first_fit]) {
            ++first_fit;
        }

        assert_int_equal(first_fit, __cache_idx_map[(alloc_size + 7) >> 3]);
    }
}

static void test_kmalloc(void **state) {
    kmalloc_init();

    for (u16_t i = 1; i <= MAX_KMALLOC_SIZE; ++i) {
        void *ptr = kmalloc(i);
        slab_t *slab = aligndown(ptr, SLAB_ORDER + PAGE_ORDER);

        u16_t expected_cache = __cache_idx_map[(i + 7) >> 3];
        assert_int_equal(expected_cache, slab->header.cache_id);
    }
}


static void test_kfree(void **state) {
    kmalloc_init();
    
    time_t t;
    srand((unsigned) time(&t));

    void *allocations[100];

    for (u8_t i = 0; i < 100; ++i) {
        u16_t size = (rand() % MAX_KMALLOC_SIZE) + 1;
        allocations[i] = kmalloc(size);
    }

    for (u8_t i = 0; i < 100; ++i) {
        kfree(allocations[i]);

        u8_t alloc_objects = 0;
        for (u8_t i = 0; i < sizeof(kmalloc_sizes) / sizeof(u16_t); ++i) {
            kmem_cache_t *cache = __caches + i;
            alloc_objects += cache->allocated_objects;
        }

        assert_int_equal(alloc_objects, 100 - i - 1);
    }
}


int main(void) {
    struct CMUnitTest tests[] = {
        cmocka_unit_test(test_kmalloc_init),
        cmocka_unit_test(test_kmalloc),
        cmocka_unit_test(test_kfree),
    };

    cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}
