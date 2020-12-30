
#include <cpu/isr.h>
#include <types.h>
#include <mm/bitmap.h>
#include <mm/boot_mmap.h>
#include <mm.h>
#include <utility/strings.h>
#include <driver/vga.h>

int main() {
    char buf[20];

    set_cursor_pos(0, 0);
    clearwin(COLOR_WHT, COLOR_BLK);
        
    println("Kernel Initialized"); 
    println("Setting up the IDT");

    isr_install();
    puts("\n\n");
    println("--- E820 Boot Map ---");
    print_boot_mmap();

    // Test Virtual Memory Mapping
    const phys_addr_t kernel_limit = get_kernel_load_limit();
    ptr_to_hex(kernel_limit, buf);

    puts("\nKernel Ends at: ");
    println(buf);

    bitmap_t bitmap;

    u8_t *bitmap_base = (u8_t *)reserve_physmem_region(2);

    bmp_init(&bitmap, bitmap_base, (1 << 12) * 8);
    
    bmp_set_bit(&bitmap, 102, 1);

    const u8_t bit102 = bmp_get_bit(&bitmap, 102);
    
    return 0;
}
