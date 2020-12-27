; 
; Jump into 32 bit protected mode
;
; elevate.asm
;

[bits 16]

elevate_bios:
    ; We need to disable interrupts because elevating to 32-bit mode
    ; causes the CPU to go a little crazy. We do this with the 'cli'
    ; command
    cli

    ; Load in the 32-bit GDT
    lgdt [gdt_32_descriptor]

    ; Enable 32-bit mode by setting bit 0 of the control
    ; register. 
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ; Do a far jump to clear the pipeline
    jmp code_seg:init_pm

[bits 32]
init_pm:
    ; Now we're in 32-bit mode we need to initialize the segment
    ; registers.
    mov ax, data_seg
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Initialize a fresh stack at hex 90000
    mov ebp, 0x90000
    mov esp, ebp

    jmp begin_protected
