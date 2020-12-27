; Clear the VGA in 32 bit protected mode

[bits 32]
clear_protected:
    ; Save all registers
    pushad

    mov ebx, vga_extent 
    mov ecx, vga_start
    mov edx, 0

    clear_protected_loop:
        ; while edx < ebx 
        cmp edx, ebx
        jge clear_protected_done

        ; Free edx to use later
        push edx

        ; Move character to al style to ah
        mov al, space_char
        mov ah, style_wb

        ; Add edx to vga_start offset
        add edx, ecx
        mov word[edx], ax

        ; Restore eax
        pop edx

        ; Increment counter
        add edx, 2

        jmp clear_protected_loop
    
clear_protected_done:
    popad
    ret


space_char: equ ` `
