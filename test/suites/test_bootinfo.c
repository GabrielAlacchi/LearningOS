#include <suite.h>
#include <mm/boot_mmap.h>
#include <utility/bootinfo.h>
#include <cmocka.h>


extern physmem_region_t region_pool[];


static void test_boot_tag_by_type(void **state) {
    assert_non_null(boot_tag_by_type(MULTIBOOT_TAG_TYPE_MMAP));
    assert_null(boot_tag_by_type(MULTIBOOT_TAG_TYPE_NETWORK));
}


static void test_load_physmem_regions(void **state) {
    physmem_region_t *region = load_physmem_regions();

    // Expect two regions starting from 0x1000 and then one at 0x100000 to MEM_LIMIT.
    assert_int_equal(region->free_start, 0x1000);
    assert_int_not_equal(region->region_end, 0x100000);
    assert_non_null(region->next_region);

    region = region->next_region;
    assert_int_equal(region->free_start, 0x100000);
    assert_int_equal(region->region_end, PHYS_MEM_SIZE);
    assert_null(region->next_region);
}

static void test_reserve_region(void **state) {
    physmem_region_t *region = load_physmem_regions();


    // Reserve 10 pages.
    phys_addr_t addr = reserve_physmem_region(10);

    assert_int_equal(0x1000, addr);
    assert_int_equal(11 << PAGE_ORDER, region->free_start);

    // Try reserving a large number of pages, forcing it to allocate at 1MB since there's no space between 1KB and 640KB.
    addr = reserve_physmem_region(1000);
    
    assert_int_equal(0x100000, addr);
    assert_int_equal(0x100000 + (1000 << PAGE_ORDER), region->next_region->free_start);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_boot_tag_by_type),
        cmocka_unit_test(test_load_physmem_regions),
        cmocka_unit_test(test_reserve_region),
    };

    return cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}
