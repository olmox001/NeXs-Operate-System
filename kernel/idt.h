// idt.h - Interrupt Descriptor Table Setup
#ifndef IDT_H
#define IDT_H

#include "kernel.h"

// IDT entry structure
struct idt_entry {
    uint16_t offset_low;    // Offset bits 0-15
    uint16_t selector;      // Code segment selector
    uint8_t  ist;           // Interrupt Stack Table offset
    uint8_t  type_attr;     // Type and attributes
    uint16_t offset_mid;    // Offset bits 16-31
    uint32_t offset_high;   // Offset bits 32-63
    uint32_t zero;          // Reserved
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Interrupt frame passed to handlers
struct interrupt_frame {
    uint64_t gs, fs, es, ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

// Initialize IDT
void idt_init(void);

// Set IDT entry
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

// Exception names
extern const char* exception_messages[32];

#endif // IDT_H