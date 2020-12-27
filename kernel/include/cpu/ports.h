#ifndef _CPU_PORTS_H
#define _CPU_PORTS_H

#include <types.h>

// Read a byte from a port
u8_t byte_in(u16_t port);

// Write a byte to a port
void byte_out(u16_t port, u8_t data);

#endif
