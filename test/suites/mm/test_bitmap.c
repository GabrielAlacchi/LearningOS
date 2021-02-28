#include <suite.h>
#include <cmocka.h>
#include <memory.h>

#include <mm/bitmap.h>

static void test_bmp_init(void **state) {
    bitmap_t bmp;

    u8_t *buffer = (u8_t*)malloc(1001);
    bmp_init(&bmp, buffer, 8000);

    assert_ptr_equal(buffer, bmp.base);
    assert_int_equal(1000, bmp.size_bytes);

    bmp_init(&bmp, buffer, 8001);
    assert_ptr_equal(buffer, bmp.base);
    assert_int_equal(1001, bmp.size_bytes);

    free(buffer);
}

static void test_bmp_get_set_and_toggle(void **state) {
    bitmap_t bmp;

    u8_t *buffer = (u8_t*)calloc(1000, 1);
    bmp_init(&bmp, buffer, 8000);

    bmp_set_bit(&bmp, 0, 1);
    bmp_set_bit(&bmp, 324, 1);

    assert_int_equal(1, bmp_get_bit(&bmp, 0));
    assert_int_equal(1, bmp_get_bit(&bmp, 324));

    assert_int_equal(1, bmp_toggle_bit(&bmp, 7999));
    assert_int_equal(0, bmp_toggle_bit(&bmp, 7999));

    free(buffer);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bmp_init),
        cmocka_unit_test(test_bmp_get_set_and_toggle),
    };

    return cmocka_run_group_tests(tests, suite_setup, suite_teardown);
}