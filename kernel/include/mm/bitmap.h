#ifndef __MM_BITMAP_H
#define __MM_BITMAP_H

#include <mm.h>
#include <types.h>

typedef struct __bitmap {
    u8_t *base;
    size_t size_bytes;
} bitmap_t;

void bmp_init(bitmap_t *bmp, u8_t *base, size_t size_bits);
u8_t bmp_get_bit(const bitmap_t *bmp, const size_t index);
u8_t bmp_set_bit(bitmap_t *bmp, const size_t index, u8_t value);
u8_t bmp_toggle_bit(bitmap_t *bmp, const size_t index);

#endif
