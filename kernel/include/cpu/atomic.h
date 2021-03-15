#ifndef __CPU_ATOMIC_H
#define __CPU_ATOMIC_H

#include <types.h>

static inline u8_t cmpxchgb(u8_t *ptr, u8_t old_value, u8_t new_value) {
    volatile u8_t *__ptr = (volatile u8_t *)ptr;
    u8_t ret;
    asm volatile("lock cmpxchgb %2,%1"
                 : "=a" (ret), "+m" (*__ptr)
                 : "q" (new_value), "0" (old_value)
                 : "memory");

    return ret;
}

static inline u16_t cmpxchgw(u16_t *ptr, u16_t old_value, u16_t new_value) {
    volatile u16_t *__ptr = (volatile u16_t *)ptr;
    u16_t ret;
    asm volatile("lock cmpxchgw %2,%1"
                 : "=a" (ret), "+m" (*__ptr)
                 : "q" (new_value), "0" (old_value)
                 : "memory");

    return ret;
}

static inline u32_t cmpxchgl(u32_t *ptr, u32_t old_value, u32_t new_value) {
    volatile u32_t *__ptr = (volatile u32_t *)ptr;
    u32_t ret;
    asm volatile("lock cmpxchgl %2,%1"
                 : "=a" (ret), "+m" (*__ptr)
                 : "q" (new_value), "0" (old_value)
                 : "memory");

    return ret;
}

static inline u64_t cmpxchgq(u64_t *ptr, u64_t old_value, u64_t new_value) {
    volatile u64_t *__ptr = (volatile u64_t *)ptr;
    u64_t ret;
    asm volatile("lock cmpxchgq %2,%1"
                 : "=a" (ret), "+m" (*__ptr)
                 : "q" (new_value), "0" (old_value)
                 : "memory");

    return ret;
}


static inline u16_t counter16_increment_atomic(u16_t *counter, s16_t increment_by) {
    u16_t current_value = *counter;

    while (1) {
        u16_t returned = cmpxchgw(counter, current_value, current_value + increment_by);
        if (likely(returned == current_value)) {
            return returned + increment_by;
        }

        current_value = returned;
    }
}


#endif
