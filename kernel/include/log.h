#ifndef __LOG_H
#define __LOG_H

#include <types.h>
#include <utility/valist.h>

void set_color(u8_t fg_color, u8_t bg_color);

// Generic vprintk function for extend printk to other output formats.
void vprintk(const char *fmt, void (*putc)(char), ...);
void printk(const char *fmt, ...);

#endif
