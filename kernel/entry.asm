[bits 64]
[extern main]

_start:
    ; Set up the stack in a safe place (for now at 2MB but we'll work on improving this in future work).
    mov rbp, 0x1EE000
    mov rsp, 0x1EE000

    call main
    jmp $
