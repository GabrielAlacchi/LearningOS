#ifndef __ASSERTIONS
#define __ASSERTIONS

#include <types.h>
#include <cmocka.h>

static inline void assert_aligned(void *ptr, u64_t byte_alignment) {
    u64_t integral = (u64_t)ptr;
    assert_int_equal(integral, (integral & ~(byte_alignment - 1)));
}

#endif
