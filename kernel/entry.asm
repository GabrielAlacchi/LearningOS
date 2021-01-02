[bits 64]
[extern main]

global _start
_start:
    ; Set up the stack in a safe place (for now at 2MB but we'll work on improving this in future work).
    mov rbp, 0xFFFFFFFFC0000000
    mov rsp, 0xFFFFFFFFC0000000

    call main
    jmp $
