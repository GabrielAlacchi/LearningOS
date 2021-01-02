;
; Load More Code
;
; boot.asm
;

; Set Program Origin
[org 0x7C00]

; 16-bit Mode
[bits 16]

NUM_EXTRA_BOOTLOADER_SECTORS: equ 2

KERNEL_START: equ 0x8200

; 0x500 should be available to store the memory map
MEMORY_MAP_BASE: equ 0x500

; Initialize the base pointer and the stack pointer
; The initial values should be fine for what we've done so far,
; but it's better to do it explicitly
mov bp, 0x7B00
mov sp, bp

; Before we do anything else, we want to save the ID of the boot
; drive, which the BIOS stores in register dl. We can offload this
; to a specific location in memory
mov byte[boot_drive], dl

; Load other bootloader sectors
mov bx, 0x7E00 ; es:bx = 0x0000:0x9000 = 0x09000
mov cl, 0x2 ; Starting Sector
mov dh, NUM_EXTRA_BOOTLOADER_SECTORS ; read next disk sectors
; the bios sets 'dl' for our boot disk number
; if you have trouble, use the '-fda' flag: 'qemu -fda file.bin'
call disk_load

mov bx, loading_kernel
call print_bios

mov bx, 0x8200
call print_hex_bios

mov bx, nl
call print_bios

mov bx, KERNEL_START - 0x200
add cl, NUM_EXTRA_BOOTLOADER_SECTORS - 1

kernel_load_loop:
    add bx, 0x200
    add cl, 1
    mov dh, 1
    call disk_load

    cmp ax, 0
    je kernel_load_loop

call load_memory_map

call elevate_bios

loading_kernel:
db `Loading Kernel at `, 0
nl:
db `\r\n`, 0

; Infinite Loop
bootsector_hold:
jmp $               ; Infinite loop

; INCLUDES
%include "real_mode/print.asm"
%include "real_mode/print_hex.asm"
%include "real_mode/disk_load.asm"
%include "real_mode/gdt.asm"
%include "real_mode/elevate.asm"
%include "real_mode/load_memory_map.asm"

; DATA STORAGE AREA

; Boot drive storage
boot_drive:                     db 0x00

; Pad boot sector for magic number
times 510 - ($ - $$) db 0x00

; Magic number
dw 0xAA55

bootsector_extended:

[bits 32]

begin_protected:

call clear_protected

call detect_lm_protected

call init_pt_protected

call elevate_protected

jmp $       ; Infinite Loop

; INCLUDE protected-mode functions
%include "protected_mode/clear.asm"
%include "protected_mode/print.asm"
%include "protected_mode/detect_lm.asm"
%include "protected_mode/init_pt.asm"
%include "protected_mode/gdt.asm"
%include "protected_mode/elevate.asm"

; Define necessary constants
vga_start:                  equ 0x000B8000
vga_extent:                 equ 80 * 25 * 2             ; VGA Memory is 80 chars wide by 25 chars tall (one char is 2 bytes)
style_wb:                   equ 0x0F

; Fill with zeros to the end of the sector
times 512 - ($ - bootsector_extended) db 0x00

; Long mode code written in sector 4
begin_long_mode:

[bits 64]

mov rdi, style_blue
call clear_long

mov rdi, style_blue
mov rsi, long_mode_note
call print_long

call KERNEL_START

jmp $

%include "long_mode/clear.asm"
%include "long_mode/print.asm"

long_mode_note:                 db `Now running in fully-enabled, 64-bit long mode!`, 0
style_blue:                     equ 0x1F

times 512 - ($ - begin_long_mode) db 0x00
