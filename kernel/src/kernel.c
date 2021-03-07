#include <cpu/isr.h>
#include <types.h>
#include <mm.h>
#include <mm/buddy_alloc.h>
#include <mm/phys_alloc.h>
#include <mm/vm.h>
#include <mm/slab.h>
#include <mm/kmalloc.h>
#include <utility/strings.h>
#include <driver/vga.h>

#include <log.h>


typedef struct {
    u64_t a;
    u64_t b;
} test_obj_t;


int kernel_main() {
    set_cursor_pos(0, 0);
    clearwin(COLOR_WHT, COLOR_BLK);
        
    kprintln("Kernel Initialized"); 
    kprintln("Setting up the IDT");

    isr_install();

    mm_init();

    kputs("\n\n");
    kprintln("--- E820 Boot Map ---");
    print_boot_mmap();

    kputs("\r\n\n");

    vmzone_extend(64, VM_ALLOW_WRITE, VMZONE_KERNEL_HEAP);
    vmzone_extend(64, VM_ALLOW_WRITE, VMZONE_KERNEL_HEAP);

    vmzone_shrink(72, VMZONE_KERNEL_HEAP);

    return 0;
}
