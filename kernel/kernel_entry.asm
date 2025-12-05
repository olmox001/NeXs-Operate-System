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

[BITS 64]

; External C kernel entry point
extern kernel_main

; Export start symbol
global _start

section .text
_start:
    ; DEBUG: Write 'K' (White on Red) to top-left corner of VGA buffer
    ; This confirms we have successfully jumped to the kernel code
    mov rax, 0xB8000
    mov word [rax], 0x4F4B
    
    ; Disable interrupts (should be disabled already, but safety first)
    cli
    
    ; Clear Direction Flag (Standard GCC/SysV ABI requirement)
    cld
    
    ; Setup Kernel Stack
    ; Address: 0x200000 (2MB mark)
    mov rsp, 0x200000
    and rsp, -16        ; Align stack to 16 bytes (ABI requirement)
    
    ; Initialize Stack Frame
    mov rbp, rsp        ; Set Base Pointer
    push rbp            ; Push terminator frame
    mov rbp, rsp
    
    ; Enable SSE (Required by GCC x86_64 code)
    mov rax, cr0
    and ax, 0xFFFB      ; Clear EM (Emulation) bit
    or ax, 0x2          ; Set MP (Monitor Coprocessor) bit
    mov cr0, rax
    
    mov rax, cr4
    or ax, 0x600        ; Set OSFXSR (Bit 9) and OSXMMEXCPT (Bit 10)
    mov cr4, rax
    
    ; Load Kernel GDT (Global Descriptor Table)
    lgdt [gdt64.pointer]
    
    ; Reload Code Segment (CS)
    push 0x08           ; Kernel Code Segment Selector
    push .reload_cs     ; Return Address
    retfq               ; Far Return to update CS
.reload_cs:
    ; Reload Data Segments
    mov ax, 0x10        ; Kernel Data Segment Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Branch to C Kernel Main
    ; RDI already contains the boot_info pointer passed by the bootloader
    call kernel_main

    ; If kernel_main returns, halt the system
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
    
    ; Kernel Code Descriptor (Offset 0x08)
    ; Present=1, DPL=0, Type=1 (Code), LongMode=1
    dw 0xFFFF       ; Limit low
    dw 0            ; Base low
    db 0            ; Base middle
    db 10011010b    ; Access (Present, Ring0, Code, Exec/Read)
    db 10101111b    ; Flags (LongMode, 4K Granularity)
    db 0            ; Base high
    
    ; Kernel Data Descriptor (Offset 0x10)
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

; GNU Stack Note (Prevents linker warning about executable stack)
section .note.GNU-stack noalloc noexec nowrite progbits
