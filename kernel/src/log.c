#include <driver/vga.h>
#include <utility/valist.h>
#include <utility/strings.h>
#include <mm/kmalloc.h>


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

u8_t _log_fg_color = COLOR_WHT, _log_bg_color = COLOR_BLK;


void set_color(u8_t fg_color, u8_t bg_color) {
    _log_bg_color = fg_color;
    _log_bg_color = bg_color;
}


void __printk_putchar(char c) {
    kputchar(c, _log_fg_color, _log_bg_color);
}


// Used to parse width and precision in specifiers
const char *__parse_spec_number(const char *num_base, u8_t *num) {
    char buffer[4];
    u8_t idx = 0;

    while (isdigit(*num_base) && idx < 3) {
        buffer[idx++] = *(num_base++);
    }

    if (idx == 0) {
        *num = 0;
        return num_base;
    }

    // Consume the rest of the number (we chop off anything after 3 digits)
    if (idx == 3) {
        while (*num_base && isdigit(*num_base++));
    }

    buffer[idx] = 0;
    *num = (u8_t)atoi(buffer);
    return num_base;
}


const char *__parse_specifier(const char *spec_base, struct __printk_specifier *spec) {
    // Indicates that the spec is invalid until we finish parsing.
    u8_t done = 0;

    memset(spec, 0, sizeof(struct __printk_specifier));

    // Parse any flags and length specifiers
    while (!done) {
        switch (*spec_base++) {
        case '-':
            spec->left_justify = 1;
            break;
        case '0':
            spec->zeros = 1;
            break;
        case '+':
            spec->plus = 1;
            break;
        case ' ':
            spec->space = 1;
            break;
        case '#':
            spec->prefix = 1;
            break;
        case '\0':
            return spec_base - 1;
        default:
            done = 1;
            --spec_base;
            break;
        }
    }

    done = 0;
    spec_base = __parse_spec_number(spec_base, &spec->width);
    if (*spec_base == '.') {
        spec_base = __parse_spec_number(spec_base + 1, &spec->precision);
    }

    if (*spec_base == 'l') {
        if (*(spec_base + 1) == 'l') {
            spec->lengthspec = 'L';
            spec_base += 2;
        } else {
            spec->lengthspec = 'l';
            spec_base += 1;
        }
    } else if (*spec_base == 'h') {
        if (*(spec_base + 1) == 'h') {
            spec->lengthspec = 'H';
            spec_base += 2;
        } else {
            spec->lengthspec = 'l';
            spec_base += 1;
        }
    }

    spec->spec = *spec_base++;
    return spec_base;
}


static void __vprint_putstr(void (*putc)(char), const char *str) {
    while (*str) {
        putc(*(str++));
    }
}


static void __vprint_putsigned(void (*putc)(char), s64_t sint, struct __printk_specifier *spec) {
    u16_t buffer_size = MAX(MAX(24, spec->precision + 4), spec->width + 4);
    char *buffer = kmalloc(buffer_size);

    char *unsigned_part;

    // Add sign (or space) to the number.
    itoa(sint, buffer, 10);
    if (spec->plus && sint >= 0) {
        rjust(buffer, strlen(buffer) + 1, '+');
        unsigned_part = buffer + 1;
    } else if (spec->space && sint >= 0) {
        rjust(buffer, strlen(buffer) + 1, ' ');
        unsigned_part = buffer + 1;
    } else if (sint <= 0) {
        unsigned_part = buffer + 1;
    } else {
        unsigned_part = buffer;
    }

    if (spec->precision > 0) {
        rjust(unsigned_part, spec->precision, '0');
    }

    if (spec->width > 0) {
        if (spec->left_justify) {
            ljust(buffer, spec->width, ' ');
        } else if (spec->precision == 0 && spec->zeros) {
            // If there's a sign then we rjust by width - 1, otherwise rjust by width.
            rjust(unsigned_part, spec->width - (unsigned_part - buffer), '0');
        } else {
            rjust(buffer, spec->width, ' ');
        }
    }

    __vprint_putstr(putc, buffer);
    kfree(buffer);
}


static void __vprint_putunsigned(void (*putc)(char), u64_t uint, struct __printk_specifier *spec, u8_t base) {
    u16_t buffer_size = MAX(MAX(24, spec->precision + 4), spec->width + 4);
    char *buffer = kmalloc(buffer_size);

    utoa(uint, buffer, base);

    char *numeric_part;
    if (spec->prefix) {
        if (base == 8) {
            prepend(buffer, "0o");
            numeric_part = buffer + 2;
        } else if (base == 16) {
            prepend(buffer, "0x");
            numeric_part = buffer + 2;
        }
    } else {
        numeric_part = buffer;
    }

    if (spec->precision > 0) {
        rjust(numeric_part, spec->precision, '0');
    }

    if (spec->width > 0) {
        if (spec->left_justify) {
            ljust(buffer, spec->width, ' ');
        } else if (spec->precision == 0 && spec->zeros) {
            // If there's a sign then we rjust by width - 1, otherwise rjust by width.
            rjust(numeric_part, spec->width - (numeric_part - buffer), '0');
        } else {
            rjust(buffer, spec->width, ' ');
        }
    }

    __vprint_putstr(putc, buffer);
    kfree(buffer); 
}


void __vprintk(const char *fmt, void (*putc)(char), va_list args) {
    size_t len = strlen(fmt);
    struct __printk_specifier spec;
    const char *pastspec;

    size_t idx = 0;
    while (idx < len) {
        if (fmt[idx] == '%') {
            // Peek ahead to see if it's a double %%
            if (fmt[idx + 1] == '%') {
                putc('%');
                idx += 2;
                continue;
            }

            // Parse the specifier
            pastspec = __parse_specifier(fmt + idx + 1, &spec);
            idx = pastspec - fmt;

            u8_t base = 10;

            // Handle special cases of unsigned specifiers.
            switch(spec.spec) {
                case 'p':
                    spec.prefix = 1;
                    spec.precision = 16;
                    spec.lengthspec = 'L';
                case 'x':
                    base = 16;
                    spec.spec = 'u';
                    break;
                case 'o':
                    base = 8;
                    spec.spec = 'u';
                    break;
                default:
                    break;
            }

            switch (spec.spec) {
                case 's':
                    __vprint_putstr(putc, va_arg(args, const char *));
                    break;
                case 'd':
                case 'i':
                    switch (spec.lengthspec) {
                    case 'l':
                        __vprint_putsigned(putc, (s64_t)va_arg(args, long int), &spec);
                        break;
                    case 'L':
                        __vprint_putsigned(putc, (s64_t)va_arg(args, long long int), &spec);
                        break;
                    case 'h':
                        __vprint_putsigned(putc, (s64_t)((short int)va_arg(args, int)), &spec);
                        break;
                    case 'H':
                        __vprint_putsigned(putc, (s64_t)((char)va_arg(args, int)), &spec);
                        break;
                    default:
                        __vprint_putsigned(putc, (s64_t)va_arg(args, int), &spec);
                        break;
                    }
                    break;
                case 'u':
                    switch(spec.lengthspec) {
                        case 'l':
                            __vprint_putunsigned(putc, (u64_t)va_arg(args, long unsigned int), &spec, base);
                            break;
                        case 'L':
                            __vprint_putunsigned(putc, (u64_t)va_arg(args, unsigned long long int), &spec, base);
                            break;
                        case 'h':
                            __vprint_putunsigned(putc, (u64_t)((unsigned short int)va_arg(args, unsigned int)), &spec, base);
                            break;
                        case 'H':
                            __vprint_putunsigned(putc, (u64_t)((unsigned char)va_arg(args, unsigned int)), &spec, base);
                            break;
                        default:
                            __vprint_putunsigned(putc, (u64_t)va_arg(args, unsigned int), &spec, base);
                            break;
                    }                
                    break;
                default:
                    break;
            }
        } else {
            putc(fmt[idx++]);
        }
    }
}


void vprintk(const char *fmt, void (*putc)(char), ...) {
    va_list ap;
    va_start(ap, putc);

    __vprintk(fmt, putc, ap);

    va_end(ap);
}


void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    __vprintk(fmt, __printk_putchar, ap);

    va_end(ap);
}
