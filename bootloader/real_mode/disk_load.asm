; load 'dh' sectors from drive 'dl' into ES:BX

disk_load:
    pusha

    ; reading from disk requires setting specific values in all registers
    ; so we will overwrite our input parameters from 'dx'. Let's save it
    ; to the stack for later use.
    push dx

    mov ah, 0x02 ; ah <- int 0x13 function. 0x02 = 'read'
    mov al, dh   ; al <- number of sectors to read (0x01 .. 0x80)
    mov cl, 0x02 ; cl <- sector (0x01 .. 0x11)
                 ; 0x01 is our boot sector, 0x02 is the first 'available' sector
    mov ch, 0x00 ; ch <- cylinder (0x0 .. 0x3FF, upper 2 bits in 'cl')
    ; dl <- driver number. Our caller sets it as a parameter and gets it from BIOS
    mov dh, 0x00 ; dh <- head number (0x0 .. 0xF)

    ; [es:bx] <- pointer to buffer where data will be stored
    ; caller sets it up for us, and it is usually the standard location for int 13h
    int 0x13       ; BIOS interrupt
    jc disk_error  ; if error (stored in carry bit)

    pop dx

    cmp al, dh     ; BIOS also sets 'al' to # of sectors read. Compare it.
    jne sectors_error
    popa
    ret


disk_error:
    mov bx, DISK_ERROR
    call print_bios
    mov dh, ah
    call print_hex_bios
    jmp disk_loop

sectors_error:
    mov bx, SECTORS_ERROR
    call print_bios

disk_loop:
    jmp $

DISK_ERROR:
    db "Disk read error\r\n", 0
SECTORS_ERROR:
    db "Incorrect number of sectors read\r\n", 0
