#include <suite.h>
#include <cmocka.h>
#include <log.h>


struct __printk_specifier {
    char spec;
    char lengthspec;
    u8_t left_justify;
    u8_t plus;
    u8_t space;
    u8_t zeros;
    u8_t prefix; // # Flag (used for printing octal or hex and appending 0x or O)
    u8_t width;
    u8_t precision;
};


extern const char *__parse_specifier(const char *spec_base, struct __printk_specifier *spec);


char buffer[4096] = { 0 };
u16_t cursor = 0;


void *kmalloc(u16_t size) {
    return malloc(size);
}


void kfree(void *ptr) {
    return free(ptr);
}


void __test_putchar(char c) {
    buffer[cursor++] = c;
    buffer[cursor] = 0;
}


static void test_parse_specifier() {
    struct __printk_specifier spec;
    __parse_specifier("s", &spec);

    assert_int_equal('s', spec.spec);
    assert_int_equal(0, spec.lengthspec);
    assert_int_equal(0, spec.space);
    assert_int_equal(0, spec.plus);
    assert_int_equal(0, spec.left_justify);
    assert_int_equal(0, spec.zeros);
    assert_int_equal(0, spec.width);
    assert_int_equal(0, spec.precision);

    __parse_specifier("052d", &spec);

    assert_int_equal('d', spec.spec);
    assert_int_equal(0, spec.lengthspec);
    assert_int_equal(0, spec.space);
    assert_int_equal(0, spec.plus);
    assert_int_equal(0, spec.left_justify);
    assert_int_equal(1, spec.zeros);
    assert_int_equal(52, spec.width);
    assert_int_equal(0, spec.precision);

    const char *pastspec = __parse_specifier("-3.12llda", &spec);

    assert_int_equal('d', spec.spec);
    assert_int_equal('L', spec.lengthspec);
    assert_int_equal(0, spec.space);
    assert_int_equal(0, spec.plus);
    assert_int_equal(1, spec.left_justify);
    assert_int_equal(0, spec.zeros);
    assert_int_equal(3, spec.width);
    assert_int_equal(12, spec.precision);
    assert_int_equal('a', *pastspec);

    __parse_specifier("", &spec);

    assert_int_equal(0, spec.spec);
}


static void test_printk_string_spec() {
    cursor = 0;
    vprintk("Some string printed %%\n", __test_putchar);
    assert_string_equal("Some string printed %\n", buffer);

    cursor = 0;
    vprintk("Some %s --- %s", __test_putchar, "Something", "Else");
    assert_string_equal("Some Something --- Else", buffer);
}


static void test_printk_signed_spec() {
    cursor = 0;
    vprintk("%d\n", __test_putchar, 3423);
    assert_string_equal("3423\n", buffer);

    cursor = 0;
    vprintk("%+d\n", __test_putchar, 3423);
    assert_string_equal("+3423\n", buffer);   

    cursor = 0;
    vprintk("%d\n", __test_putchar, -23);
    assert_string_equal("-23\n", buffer);    

    cursor = 0;
    vprintk("%07d\n", __test_putchar, 3423);
    assert_string_equal("0003423\n", buffer);

    cursor = 0;
    vprintk("%+07d\n", __test_putchar, 3423);
    assert_string_equal("+003423\n", buffer);

    cursor = 0;
    vprintk("%-8.4i\n", __test_putchar, 3);
    assert_string_equal("0003    \n", buffer);

    cursor = 0;
    vprintk("%- 8.4i\n", __test_putchar, 3);
    assert_string_equal(" 0003   \n", buffer);

    cursor = 0;
    vprintk("%lld\n", __test_putchar, (s64_t)-23224234234);
    assert_string_equal("-23224234234\n", buffer);

    cursor = 0;
    vprintk("%hhd\n", __test_putchar, 1000);
    assert_string_equal("-24\n", buffer);
}


static void test_printk_unsigned_spec() {
    cursor = 0;
    vprintk("%u\n", __test_putchar, 3423);
    assert_string_equal("3423\n", buffer);
    
    cursor = 0;
    vprintk("%#o\n", __test_putchar, 0777);
    assert_string_equal("0o777\n", buffer);

    cursor = 0;
    vprintk("%llx\n", __test_putchar, KERNEL_VMA);
    assert_string_equal("FFFFFFFF80000000\n", buffer);

    cursor = 0;
    vprintk("%-24p\n", __test_putchar, KERNEL_VMA);
    assert_string_equal("0xFFFFFFFF80000000      \n", buffer);

    cursor = 0;
    vprintk("%#hhx\n", __test_putchar, 0xFF);
    assert_string_equal("0xFF\n", buffer);
}


int main(void) {
    struct CMUnitTest tests[] = {
        cmocka_unit_test(test_parse_specifier),
        cmocka_unit_test(test_printk_string_spec),
        cmocka_unit_test(test_printk_signed_spec),
        cmocka_unit_test(test_printk_unsigned_spec),
    };

    cmocka_run_group_tests(tests, NULL, NULL);
}
