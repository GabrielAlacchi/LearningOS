#include <cpu/isr.h>
#include <types.h>
#include <mm.h>
#include <mm/buddy_alloc.h>
#include <mm/phys_alloc.h>
#include <mm/vm.h>
#include <utility/strings.h>
#include <driver/vga.h>

int kernel_main() {
    set_cursor_pos(0, 0);
    clearwin(COLOR_WHT, COLOR_BLK);
        
    kprintln("Kernel Initialized"); 
    kprintln("Setting up the IDT");

    isr_install();
    kputs("\n\n");
    kprintln("--- E820 Boot Map ---");
    print_boot_mmap();

    kputs("\r\n\n");

    mm_init();

    vmzone_extend(64, VM_ALLOW_WRITE, VMZONE_KERNEL_HEAP);
    vmzone_extend(64, VM_ALLOW_WRITE, VMZONE_KERNEL_HEAP);

    vmzone_shrink(72, VMZONE_KERNEL_HEAP);

    return 0;
}
