; Print implementation in 32 bit protected mode
; we no longer have BIOS print capabilities
;

[bits 32]

; Simple 32-bit print
print_protected:
    pushad
    mov edx, vga_start

    print_protected_loop:
        ; If char == \0, string is done
        cmp byte[esi], 0
        je print_protected_done

        ; Move character to al, style to ah
        mov al, byte[esi]
        mov ah, style_wb

        mov word[edx], ax
        add esi, 1
        add edx, 2

        jmp print_protected_loop
    
print_protected_done:
    popad
    ret