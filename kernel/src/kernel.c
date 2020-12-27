#define VGA_START 0xB8000
#define VGA_EXTENT 80 * 25

#define STYLE_WB 0x0F

#include <cpu/isr.h>
#include <types.h>
#include <mm/boot_mmap.h>
#include <utility/strings.h>
#include <driver/vga.h>

int main() {
    set_cursor_pos(0, 0);
    clearwin(COLOR_WHT, COLOR_BLK);
        
    println("Kernel Initialized"); 
    println("Setting up the IDT");

    isr_install();

    return 0;
}
