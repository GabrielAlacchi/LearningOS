#ifndef _UTILS_MEM_H
#define _UTILS_MEM_H

#include <types.h>

// We don't have libc in kernel space so we need to redefine some useful libc helpers
// here

int isdigit(char c);
int isalpha(char c);
int isspace(char c);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *str);

void *memcpy(void *dest, const void *source, size_t n);
void *memset(void *dest, u8_t value, size_t n);

void reverse(char *str);
int itoa(s64_t value, char *str, u64_t base);
int utoa(u64_t value, char *str, u64_t base);
char *ljust(char *str, size_t n, char pad_char);

void ptr_to_hex(void *ptr, char *buf);

#endif
