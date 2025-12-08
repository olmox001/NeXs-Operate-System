; ==============================================================================
; kernel_entry.asm - Kernel Execution Entry Point
; ==============================================================================
; BSD 3-Clause License
;
; Copyright (c) 2025, NeXs Operate System
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; 3. Neither the name of the copyright holder nor the names of its
;    contributors may be used to endorse or promote products derived from
;    this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
; ==============================================================================

[BITS 64]                   ; Specify 64-bit Long Mode instructions

; External C kernel entry point
extern kernel_main          ; Symbol defined in kernel.c

; Export start symbol
global _start               ; Entry point symbol for the linker

section .text               ; Executable code section
_start:
    ; DEBUG: Write 'K' (White on Red) to top-left corner of VGA buffer
    ; This confirms we have successfully jumped to the kernel code
    mov rax, 0xB8000        ; Load VGA text buffer address into RAX
    mov word [rax], 0x4F4B  ; Write 'K' (0x4B) with Red bg/White fg (0x4F)
    
    ; Disable interrupts (should be disabled already, but safety first)
    cli                     ; Clear Interrupt Flag (IF)
    
    ; Clear Direction Flag (Standard GCC/SysV ABI requirement)
    cld                     ; Clear Direction Flag (DF) for string operations
    
    ; Setup Kernel Stack
    ; Address: 0x200000 (2MB mark) - Ensures stack is below the kernel code if loaded higher, or just a safe safe zone.
    mov rsp, 0x200000       ; Set Stack Pointer (RSP)
    and rsp, -16            ; Align stack to 16 bytes (ABI requirement)
    
    ; Initialize Stack Frame
    mov rbp, rsp            ; Set Base Pointer (RBP) to current RSP
    push rbp                ; Push terminator frame pointer (0 or self)
    mov rbp, rsp            ; Reset RBP
    
    ; Enable SSE (Required by GCC x86_64 code, preventing #UD on SSE instructions)
    mov rax, cr0            ; Read Control Register 0
    and ax, 0xFFFB          ; Clear EM (Emulation) bit (Bit 2)
    or ax, 0x2              ; Set MP (Monitor Coprocessor) bit (Bit 1)
    mov cr0, rax            ; Write back to CR0
    
    mov rax, cr4            ; Read Control Register 4
    or ax, 0x600            ; Set OSFXSR (Bit 9) and OSXMMEXCPT (Bit 10)
    mov cr4, rax            ; Write back to CR4
    
    ; Load Kernel GDT (Global Descriptor Table)
    ; Although Stage2 loaded a GDT, we reload it here to ensure we own it
    lgdt [gdt64.pointer]    ; Load GDTR with new GDT limit and base
    
    ; Reload Code Segment (CS)
    push 0x08               ; Push Kernel Code Segment Selector
    push .reload_cs         ; Push Return Address
    retfq                   ; Far Return to update CS (pops IP and CS)
.reload_cs:
    ; Reload Data Segments
    mov ax, 0x10            ; Kernel Data Segment Selector
    mov ds, ax              ; Update DS
    mov es, ax              ; Update ES
    mov fs, ax              ; Update FS
    mov gs, ax              ; Update GS
    mov ss, ax              ; Update SS
    
    ; Branch to C Kernel Main
    ; RDI already contains the boot_info pointer passed by the bootloader (System V ABI)
    call kernel_main        ; Call C function
    
    ; If kernel_main returns, halt the system
    cli                     ; Disable interrupts
    jmp .hang               ; Enter hang loop

.hang:
    hlt                     ; Halt CPU
    jmp .hang               ; Infinite loop

section .data               ; Initialized Data Section
align 16                    ; Align GDT to 16 bytes for performance
gdt64:
    ; Null Descriptor (Offset 0x00)
    dq 0                    ; Must be zero
    
    ; Kernel Code Descriptor (Offset 0x08)
    ; Present=1, DPL=0, Type=1 (Code), LongMode=1
    dw 0xFFFF               ; Limit low (ignored in Long Mode)
    dw 0                    ; Base low (ignored)
    db 0                    ; Base middle (ignored)
    db 10011010b            ; Access (Present, Ring0, Code, Exec/Read)
    db 10101111b            ; Flags (LongMode, 4K Granularity)
    db 0                    ; Base high (ignored)
    
    ; Kernel Data Descriptor (Offset 0x10)
    ; Present=1, DPL=0, Type=0 (Data)
    dw 0xFFFF               ; Limit low
    dw 0                    ; Base low
    db 0                    ; Base middle
    db 10010010b            ; Access (Present, Ring0, Data, Read/Write)
    db 11001111b            ; Flags (LongMode, 4K Granularity)
    db 0                    ; Base high
    
.pointer:
    dw $ - gdt64 - 1        ; Limit (Size - 1)
    dq gdt64                ; Base Address

; GNU Stack Note (Prevents linker warning about executable stack)
section .note.GNU-stack noalloc noexec nowrite progbits
