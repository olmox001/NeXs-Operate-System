; kernel_entry.asm - Kernel execution entry point
[BITS 64]

; External C kernel main
extern kernel_main

; Export entry point
global _start

section .text
_start:
    ; DEBUG: Write 'K' (White on Red) to top-left corner
    mov rax, 0xB8000
    mov word [rax], 0x4F4B
    
    ; Disable interrupts
    cli
    
    ; Clear Direction Flag (standard ABI requirement)
    cld
    
    ; Set up stack
    mov rsp, 0x200000
    and rsp, -16
    mov rbp, rsp        ; Initialize RBP for valid stack frame checks
    push rbp            ; Create first frame link (0)
    mov rbp, rsp
    
    ; Enable SSE (standard requirement)
    mov rax, cr0
    and ax, 0xFFFB      ; Clear EM (Bit 2)
    or ax, 0x2          ; Set MP (Bit 1)
    mov cr0, rax
    
    mov rax, cr4
    or ax, 0x600        ; Set OSFXSR (Bit 9) and OSXMMEXCPT (Bit 10)
    mov cr4, rax
    
    ; Load Kernel GDT (Atomic Fix)
    lgdt [gdt64.pointer]
    
    ; Reload Code Segment
    push 0x08
    push .reload_cs
    retfq
.reload_cs:
    ; Reload Data Segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Call kernel
    call kernel_main

    ; If kernel_main returns, loop forever
    cli
    jmp .hang

.hang:
    hlt
    jmp .hang

section .data
align 16
gdt64:
    ; Null Descriptor
    dq 0
    ; Code Descriptor (Offset 0x08)
    ; Present=1, DPL=0, Type=1 (Code), LongMode=1
    dw 0xFFFF       ; Limit low
    dw 0            ; Base low
    db 0            ; Base middle
    db 10011010b    ; Access (Present, Ring0, Code, Exec/Read)
    db 10101111b    ; Flags (LongMode, 4K Granularity)
    db 0            ; Base high
    ; Data Descriptor (Offset 0x10)
    ; Present=1, DPL=0, Type=0 (Data)
    dw 0xFFFF       ; Limit low
    dw 0            ; Base low
    db 0            ; Base middle
    db 10010010b    ; Access (Present, Ring0, Data, Read/Write)
    db 11001111b    ; Flags (4K Granularity)
    db 0            ; Base high

.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .note.GNU-stack noalloc noexec nowrite progbits
