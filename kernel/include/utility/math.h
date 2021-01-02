#ifndef __UTILITY_MATH_H
#define __UTILITY_MATH_H

#include <types.h>

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

#define MASK_FOR_FIRST_N_BITS(n) ((1ul << n) - 1)

static inline size_t round_up_shift_right(size_t x, u8_t shift) {
    size_t shifted = x >> shift;

    if (x & MASK_FOR_FIRST_N_BITS(shift)) {
        return shifted + 1;
    }

    return shifted;
}

#endif
