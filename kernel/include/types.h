#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned long int u64_t;
typedef unsigned int u32_t;
typedef unsigned short u16_t;
typedef unsigned char u8_t;

typedef signed long int s64_t;
typedef signed int s32_t;
typedef signed short s16_t;
typedef signed char s8_t;

// We're targeting 64-bit so let's use a full long register as size_t
typedef u64_t size_t;

#define likely(expr) __builtin_expect(expr, 1)
#define unlikely(expr) __builtin_expect(expr, 0)

#endif
