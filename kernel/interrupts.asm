; ==============================================================================
; interrupts.asm - x86_64 Low-Level Interrupt Handling
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

[BITS 64]                   ; Specify 64-bit Long Mode

section .text               ; Code section

; External C High-Level Handlers
extern isr_exception_handler ; Defined in handlers.c
extern irq_common_handler    ; Defined in handlers.c

; ==============================================================================
; Macros for ISR Generation
; ==============================================================================

; Macro for Exception ISRs WITHOUT error code
; Pushes a dummy error code (0) to maintain stack alignment uniformity.
%macro ISR_NOERR 1
    global isr%1            ; Export symbol
    isr%1:
        cli                 ; Disable interrupts
        push qword 0        ; Push dummy error code
        push qword %1       ; Push Interrupt number
        jmp isr_common_stub ; Jump to common handler
%endmacro

; Macro for Exception ISRs WITH error code
; The CPU pushes the error code automatically.
%macro ISR_ERR 1
    global isr%1            ; Export symbol
    isr%1:
        cli                 ; Disable interrupts
        push qword %1       ; Push Interrupt number
        jmp isr_common_stub ; Jump to common handler
%endmacro

; Macro for Hardware IRQ Handlers
%macro IRQ 2
    global irq%1            ; Export symbol
    irq%1:
        cli                 ; Disable interrupts
        push qword 0        ; Push dummy error code
        push qword %2       ; Push IRQ number (mapped vector)
        jmp irq_common_stub ; Jump to common handler
%endmacro

; ==============================================================================
; ISR Definitions (0-31)
; ==============================================================================
ISR_NOERR 0     ; Divide by zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; Non-maskable interrupt
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound range exceeded
ISR_NOERR 6     ; Invalid opcode
ISR_NOERR 7     ; Device not available
ISR_ERR   8     ; Double fault (has error code)
ISR_NOERR 9     ; Coprocessor segment overrun
ISR_ERR   10    ; Invalid TSS (has error code)
ISR_ERR   11    ; Segment not present (has error code)
ISR_ERR   12    ; Stack-segment fault (has error code)
ISR_ERR   13    ; General protection fault (has error code)
ISR_ERR   14    ; Page fault (has error code)
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 FPU error
ISR_ERR   17    ; Alignment check (has error code)
ISR_NOERR 18    ; Machine check
ISR_NOERR 19    ; SIMD floating-point exception
ISR_NOERR 20    ; Virtualization exception
ISR_NOERR 21    ; Reserved
ISR_NOERR 22    ; Reserved
ISR_NOERR 23    ; Reserved
ISR_NOERR 24    ; Reserved
ISR_NOERR 25    ; Reserved
ISR_NOERR 26    ; Reserved
ISR_NOERR 27    ; Reserved
ISR_NOERR 28    ; Reserved
ISR_NOERR 29    ; Reserved
ISR_ERR   30    ; Security exception (has error code)
ISR_NOERR 31    ; Reserved

; ==============================================================================
; IRQ Definitions (32-47, which maps to Hardware IRQ 0-15)
; ==============================================================================
IRQ 0,  32      ; Timer
IRQ 1,  33      ; Keyboard
IRQ 2,  34      ; Cascade
IRQ 3,  35      ; COM2
IRQ 4,  36      ; COM1
IRQ 5,  37      ; LPT2
IRQ 6,  38      ; Floppy
IRQ 7,  39      ; LPT1
IRQ 8,  40      ; RTC
IRQ 9,  41      ; Free
IRQ 10, 42      ; Free
IRQ 11, 43      ; Free
IRQ 12, 44      ; Mouse
IRQ 13, 45      ; FPU
IRQ 14, 46      ; Primary ATA
IRQ 15, 47      ; Secondary ATA

; ==============================================================================
; Software Interrupt: Syscall (INT 0x80 = 128)
; ==============================================================================
global isr128
extern syscall_handler      ; Defined in syscall.c
isr128:
    cli                     ; Disable interrupts
    push qword 0            ; Dummy error code
    push qword 128          ; Interrupt number
    
    ; Save all registers (same as ISR/IRQ stubs for consistency)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save segment registers
    mov rax, ds
    push rax
    mov rax, es
    push rax
    mov rax, fs
    push rax
    mov rax, gs
    push rax
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler
    ; syscall_handler(rax=syscall_num, rdi=arg1, rsi=arg2, rdx=arg3)
    ; Registers are already set by caller, pass stack pointer in RDI as 4th arg
    mov rdi, rsp
    call syscall_handler
    
    ; Restore segment registers
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Clean up stack
    add rsp, 16             ; Skip dummy error code and int number
    
    iretq                   ; Return from Interrupt

; ==============================================================================
; Common ISR Stub
; Saves CPU state and calls C Exception Handler
; ==============================================================================
isr_common_stub:
    ; Save General Purpose Registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save Segment Registers
    mov rax, ds
    push rax
    mov rax, es
    push rax
    mov rax, fs
    push rax
    mov rax, gs
    push rax
    
    ; Load Kernel Data Segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Pass Stack Pointer as Argument (RDI)
    mov rdi, rsp            ; Pointer to registers_t struct
    
    ; Call C Handler
    call isr_exception_handler
    
    jmp restore_context     ; Output point

; ==============================================================================
; Common IRQ Stub
; Saves CPU state and calls C IRQ Handler
; ==============================================================================
irq_common_stub:
    ; Save General Purpose Registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save Segment Registers
    mov rax, ds
    push rax
    mov rax, es
    push rax
    mov rax, fs
    push rax
    mov rax, gs
    push rax
    
    ; Load Kernel Data Segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Pass Stack Pointer as Argument (RDI)
    mov rdi, rsp            ; Pointer to registers_t struct
    
    ; Call C Handler
    call irq_common_handler
    
    ; [NEW] Context Switch Hook
    ; uint64_t scheduler_switch(uint64_t current_rsp);
    ; Returns the new RSP (which might be the same as current_rsp)
    extern scheduler_switch
    mov rdi, rsp            ; Arg1: Current Stack Pointer
    call scheduler_switch   ; Call Scheduler
    mov rsp, rax            ; Update Stack Pointer (Switch Task if changed)
    
restore_context:
    ; Restore Segment Registers
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore General Purpose Registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Clean up Stack (Remove Error Code and Int Number)
    add rsp, 16             ; Skip error code (8) and int no (8)
    
    ; Return from Interrupt
    iretq                   ; Restore CS:RIP and RFLAGS, switch stack if needed
    
section .note.GNU-stack noalloc noexec nowrite progbits