; Use the BIOS Extended Memory Map Routine to 
; load a map of memory after 1MB.
;
; This is useful to pass into the kernel at startup. Since we're using the bios we should call this 
; prior to getting into protected & long mode.

; For reference https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_EAX_.3D_0xE820

[bits 16]
load_memory_map:
    pushad

    ; Store the entries at MEMORY_MAP_BASE
    mov di, MEMORY_MAP_BASE

    ; Clear EBX
    xor ebx, ebx

    ; The magic number
    mov edx, 0x534D4150
    
    load_memory_map_loop:
        ; Read memory map entry
        mov eax, 0xE820
        mov ecx, MEMORY_MAP_ENTRY_SIZE

        ; Memory map entry read
        int 0x15

        ; Carry flag or EBX = 0 indicates that we're done
        jc load_memory_map_done

        cmp ebx, 0
        je load_memory_map_done

        add di, MEMORY_MAP_ENTRY_SIZE

        jmp load_memory_map_loop

load_memory_map_done:
    ; We zero out the last entry of the memory map on purpose so that the kernel knows to stop
    ; There is no valid fully 0 memory map entry returned by the BIOS so this is distinctive from a real one
    mov ax, 24

    lmm_clear_loop:
        dec ax

        mov bx, ax
        add bx, di
        mov byte[bx], 0

        cmp ax, 0
        jge lmm_clear_loop

    lmm_clear_done:
        popad 
        ret


MEMORY_MAP_ENTRY_SIZE: equ 24
