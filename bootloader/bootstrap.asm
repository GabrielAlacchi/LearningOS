;
; The bootstrap function which is called by GRUB in 32 bit protected mode.
; It will perform the following steps before jumping into the kernel.
;
;   1. Set up paging with higher half addressing
;   2. Initialize the stack for the kernel's init thread.
;
; This file will be outputted as an object file which is then linked into the kernel.
;
; bootstrap.asm
;

; Define necessary constants
vga_start:                  equ 0x000B8000
vga_extent:                 equ 80 * 25 * 2             ; VGA Memory is 80 chars wide by 25 chars tall (one char is 2 bytes)
style_wb:                   equ 0x0F

; Save 2 pages for the stack
KERNEL_INIT_STACK_SIZE: equ 2 << 12

[bits 32]
%include "multiboot.asm"

SECTION .text

global _start
extern main

_start:
  cli

  ; Grub gives us multiboot info in ebx
  mov [multiboot_info_ptr_32], ebx

  call clear_protected

  call detect_lm_protected

  call init_pt_protected

  call elevate_protected

  jmp $       ; Infinite Loop

  ; INCLUDE protected-mode functions
  %include "protected_mode/clear.asm"
  %include "protected_mode/print.asm"
  %include "protected_mode/detect_lm.asm"
  %include "protected_mode/gdt.asm"
  %include "protected_mode/elevate.asm"
  %include "protected_mode/init_pt.asm"

SECTION .data

; Temporarily store the multiboot info pointer as a 32 bit ptr here
multiboot_info_ptr_32:
  dd 0

[bits 64]

extern kernel_main
extern KERNEL_VMA

bootstrap_kernel:
  mov rbp, kernel_init_stack_space
  mov rsp, kernel_init_stack_space

  xor rax, rax
  mov eax, [multiboot_info_ptr_32]

  mov [multiboot_info_ptr], rax

  call kernel_main
  jmp $

SECTION .bss
resb KERNEL_INIT_STACK_SIZE
kernel_init_stack_space:

align 8
global multiboot_info_ptr
multiboot_info_ptr:
  resb 8
