#ifndef __SETUP_BOOTINFO_H
#define __SETUP_BOOTINFO_H

#define MEMHOLE_BEGIN 0x9D000
#define MEMHOLE_END   0x100000
#define MEMHOLE_SIZE  (MEMHOLE_END - MEMHOLE_BEGIN)

void setup_bootinfo();

#endif
