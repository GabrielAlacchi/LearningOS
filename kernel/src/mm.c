#include <cpu/msr.h>
#include <driver/vga.h>
#include <mm.h>
#include <mm/phys_alloc.h>
#include <mm/vm.h>
#include <mm/kmalloc.h>

void mm_init() {
    kprintln("Enabling EFER.NXE for execution protection");

    // Enable EFER.NXE bit in EFER MSR.
    enable_paging_protection_bits();

    vm_init();

    // Initialize Physical Allocator
    phys_alloc_init();

    kmalloc_init();
}

virt_addr_t phys_to_kvirt(phys_addr_t phys_addr) {
    return KPHYS_ADDR(phys_addr);
}

phys_addr_t virt_to_phys(virt_addr_t virt_addr) {
    const u64_t pml4t_offset = ((u64_t)virt_addr >> (12 + 27)) & 0x1FF;
    const u64_t pdpt_offset = ((u64_t)virt_addr >> (12 + 18)) & 0x1FF;

    const pt_entry_t pml4t_entry = ((page_table_t *)KERNEL_PML4T)->entries[pml4t_offset];

    if (!(pml4t_entry & PT_PRESENT)) {
        return NULL;
    }

    const virt_addr_t pdpt_address = kphys_addr_for_entry(pml4t_entry);

    const pt_entry_t pdpt_entry = ((page_table_t *)pdpt_address)->entries[pdpt_offset];
    
    if (!(pdpt_entry & PT_PRESENT)) {
        return NULL;
    }

    const u64_t pdt_offset = ((u64_t)virt_addr >> (12 + 9)) & 0x1FF;

    const virt_addr_t pdt_address = kphys_addr_for_entry(pdpt_entry);
    const pt_entry_t pdt_entry = ((page_table_t *)pdt_address)->entries[pdt_offset];

    if (!(pdt_entry & PT_PRESENT)) {
        return NULL;
    }

    if (pdt_entry & PT_HUGEPAGE) {
        return (phys_addr_t)((u64_t)phys_addr_for_entry(pdt_entry) + ((u64_t)virt_addr & 0x1FFFFF));
    }

    const u64_t pt_offset = ((u64_t)virt_addr >> 12) & 0x1FF;

    const virt_addr_t pt_address = kphys_addr_for_entry(pdt_entry);
    const pt_entry_t pt_entry = ((page_table_t *)pt_address)->entries[pt_offset];

    if (!(pt_entry & PT_PRESENT)) {
        return NULL;
    }

    const virt_addr_t page_base_addr = kphys_addr_for_entry(pt_entry);
    return (phys_addr_t)((u64_t)page_base_addr + ((u64_t)virt_addr & 0xFFF));
}
