#ifndef __CPU_MSR_H
#define __CPU_MSR_H

// Enables CR0.WP for write protection and EFER.NXE for execution protection.
extern void enable_paging_protection_bits();

#endif
