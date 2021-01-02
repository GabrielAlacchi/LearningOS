; Operations for MSRs

global enable_paging_protection_bits
enable_paging_protection_bits:
    push rcx
    push rax

    mov ecx, 0xC0000080 ; EFER MSR
    rdmsr
    or eax, (1 << 11) ; EFER.NXE bit 
    wrmsr

    mov rax, cr0
    or rax, (1 << 16) ; Write protect bit
    mov cr0, rax

    pop rax
    pop rcx

    ret
