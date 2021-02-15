#include <utility/strings.h>
#include <utility/math.h>
#include <mm.h>
#include <mm/vm.h>
#include <mm/vmzone.h>
#include <mm/phys_alloc.h>

static vmzone_t zones[VMZONE_NUMBER_OF_ZONES];

// Cache the PDPT entries of the sensitive and normal virtual memory regions.
// All vm zones are contained within these sub regions.
static page_table_t *sensitive_mem_pdpt = NULL;
static page_table_t *normal_mem_pdpt = NULL;

void __define_vmzone(virt_addr_t base, size_t zone_size_gb, u16_t vmzone, u16_t flags)
{
    zones[vmzone].start_address = base;
    zones[vmzone].end_address = base + (zone_size_gb << 30);
    zones[vmzone].flags = flags;
    zones[vmzone].cursor_addr = base;

    zones[vmzone].vm_flags = VM_ALLOW_WRITE;

    if (flags & VMZFLAG_ALLOW_EXECUTE) {
        zones[vmzone].vm_flags |= VM_ALLOW_EXEC;
    }
}

void vmzone_init()
{
    memset(zones, 0, sizeof(vmzone_t) * VMZONE_NUMBER_OF_ZONES);

    // 8GB zone for the kernel heap (way more than enough but we have plently of address space to go around).
    __define_vmzone(KERNEL_SENSITIVE_MEM, 8, VMZONE_KERNEL_HEAP, VMZFLAG_CONTIGUOUS);

    // Another 8GB zone for larger general (non contiguous) kernel allocations.
    __define_vmzone(KERNEL_SENSITIVE_MEM + (8ul << 30), 8, VMZONE_KERNEL_ALLOC, 0);

    // One contiguous 1GB zone for storing buddy freelist allocations. This is overkill in terms of
    // address space size but it's cleaner to have 1 PDPT entry per zone.
    __define_vmzone(KERNEL_NORMAL_MEM, 1, VMZONE_BUDDY_MEM, VMZFLAG_CONTIGUOUS);

    // Give the rest of the KERNEL_NORMAL_MEM_ZONE, we allow execute because some interrupt code
    // may need to be mapped into this zone.
    __define_vmzone(KERNEL_NORMAL_MEM + (1ul << 30), 511, VMZONE_USER_SHARED, VMZFLAG_ALLOW_EXECUTE);
}

vmzone_t *vmzone_info(u16_t vmzone)
{
    if (vmzone >= VMZONE_NUMBER_OF_ZONES)
    {
        return NULL;
    }

    return &zones[vmzone];
}

void vmspace_init(page_table_t *pml4t, u8_t flags)
{
    u16_t normal_mem_offset = pml4t_offset(KERNEL_NORMAL_MEM);
    u16_t sensitive_mem_offset = pml4t_offset(KERNEL_SENSITIVE_MEM);

    // Initialize 3rd level page tables for the KERNEL_SENSITIVE_MEM and KERNEL_NORMAL_MEM sections
    if (sensitive_mem_pdpt == NULL) {
        if (pml4t->entries[sensitive_mem_offset] & PT_PRESENT) {
            sensitive_mem_pdpt = kphys_addr_for_entry(pml4t->entries[sensitive_mem_offset]);
        } else {
            sensitive_mem_pdpt = KPHYS_ADDR(_alloc_page_tables(1, flags & VM_ALLOC_EARLY));
        }
    }

    if (!(pml4t->entries[sensitive_mem_offset] & PT_PRESENT)) {
        pml4t->entries[sensitive_mem_offset] |= vm_pt_entry_create(phys_addr_for_kphys(sensitive_mem_pdpt), VM_ALLOW_EXEC | VM_ALLOW_WRITE);
    }

    if (normal_mem_pdpt == NULL) {
        if (pml4t->entries[normal_mem_offset] & PT_PRESENT) {
            normal_mem_pdpt = kphys_addr_for_entry(pml4t->entries[normal_mem_offset]);
        } else {
            normal_mem_pdpt = KPHYS_ADDR(_alloc_page_tables(1, flags & VM_ALLOC_EARLY));
        }
    }

    if (!(pml4t->entries[normal_mem_offset] & PT_PRESENT)) {
        pml4t->entries[normal_mem_offset] |= vm_pt_entry_create(phys_addr_for_kphys(normal_mem_pdpt), VM_ALLOW_EXEC | VM_ALLOW_WRITE);
    }

    // Initialize the final 2GB memory mapping if necessary
    if (!(sensitive_mem_pdpt->entries[511] & PT_PRESENT)) {
        page_table_t * pt = KPHYS_ADDR(_alloc_page_tables(1, flags & VM_ALLOC_EARLY));
        vm_space_pt_init(pml4t, pt, KERNEL_VMA, VM_ALLOW_WRITE | VM_ALLOW_EXEC, 2);
        
        phys_addr_t phys_addr = 0;
        
        for (size_t i = 0; i < 512; ++i) {
            pt->entries[i] |= vm_pt_entry_create(phys_addr, VM_ALLOW_WRITE | VM_ALLOW_EXEC | VM_HUGEPAGE);
            // One huge page is equivalent to 512 pages (2MB)
            phys_addr += 512ul << PAGE_ORDER;
        }
    }

    // Set up page tables for the base address of every single vm zone if necessary
    for (u16_t zone_idx = 0; zone_idx < VMZONE_NUMBER_OF_ZONES; ++zone_idx) {
        vmzone_t *zone = vmzone_info(zone_idx);

        if (zone->flags & VMZFLAG_INITIALIZED) {
            continue;
        }

        // Assume we need to initialize a page directory table and a page table for each zone.
        page_table_t *pdpt = KPHYS_ADDR(_alloc_page_tables(2, flags & VM_ALLOC_EARLY));
        pdpt->entries[0] |= vm_pt_entry_create(phys_addr_for_kphys(pdpt + 1), zone->vm_flags);

        zone->flags |= VMZFLAG_INITIALIZED;
    }
}