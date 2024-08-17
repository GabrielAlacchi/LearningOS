#include <utility/math.h>
#include <utility/strings.h>
#include <mm/vm.h>
#include <mm/vmzone.h>
#include <mm/page_alloc.h>
#include <mm/page.h>

#define PAGE_ADDRESS_MASK ((MASK_FOR_FIRST_N_BITS(40)) << 12)
#define MASK_UNRESERVED_BITS (~(PAGE_ADDRESS_MASK | (1ul << 63) | MASK_FOR_FIRST_N_BITS(9)))

vmblock_alloc_t pt_virt_allocator;

extern phys_addr_t __KERNEL_PHYSICAL_START;

// PDPTs which are shared by all address spaces. They will be initialized once and then reused by all processes.
page_table_t *kmap_pdpt = NULL, *heap_pdpt = NULL;

static phys_addr_t __early_page_table_alloc() {
    phys_addr_t pt = pfa_alloc_page();
    memset(KPHYS_ADDR(pt), 0, PAGE_SIZE);
    return pt;
}

static inline void __mark_pt_page_info(const phys_addr_t pt_phys, const page_table_t *pt_virt) {
    global_page_map[pt_phys >> PAGE_ORDER].flags |= PAGE_PT;
    global_page_map[pt_phys >> PAGE_ORDER].pt_virt = pt_virt;
}

static inline void __early_pt_map(page_table_t *ptzone_pt, const phys_addr_t pt_address, u16_t offset) {
    // Map the page table
    ptzone_pt->entries[offset] = vm_pt_entry_create(pt_address, VM_ALLOW_WRITE);
    page_info_t *pt_page = page_info(pt_address);

    pt_page->flags |= PAGE_PT;
    pt_page->pt_virt = VM_REGION_PT + ((size_t)offset << PAGE_ORDER);
}

static inline pt_entry_t __pt_entry_for_blk_offset(virt_addr_t origin, virt_addr_t dest) {
    if (dest > origin) {
        return dest - origin;
    } else {
        return (dest - origin) | PT_OFFSET_NEGATIVE;
    }
}

// Creates the first virtual memory space
static phys_addr_t __create_initial_vmspace() {
    phys_addr_t pml4t_phys = __early_page_table_alloc();
    phys_addr_t map_pdpt_phys = __early_page_table_alloc();
    phys_addr_t heap_pdpt_phys = __early_page_table_alloc();

    page_table_t *pml4t = KPHYS_ADDR(pml4t_phys);
    kmap_pdpt = KPHYS_ADDR(map_pdpt_phys);
    heap_pdpt = KPHYS_ADDR(heap_pdpt_phys);

    memset(pml4t, 0, sizeof(page_table_t));
    memset(kmap_pdpt, 0, sizeof(page_table_t));
    memset(heap_pdpt, 0, sizeof(page_table_t));

    // Kernel code is mapped into this zone, allow exec
    pml4t->entries[pml4t_offset(VM_REGION_PT)] = vm_pt_entry_create(map_pdpt_phys, VM_ALLOW_EXEC | VM_ALLOW_WRITE);
    pml4t->entries[pml4t_offset(VM_REGION_HEAP)] = vm_pt_entry_create(heap_pdpt_phys, VM_ALLOW_WRITE);

    page_info_t *kernel_page = page_info(first_kernel_page);
    phys_addr_t kmap_pdt_phys = __early_page_table_alloc();
    page_table_t *kmap_pdt = KPHYS_ADDR(kmap_pdt_phys);
    void *kernel_base = KPHYS_ADDR(first_kernel_page);

    kmap_pdpt->entries[pdpt_offset(kernel_base)] = vm_pt_entry_create(kmap_pdt_phys, VM_ALLOW_EXEC | VM_ALLOW_WRITE);
    
    // Now we need to map the PTs into the PT zone.
    phys_addr_t pt_pdt_phys = __early_page_table_alloc(), pt_pt_phys = __early_page_table_alloc();
    page_table_t *pt_pdt = KPHYS_ADDR(pt_pdt_phys), *pt_pt = KPHYS_ADDR(pt_pt_phys);
    kmap_pdpt->entries[pdpt_offset(VM_REGION_PT)] = vm_pt_entry_create(pt_pdt_phys, VM_ALLOW_WRITE);
    pt_pdt->entries[pdt_offset(VM_REGION_PT)] = vm_pt_entry_create(pt_pt_phys, VM_ALLOW_WRITE);
    
    __early_pt_map(pt_pt, pml4t_phys, 0);
    __early_pt_map(pt_pt, kmap_pdt_phys, 1);
    __early_pt_map(pt_pt, heap_pdpt_phys, 2);
    __early_pt_map(pt_pt, kmap_pdt_phys, 3);
    __early_pt_map(pt_pt, pt_pdt_phys, 4);
    __early_pt_map(pt_pt, pt_pt_phys, 5);

    size_t num_pts = pt_offset(KPHYS_ADDR(last_kernel_page)) - pt_offset(KPHYS_ADDR(first_kernel_page)) + 1;
    page_info_t *last_page_to_map = page_info(last_kernel_page);

    // Map the kernel
    for (size_t i = 0; i < num_pts; ++i) {
        phys_addr_t pt = __early_page_table_alloc();
        __early_pt_map(pt_pt, pt, 6 + i);
        page_table_t *pt_page = KPHYS_ADDR(pt);

        size_t max_to_map = MIN(512, last_page_to_map - kernel_page);

        for (size_t j = 0; j < max_to_map; ++j, ++kernel_page) {
            phys_addr_t page_phys = page_address_from_info(kernel_page);
            void *map_to_address = KPHYS_ADDR(page_phys);
            
            u8_t flags = VM_ALLOW_EXEC;
            if (unlikely(!(kernel_page->flags & PAGE_READONLY))) {
                flags |= VM_ALLOW_WRITE;
            }

            pt_page->entries[pt_offset(map_to_address)] = vm_pt_entry_create(page_phys, flags);
        }

        kmap_pdt->entries[pt_offset(first_kernel_page) + i] = vm_pt_entry_create(pt, VM_ALLOW_WRITE | VM_ALLOW_EXEC);
    }

    return pml4t_phys;
}

// Initialize a page table which is a freelist of blocks
static void __allocate_block_pt(vmblock_alloc_t *allocator) {
    // Allocate a page for the PT and map it to an available block in pt_virt_allocator.
    phys_addr_t pt_phys = pfa_alloc_page();
    page_table_t *pdt, *pt = (page_table_t *)vm_block_claim(&pt_virt_allocator, pt_phys, VM_ALLOW_WRITE);
    __mark_pt_page_info(pt_phys, pt);

    pt_entry_t pdpt_entry = allocator->shared_pdpt->entries[pdpt_offset(allocator->next_empty_pt)];

    if (likely(pdpt_entry & PT_PRESENT)) {
        pdt = pt_from_entry(pdpt_entry);
    } else {
        phys_addr_t pdt_phys = pfa_alloc_page();
        pdt = (page_table_t *)vm_block_claim(&pt_virt_allocator, pdt_phys, VM_ALLOW_WRITE);
        memset(pdt, 0, 4096);

        allocator->shared_pdpt->entries[pdpt_offset(allocator->next_empty_pt)] = vm_pt_entry_create(pdt_phys, VM_ALLOW_WRITE);
        __mark_pt_page_info(pdt_phys, pdt);
    }

    memset(pt, 0, 4096);

    u16_t nblocks = 512 / allocator->block_size;
    
    for (u16_t i = 0; i < nblocks - 1; ++i) {
        u32_t offset_to_next = allocator->block_size << PAGE_ORDER;
        pt->entries[i * allocator->block_size] = offset_to_next;
    }

    pdt->entries[pdt_offset(allocator->next_empty_pt)] = vm_pt_entry_create(pt_phys, VM_ALLOW_WRITE);

    if (allocator->last_free_block == NULL) {
        // Point the first free block to the first entry of the new PT we just created
        allocator->first_free_block = allocator->next_empty_pt;
        allocator->first_free_pt = pt;
    } else {
        // Point the former last free block to the first block of the newly allocated PT.
        allocator->last_free_pt->entries[pt_offset(allocator->last_free_block)] = 
            __pt_entry_for_blk_offset(allocator->last_free_block, allocator->next_empty_pt);
    }

    allocator->last_free_block = allocator->next_empty_pt + (511 << PAGE_ORDER);
    allocator->last_free_pt = pt;
    allocator->free_blocks += nblocks;
    allocator->next_empty_pt += (512 << PAGE_ORDER);
}

void vm_init() {
    phys_addr_t init_pml4t = __create_initial_vmspace();
    write_cr3(init_pml4t);

    // Initialize the VM page table allocator
    pt_virt_allocator.block_size = 1;
    pt_virt_allocator.zone_base = VM_REGION_PT;
    pt_virt_allocator.shared_pdpt = kmap_pdpt;
    
    page_table_t *pt = pt_from_entry(pt_virt_allocator.shared_pdpt->entries[pdpt_offset(VM_REGION_PT)]);
    pt = pt_from_entry(pt->entries[pdt_offset(VM_REGION_PT)]);
    
    u16_t first_free_idx = 0;
    while (pt->entries[first_free_idx] & PT_PRESENT) {
        ++first_free_idx;
    }

    pt_virt_allocator.first_free_pt = pt;
    pt_virt_allocator.first_free_block = VM_REGION_PT + (first_free_idx << PAGE_ORDER);
    pt_virt_allocator.next_empty_pt = VM_REGION_PT + (512 << PAGE_ORDER);

    pt_virt_allocator.last_free_block = VM_REGION_HEAP + (511 << PAGE_ORDER);
    pt_virt_allocator.last_free_pt = pt;
}

int vm_check_status(pt_entry_t entry, u8_t flags) {
    if (!(entry & PT_PRESENT))
    {
        return ERR_VM_UNMAPPED;
    }

    if ((flags & VM_ALLOW_WRITE) && !(entry & PT_WRITABLE))
    {
        return ERR_VM_PRIVILEGE;
    }

    if ((flags & VM_ALLOW_USER) && !(entry & PT_USER_ACCESSIBLE))
    {
        return ERR_VM_PRIVILEGE;
    }

    return 0;
}

pt_entry_t vm_pt_entry_create(const phys_addr_t phys_addr, u8_t flags) {
    pt_entry_t entry = PT_PRESENT;

    if (flags & VM_ALLOW_WRITE)
    {
        entry |= PT_WRITABLE;
    }

    if (flags & VM_ALLOW_USER)
    {
        entry |= PT_USER_ACCESSIBLE;
    }

    if (!(flags & VM_ALLOW_EXEC))
    {
        entry |= PT_NO_EXECUTE;
    }

    if (flags & VM_HUGEPAGE)
    {
        entry |= PT_HUGEPAGE;
    }

    return entry | (phys_addr & PAGE_ADDRESS_MASK);
}

static phys_addr_t __alloc_page_table() {
    phys_addr_t frame = pfa_alloc_page();

    // Add a new pt to the zone so there's space for the one we're trying to add
    if (unlikely(pt_virt_allocator.free_blocks <= 2)) {
        __allocate_block_pt(&pt_virt_allocator);
    }

    page_table_t *pt = vm_block_claim(&pt_virt_allocator, frame, VM_ALLOW_WRITE);
    memset(pt, 0, 4096);
    __mark_pt_page_info(frame, pt);

    return frame;
}

// Map a page table entry to a physical address with page protection flags
static inline void __map_phys_page(page_table_t *pt, size_t offset, phys_addr_t phys_addr, u8_t flags) {
    pt_entry_t entry = vm_pt_entry_create(phys_addr, flags);

    // Preserve bits that are not reserved
    pt->entries[offset] &= MASK_UNRESERVED_BITS;
    pt->entries[offset] |= entry;
}

// Follow the offsets of virt_addr to a provided depth. Depth=1 yields page directory pointer. Depth=2, yields the page
// directory table, and depth=3 yields the page table. Along the traversal it will check the status against the given flags
// in case there are any errors.
static page_table_t *__traverse_with_status(
    page_table_t *pml4t,
    virt_addr_t virt_addr,
    u8_t flags,
    u8_t *depth,
    int *error)
{
    page_table_t *current_table = pml4t;
    u8_t current_shift = 12 + 27;

    while (*depth <= 3 && *depth > 0)
    {
        size_t current_offset = ((size_t)virt_addr >> current_shift) & 0x1FF;
        pt_entry_t entry = current_table->entries[current_offset];

        *error = vm_check_status(entry, flags);
        if (*error)
        {
            return current_table;
        }

        current_table = (page_table_t *)kphys_addr_for_entry(entry);
        current_shift -= 9;

        --(*depth);
    }

    return current_table;
}

// Finds the page table for the provided virtual address and allocates any missing pages tables along the way.
static page_table_t *__find_or_allocate_pt(
    page_table_t *pml4t,
    virt_addr_t virt_addr,
    u8_t flags,
    int *error,
    int *allocated_pages)
{
    *allocated_pages = 0;
    u8_t height = 3;
    int traversal_error = 0;
    page_table_t *pt_or_parent = __traverse_with_status(pml4t, virt_addr, flags, &height, &traversal_error);

    if (traversal_error == ERR_VM_UNMAPPED)
    {
        while (height > 0)
        {
            size_t offset = height_offset(virt_addr, height);
            phys_addr_t new_page_table = __alloc_page_table();
            ++(*allocated_pages);
            __map_phys_page(pt_or_parent, offset, new_page_table, flags);

            pt_or_parent = KPHYS_ADDR(new_page_table);

            --height;
        }
    }
    else if (traversal_error > 0)
    {
        *error = traversal_error;
        return NULL;
    }

    *error = 0;
    return pt_or_parent;
}

static page_table_t *__find_pt_or_null(
    page_table_t *pml4t,
    virt_addr_t virt_addr)
{
    page_table_t *table = pml4t;

    u8_t current_shift = PAGE_ORDER + 27;
    for (u8_t i = 0; i < 3; ++i)
    {
        size_t offset = (((size_t)virt_addr) >> current_shift) & 0x1FF;
        pt_entry_t entry = table->entries[offset];

        if (!(entry & PT_PRESENT))
        {
            return NULL;
        }

        table = kphys_addr_for_entry(entry);
        current_shift -= 9;
    }

    return table;
}


int vm_map_pages(page_table_t *pml4t, phys_addr_t phys_base, size_t pages, virt_addr_t virt_base, u8_t flags) {
    size_t offset = pt_offset(virt_base);

    phys_addr_t page = phys_base;
    phys_addr_t end = phys_base + (pages << PAGE_ORDER);

    while (page < end)
    {
        int err;
        int allocated_pages;
        page_table_t *pt = __find_or_allocate_pt(pml4t, virt_base, flags, &err, &allocated_pages);

        if (unlikely(err != 0))
        {
            return err;
        }

        int iterations_left = MIN((end - page) >> PAGE_ORDER, 512 - offset);
        for (int i = 0; i < iterations_left; ++i)
        {
            // Map a page in this page table
            pt->entries[offset + i] = vm_pt_entry_create(page, flags);
            page += PAGE_SIZE;
        }

        offset = 0;
    }

    return 0;
}

virt_addr_t vm_block_claim(vmblock_alloc_t *allocator, const phys_addr_t block_address, u8_t flags) {
    if (unlikely(allocator->free_blocks == 0)) {
        return NULL;
    }

    virt_addr_t block = allocator->first_free_block;
    size_t offset = pt_offset(block);
    pt_entry_t entry = allocator->first_free_pt->entries[offset];

    virt_addr_t next_free_block = next_free_entry(entry, allocator->first_free_block);

    // Map the pages
    for (size_t i = 0; i < allocator->block_size; ++i) {
        allocator->first_free_pt->entries[offset + i] = vm_pt_entry_create(block_address + (i << PAGE_ORDER), flags);
    }

    page_table_t *pt = pt_from_entry(allocator->shared_pdpt->entries[pdpt_offset(next_free_block)]);
    pt = pt_from_entry(pt->entries[pdt_offset(next_free_block)]);

    allocator->first_free_block = next_free_block;
    allocator->first_free_pt = pt;
    allocator->free_blocks -= 1;

    if (unlikely(allocator->free_blocks == 0)) {
        allocator->last_free_block = NULL;
        allocator->last_free_pt = NULL;
    }

    return block;
}

void vm_block_release(vmblock_alloc_t *allocator, virt_addr_t block_address) {
    page_table_t *pt = allocator->shared_pdpt->entries[pdpt_offset(block_address)];
    pt = pt->entries[pdt_offset(block_address)];
    size_t offset = pt_offset(block_address);

    // Drop references to the physical pages
    for (size_t i = 0; i < allocator->block_size; ++i) {
        page_info_t *page = page_for_entry(pt->entries[offset + i]);
        drop_page_reference(page);
    }

    virt_addr_t next_free = allocator->first_free_block;
    if (next_free - block_address >= 0) {
        pt->entries[offset] = next_free - block_address;
    } else {
        pt->entries[offset] = (block_address - next_free) | PT_OFFSET_NEGATIVE;
    }

    allocator->first_free_block = block_address;
    allocator->first_free_pt = pt;
    allocator->free_blocks += 1;
}
