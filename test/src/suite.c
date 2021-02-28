// Simple stupid testing framework which runs in user space.
// Emulates physical memory by MMAPing a pool of memory

#include <mm.h>
#include <mm/boot_mmap.h>
#include <multiboot2.h>

#include <suite.h>
#include <setup/setup_bootinfo.h>


/* These functions will be used to initialize
   and clean resources up after each test run */
int suite_setup() {
    // 0 out the physical memory
    memset(__test_physical_mem, 0, PHYS_MEM_SIZE);

    setup_bootinfo();

    return 0;
}

int suite_teardown() {
    return 0;
}
