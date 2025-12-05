; interrupts.asm - x86_64 Interrupt Handlers
[BITS 64]

section .text

; External C handlers
extern isr_exception_handler
extern irq_common_handler

; Macro for exception ISRs (no error code)
%macro ISR_NOERR 1
    global isr%1
    isr%1:
        cli
        push qword 0        ; Dummy error code
        push qword %1       ; Interrupt number
        jmp isr_common_stub
%endmacro

; Macro for exception ISRs (with error code)
%macro ISR_ERR 1
    global isr%1
    isr%1:
        cli
        push qword %1       ; Interrupt number
        jmp isr_common_stub
%endmacro

; Macro for IRQ handlers
%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push qword 0        ; Dummy error code
        push qword %2       ; IRQ number
        jmp irq_common_stub
%endmacro

; CPU Exception ISRs (0-31)
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

; IRQ handlers (32-47 = IRQ 0-15)
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

; Common ISR stub - saves context and calls C handler
isr_common_stub:
    ; Save all registers
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
    
    ; Call C exception handler (stack pointer as argument)
    mov rdi, rsp
    call isr_exception_handler
    
    jmp restore_context

; Common IRQ stub
irq_common_stub:
    ; Save all registers
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
    
    ; Call C IRQ handler
    mov rdi, rsp
    call irq_common_handler
    
restore_context:
    ; Restore segment registers
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore all registers
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
    
    ; Remove error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq
    
section .note.GNU-stack noalloc noexec nowrite progbits