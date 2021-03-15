#include <types.h>

#include <mm/page.h>
#include <mm/page_alloc.h>
#include <mm/buddy_alloc.h>

#include <cpu/atomic.h>


volatile page_info_t *__freelist_head;


static void __init_general_pages(phys_addr_t buddy_start) {
    // Start at PAGE_SIZE, the first page of physical memory is ignored.
    size_t page = PAGE_SIZE, idx = 1;

    __freelist_head = &global_page_map[1];

    while (page < DMA_ZONE_BEGIN) {
        global_page_map[idx].flags |= PAGE_FREELIST;
        global_page_map[idx].freelist_info.next_free = &global_page_map[idx + 1];

        page += PAGE_SIZE;
        idx += 1;
    }

    page_info_t *first_pkpage = NULL;
    page_info_t *kpage = NULL;

    // Allocate pages between the end of the kernel and the beginning of the buddy allocator.
    for (size_t pk_page = last_kernel_page + PAGE_SIZE; pk_page < buddy_start; pk_page += PAGE_SIZE) {
        kpage = page_info(pk_page);

        if (first_pkpage == NULL) {
            first_pkpage = kpage;
        }
        
        kpage->flags |= PAGE_FREELIST;
        kpage->freelist_info.next_free = page_info(pk_page + PAGE_SIZE);
    }

    global_page_map[idx - 1].freelist_info.next_free = first_pkpage;
}


void page_alloc_init() {
    // Set up the upper mem buddy allocator (after kernel).
    size_t buddy_start = last_kernel_page + PAGE_SIZE;
    // Align with the next MAX ORDER block.
    buddy_start = (buddy_start + MASK_FOR_FIRST_N_BITS(PAGE_ORDER + MAX_ORDER)) & (~MASK_FOR_FIRST_N_BITS(PAGE_ORDER + MAX_ORDER));

    // TODO: Call the buddy allocator for this.
    page_info_t *page = page_info(buddy_start);

    __init_general_pages(buddy_start);
}

// Allocate and free a single page from the general reserved region.
phys_addr_t pfa_alloc_page() {
    // Lock free list removal. Since we're in the special case where we only add and remove blocks at the head of the linked list
    // it's quite easy to implement this in a lock free way using atomic primitives.
    page_info_t *head = __freelist_head;

    if (unlikely(head == NULL)) {
        return NULL;
    }

    while (1) {
        // Try setting __freelist_head to next_head atomically (unlinking it).
        // If this fails keep trying again.
        page_info_t *next_head = head->freelist_info.next_free;
        page_info_t *returned_head = cmpxchgq((u64_t*)&__freelist_head, (u64_t)head, (u64_t)next_head);
        if (likely(returned_head == head)) {
            reference_page(head);
            return page_address_from_info(head);
        }

        head = returned_head;
    }
}

void _pfa_free_page(phys_addr_t addr) {
    page_info_t *new_head = page_info(addr);
    page_info_t *current_head = __freelist_head;
    
    while (1) {
        new_head->freelist_info.next_free = current_head;
        // Try adding new_head to the list atomically, if failed then try again.
        page_info_t *returned_head = cmpxchgq((u64_t*)&__freelist_head, (u64_t)current_head, (u64_t)new_head);
        if (likely(returned_head == current_head)) {
            return;
        }

        current_head = returned_head;
    }
}
