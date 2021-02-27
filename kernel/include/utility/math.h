#ifndef __UTILITY_MATH_H
#define __UTILITY_MATH_H

#include <types.h>

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

#define MASK_FOR_FIRST_N_BITS(n) ((1ul << (n)) - 1)

static inline size_t round_up_shift_right(size_t x, u8_t shift) {
    size_t shifted = x >> shift;

    if (x & MASK_FOR_FIRST_N_BITS(shift)) {
        return shifted + 1;
    }

    return shifted;
}

static inline u8_t bit_order(u32_t x) {
    s32_t leading_zeros = __builtin_clz(x);
    
    u8_t order = 31 - leading_zeros;

    if (x == (1u << order)) {
        return order;
    } else {
        return order + 1;
    }
}

// Rounds up to the next power of two. If the number is a power of two it remains unchanged.
static inline size_t next_power_of_two(u64_t x) {
    // This series of ors and shifts sets every bit right of the most significant set bit to 1.
    // this brings us to 1 less than the next power of two.
    u64_t cp = x | x >> 1;
    cp |= cp >> 2;
    cp |= cp >> 4;
    cp |= cp >> 8;
    cp |= cp >> 16;
    cp |= cp >> 32;

    // Add 1 to get the next power of 2
    cp += 1;

    // If the right shift of 1 is equal to x this indicates that x also is a power of 2, so return x, else return cp.
    if ((cp >> 1) == x) {
        return x;
    } else {
        return cp;
    }
}

// Truncates the first n bits of a number essentially rounding down to the nearest multiple of 2^n.
// Use for n <= 63.
static inline size_t trunc_n_bits(size_t x, u8_t n) {
    return x & (~MASK_FOR_FIRST_N_BITS(n));
}

#endif
