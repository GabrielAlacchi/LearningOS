#include <utility/strings.h>
#include <types.h>

// Naive implementations are fine for now.
// Obviously there are faster assembly implementations which copy a full word in bits at a time.

inline int isdigit(char c) {
    return (c >= '0' && c <= '9');
}

inline int isalpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline int isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

char *strcpy(char *dest, const char *src) {
    char * dest_base = dest;

    while (*src != 0) {
        *(dest++) = *(src++);
    }
    
    *dest = 0;

    return dest_base;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char * dest_base = dest;

    while (*src != 0 && n--) {
        *(dest++) = *(src++);
    }

    *dest = 0;
    
    return dest_base;
}

size_t strlen(const char *str) {
    const char *cursor;

    for (cursor = str; *cursor != 0; ++cursor);

    return cursor - str;
}

void *memcpy(void *dest, const void *source, size_t n) {
    // longword
    u64_t *dest_longwords = (u64_t *)dest;
    const u64_t *source_longwords = (const u64_t *)source;

    size_t longwords = n / 8;
    size_t chars = n % 8;

    while (longwords--) {
        *(dest_longwords++) = *(source_longwords++);
    }

    char *dest_chars = (char *)dest_longwords;
    const char *source_chars = (const char *)source_longwords;

    while (chars--) {
        *(dest_chars++) = *(source_chars++);
    }

    return dest;
}

void *memset(void *dest, u8_t value, size_t n) {
    u8_t *dest_bytes = (u8_t *)dest;

    while (n--) {
        *(dest_bytes++) = value;
    }

    return dest;
}

void reverse(char *str) {
    size_t len = strlen(str);
    char *end = str + len - 1;

    while (end > str) {
        char temp = *end;
        *(end--) = *str;
        *(str++) = temp;
    }
}

int itoa(s64_t value, char *str, u64_t base) {
    size_t i = 0;
    int isNegative = 0;

    if (base <= 1) {
        // Invalid base
        return 0;
    }

    // Explicitly handle 0
    if (value == 0) {
        str[0] = '0';
        str[1] = 0;
        return 1;
    }

    // In standard itoa(), negative numbers are handled only with  
    // base 10. Otherwise numbers are considered unsigned. 
    if (value < 0 && base == 10) 
    { 
        isNegative = 1; 
        value = -value; 
    }

    // Process individual digits 
    while (value != 0) 
    { 
        s64_t rem = value % base; 
        str[i++] = (rem > 9) ? (rem-10) + 'A' : rem + '0'; 
        value = value / base; 
    }

    // If number is negative, append '-' 
    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = 0;

    reverse(str);

    return 1;
}

char *ljust(char *str, size_t n, char pad_char) {
    // Justify a numeric string with 0s
    // We'll assume the buffer is long enough
    size_t length = strlen(str);

    if (length < n) {
        size_t pad = n - length;
        
        // Shift everything including the null terminator
        char *last = str + length;
        char *shifted = str + length + pad;

        // Shift the string right by `pad` to make room for zeros
        while (last >= str) {
            *(shifted--) = *(last--);
        }

        for (size_t i = 0; i < pad; ++i) {
            str[i] = pad_char;
        }
    }

    return str;
}

void ptr_to_hex(void *ptr, char *buf) {
    buf[0] = '0';
    buf[1] = 'x';
    
    itoa((s64_t)ptr, buf + 2, 16);

    ljust(buf + 2, 12, '0');
}
