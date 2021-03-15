#include <cpu/isr.h>
#include <types.h>
#include <mm.h>
#include <mm/page.h>
#include <mm/buddy_alloc.h>
#include <mm/phys_alloc.h>
#include <mm/page_alloc.h>
#include <mm/vm.h>
#include <mm/slab.h>
#include <mm/kmalloc.h>
#include <utility/strings.h>
#include <driver/vga.h>
#include <cpu/atomic.h>

#include <log.h>


extern page_info_t *__freelist_head;


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

    printk("sizeof(page_info_t)=%d\n", sizeof(page_info_t));

    printk("last_kernel_page = %#llx\n", last_kernel_page);

    page_info_t *page = page_info(last_kernel_page + 0x1000);
    printk("kernel flag of next page = %hu\n", page->flags & PAGE_KERNEL);

    size_t freelist_pages = 0;
    page = __freelist_head;

    while (page != NULL) {
        freelist_pages += 1;
        page = page->freelist_info.next_free;
    }

    printk("%u available freelist pages\n", freelist_pages);

    return 0;
}
