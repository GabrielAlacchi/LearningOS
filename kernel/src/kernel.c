
#include <cpu/isr.h>
#include <types.h>
#include <mm.h>
#include <mm/buddy_alloc.h>
#include <mm/phys_alloc.h>
#include <utility/strings.h>
#include <driver/vga.h>

void check_free_integrity() {
    size_t free_bytes = 0;
    char buf[10];
    buddy_allocator_t *allocator = get_allocator();

    size_t order_counts[MAX_ORDER + 1];

    // Verify integrity by counting free bytes with the free list and ensuring they make sense
    for (u8_t order = 0; order <= MAX_ORDER; ++order) {
        order_counts[order] = 0;

        freelist_entry_t *free = allocator->freelists[order];

        while (free->flags & PT_PRESENT) {
            ++order_counts[order];
            free_bytes += 1ul << (order + PAGE_ORDER);

            free = allocator->freelist_pool + free->next_entry;
        }

        itoa(order_counts[order], buf, 10);
        puts(buf);
        puts(" ");
    }

    println("");

    size_t used_freelist_count = 0;

    // Check the freelist pool to see if it makes sense.
    for (size_t i = 0; i < allocator->freelist_pool_size; ++i) {
        if (allocator->freelist_pool[i].flags & PT_PRESENT) {
            used_freelist_count += 1;
        }
    }

    if (allocator->freelist_pool_size - used_freelist_count - 1 == allocator->freelist_space_left) {
        println("Freelist pool size checks out");
    } else {
        println("Freelist space left is incorrect");
    }

    if (allocator->free_space_bytes == free_bytes) {
        println("All checks out");
    } else {
        println("Free spaces are not consistent");
    }
}

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

    puts("\r\n\n");

    mm_init();

    
    phys_addr_t allocs[6];
    
    println("Allocating order 7");
    allocs[0] = phys_alloc(7);
    check_free_integrity();

    check_free_integrity();

    println("Allocating order 6");
    allocs[1] = phys_alloc(6);
    check_free_integrity();

    println("Allocating order 0");
    allocs[2] = phys_alloc(0);
    check_free_integrity();

    println("Allocating order 0");
    allocs[3] = phys_alloc(0);
    check_free_integrity();

    println("Allocating order 1");
    allocs[4] = phys_alloc(1);
    check_free_integrity();

    println("Allocating order 3");
    allocs[5] = phys_alloc(3);
    check_free_integrity();

    println("Freeing first alloc");
    phys_free(allocs[0], 7);
    check_free_integrity();

    println("Freeing second alloc");
    phys_free(allocs[1], 6);
    check_free_integrity();

    println("Allocating order 5");
    allocs[0] = phys_alloc(5);
    check_free_integrity();

    println("Freeing allocs 2-5");
    phys_free(allocs[2], 0);
    check_free_integrity();

    phys_free(allocs[3], 0);
    check_free_integrity();

    phys_free(allocs[4], 1);
    check_free_integrity();

    phys_free(allocs[5], 3);
    check_free_integrity();

    println("Freeing alloc 0");
    phys_free(allocs[0], 5);
    check_free_integrity();

    println("Done");

    size_t free_bytes = 0;

    return 0;
}
