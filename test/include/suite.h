#ifndef __TEST_SUITE_H
#define __TEST_SUITE_H

#ifndef PHYS_MEM_SIZE
#define PHYS_MEM_SIZE (10ul << 20)
#endif

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <types.h>
#include <cmocka.h>
#include <stdio.h>
#include <memory.h>
#include <mm.h>

u8_t __test_physical_mem[PHYS_MEM_SIZE];

int suite_setup();
int suite_teardown();

#endif
