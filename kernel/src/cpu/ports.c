#include <cpu/ports.h>

// Read a byte from a port
u8_t byte_in(u16_t port) {
    u8_t result;

    __asm__ volatile ("in %%dx, %%al" : "=a" (result) : "d" (port));
    return result;
}

// Write a byte to a port
void byte_out(u16_t port, u8_t data) {
    __asm__ volatile ("out %%al, %%dx" : : "a" (data), "d" (port));
}
