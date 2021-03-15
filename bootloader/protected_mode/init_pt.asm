;
; Paging
;
; init_pt.asm
;

; Initialize the page table
; 
; The Page Table has 4 components which will be mapped as follows:
;
; PML4T -> 0x1000 (Page Map Level 4 Table)
; PDPT  -> 0x2000 (Page Directory Pointer Table)
; PDT   -> 0x3000 (Page Directory Table)
; PT    -> 0x4000 (Page table)
;
; Clear the memory in those areas and then set up the page table structure

; Each table in the page table has 512 entries, all of which are 8 bytes
; (one quadword or 64 bits) long. In this step we'll be identity mapping
; ONLY the lowest 2 MB of memory, since this is all we need for now. Note
; that this only requires one page table, so the upper 511 entries in the
; PML4T, PDPT, and PDT will all be NULL.

; Once we have the zeroth address in all pointing to our page table, we
; will need to create a identity map, which will point each virtual page
; to the physical page accessed with that address. Note that in the x86_64
; architecture, a page is addressed using 12 bits, which corresponds to
; 4096 addressible bytes (4KB). Remember this, it'll be important later.

; Another thing to note is that we're in protected mode now, which grants
; us access to fancy CISC instructions like rep, stosd, and loop. I'll
; explain what each of these do a bit later in the file.

[bits 32]

init_pt_protected:
    pushad

    ; The goal here is to do the following
    ; Identity Map the bottom 2MB of memory

    ; And map the first 2GB of memory into the last 2GB of the virtual address space
    ; This can be achieved with 2MB pages.

    ; We'll just need to set up PML4[0] -> some P3, P3[0] -> some P2, then P2[0] can be mapped to 0x0 with the
    ; huge page bit present. This will make a large 2MB page.
    ; We'll also map PML4[511] -> another P3 with P3[510] and P3[511] mapped to complete page directories
    ; filled with 2MB pages.

    ; Clear the memory area using rep stosd
    ;
    ; eax is the value to write, edi is the start address, and ecx is the
    ; number of repetitions to perform.

    mov edi, page_tables_begin  ; Set the base address for rep stosd. Our page tables go from
                                ; 0x1000 to 0x4FFF, so we want to start at 0x1000

    mov cr3, edi                ; Save the PML4T start address in cr3. This will save us time later
                                ; because cr3 is what the CPU uses to locate the page table entries

    xor eax, eax                ; Set eax to 0. We want to zero out the page tables to start with.

    mov ecx, 5 * 1024           ; Repeat 5 * 1024 times. Since each page table is 4096 bytes, and we're
                                ; writing 4 bytes each repetition, this will zero out all 5 page tables

    rep stosd                   ; Now actually zero out the page table entries

    ; Set edi back to PML4T[0]
    mov edi, cr3

    ; We'll use the flags 0x3 which makes the memory "present" and "writable"
    ; For mapping to huge pages we'll use flags 0x83 which toggles 7 along with the same flags as above.

    ; Set up the first entry of the page table for the first 2MB identity map
    mov dword[edi], page_tables_begin + 0x1003 ; Set PML4T[0] to address 0x2000 which is P3[0] with flags 0x0003
    add edi, 0xFF8 ; Move to PML4T[511]
    mov dword[edi], page_tables_begin + 0x3003 ; Set PML4T[511] to address 0x4000 which is the other P3 for the 2GB mapping.
    add edi, 0x8 ; Move to the P3[0]
    mov dword[edi], page_tables_begin + 0x2003 ; Set P3[0] to address 0x3000 which is P2[0]
    add edi, 0x1000 ; Move to the next page table
    
    push edi
    push ebx

    xor ebx, ebx
    call fill_huge_pages

    pop ebx
    pop edi

    add edi, 0x1FF0 ; Move to the latter P3 at index 510
    mov dword[edi], page_tables_begin + 0x4003 ; Point to the page directory at 0x5000
    add edi, 0x8 ; Move the P3[511]
    mov dword[edi], page_tables_begin + 0x5003 ; Point to the page directory at 0x6000

    mov edi, page_tables_begin + 0x4000
    xor ebx, ebx

    call fill_huge_pages

    mov edi, page_tables_begin + 0x5000
    mov ebx, 0x40000000

    call fill_huge_pages

    ; Set up PAE paging, but don't enable it quite yet
    ;
    ; Here we're basically telling the CPU that we want to use paging, but not quite yet.
    ; We're enabling the feature, but not using it.
    mov eax, cr4
    or eax, 1 << 5               ; Set the PAE-bit, which is the 5th bit
    mov cr4, eax

    ; Now we should have a page table that identities maps the lowest 2MB of physical memory into
    ; virtual memory!
    popad
    ret

; edi should store the base address of the page directory to fill
; ebx should store the base physical address the first huge page maps to
fill_huge_pages:
    ; Set the flags for 10000011 (bit 7 is huge page)
    or ebx, 0x83
    mov ecx, 512 ; Do this operation 512 times

    page_fill_loop:
        mov dword[edi], ebx
        add ebx, 0x200000 ; Each huge page is 2MB
        add edi, 8 ; Increment page table location by 8

        loop page_fill_loop ; Decrement ecx and loop again

    ret

SECTION pages.bss
align 0x1000
page_tables_begin:
    resb 0x6000
page_tables_end:
