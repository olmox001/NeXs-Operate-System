; ==============================================================================
; trampoline.asm - SMP AP Bootstrap Code (16-bit -> 64-bit)
; ==============================================================================
; This code is loaded at 0x8000 by the BSP.
; It transitions Application Processors (APs) from Real Mode to Long Mode
; and effectively joins the main kernel.

BITS 16
ORG 0x8000

global trampoline_start
global trampoline_end
global trampoline_data

; ==============================================================================
; Header / Data Section (Fixed Offsets)
; ==============================================================================
    jmp trampoline_entry
    align 16

    ; Data accessible by C code at 0x8000 + Offset
    ; Offset 0x10: PML4 Pointer
    pml4_ptr:       dq 0
    ; Offset 0x18: Stack Pointer (Top)
    ap_stack_ptr:   dq 0
    ; Offset 0x20: Code Pointer (C Entry)
    ap_code_ptr:    dq 0

trampoline_entry:
    cli                         ; Disable interrupts immediately

    ; DEBUG: '1' at 0xB8000 (Real Mode Start) - White on Blue
    push ax
    push es
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x1F31 
    pop es
    pop ax
    
    ; 1. Initialize Segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000              ; Stack grows down from 0x8000 (safe for 16-bit)

    ; 2. Load GDT (Protected Mode preparation)
    lgdt [gdt_desc]

    ; 3. Enable Protected Mode (PE bit in CR0)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 4. Jump to 32-bit Protected Mode
    jmp 0x18:protected_mode     ; 0x18 is 32-bit Code in new GDT layout

BITS 32
protected_mode:
    ; 5. Set up 32-bit Data Segments
    mov ax, 0x20                ; 0x20 is 32-bit Data
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; DEBUG: '2' at 0xB8002 (Protected Mode Start) - White on Red
    mov word [0xB8002], 0x4F32

    ; 6. Enable PAE (Physical Address Extension) - Required for Long Mode
    mov eax, cr4
    or eax, 0x20                ; Bit 5 = PAE
    mov cr4, eax

    ; 7. Load Page Table (PML4) - Passed by BSP
    ; We read the CR3 value stored at 'pml4_ptr'
    mov eax, [pml4_ptr]
    mov cr3, eax

    ; 8. Enable Long Mode (LME in EFER MSR)
    mov ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or eax, 0x100               ; Bit 8 = LME
    wrmsr

    ; 9. Enable Paging (PG bit in CR0)
    mov eax, cr0
    or eax, 0x80000000          ; Bit 31 = PG
    mov cr0, eax
    
    ; DEBUG: '3' at 0xB8004 (Paging Enabled) - White on Green
    mov word [0xB8004], 0x2F33

    ; 10. Jump to 64-bit Long Mode
    ; We need a Far Jump to reload CS with 64-bit descriptor
    jmp 0x08:long_mode          ; 0x08 is 64-bit Code (STANDARD KERNEL SELECTOR)

BITS 64
long_mode:
    ; 11. Final 64-bit Setup
    mov ax, 0x10                ; 0x10 is 64-bit Data (STANDARD KERNEL SELECTOR)
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; DEBUG: '4' at 0xB8006 (Long Mode Start) - White on Magenta
    mov rax, 0xB8006
    mov word [rax], 0x5F34

    ; 12. Load Stack Pointer
    ; The BSP deposits the stack pointer for this CPU at 'ap_stack_ptr'
    mov rsp, [ap_stack_ptr]

    ; 13. Call C Entry Point
    ; The BSP deposits the C function address at 'ap_code_ptr'
    mov rax, [ap_code_ptr]
    call rax

    ; 14. Halt Loop (Should never reach here)
    cli
.halt:
    hlt
    jmp .halt

; ==============================================================================
; Data Section (Variables shared between BSP and AP)
; ==============================================================================
align 16
trampoline_data:

gdt_start:
    dq 0x0000000000000000       ; 0x00: Null
    dq 0x00AF9A000000FFFF       ; 0x08: 64-bit Code (Kernel Standard: L=1, D=0)
    dq 0x00CF92000000FFFF       ; 0x10: 64-bit Data (Kernel Standard: L=0, D=1)
    dq 0x00CF9A000000FFFF       ; 0x18: 32-bit Code (Tmp for Transition)
    dq 0x00CF92000000FFFF       ; 0x20: 32-bit Data (Tmp for Transition)
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start                ; Linear address (0x8000 + offset)

trampoline_end:
