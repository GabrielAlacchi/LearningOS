#include <mm/bitmap.h>
#include <utility/strings.h>


void bmp_init(bitmap_t *bmp, u8_t *base, size_t size_bits) {
    bmp->base = base;
    bmp->size_bytes = size_bits / 8;
    
    if (size_bits % 8 > 0) {
        bmp->size_bytes += 1;
    }

    memset(bmp->base, 0, bmp->size_bytes);
}

u8_t bmp_get_bit(const bitmap_t *bmp, const size_t index) {
    const size_t byte_index = index / 8;

    if (byte_index >= bmp->size_bytes) {
        return 0;
    }

    const u8_t byte = bmp->base[byte_index];
    return (byte >> (index % 8)) & 0x1;
}

u8_t bmp_set_bit(bitmap_t *bmp, const size_t index, u8_t value) {
    const size_t byte_index = index / 8;

    if (byte_index >= bmp->size_bytes) {
        return 0;
    }

    const u8_t byte = bmp->base[byte_index];

    if (value > 0) {
        bmp->base[byte_index] |= 1 << (index % 8);
    } else {
        bmp->base[byte_index] &= ~(1 << (index % 8));
    }
    
    return 1;
}

u8_t bmp_toggle_bit(bitmap_t *bmp, const size_t index) {
    const size_t byte_index = index / 8;
    const u8_t bit_mask = 1 << (index % 8);

    if (byte_index >= bmp->size_bytes) {
        return 0;
    }

    const u8_t byte = bmp->base[byte_index];

    if (byte & bit_mask) {
        bmp->base[byte_index] &= ~bit_mask;
    } else {
        bmp->base[byte_index] |= bit_mask;
    }
}
