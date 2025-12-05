/*
 * idt.c - Interrupt Descriptor Table (IDT) Implementation
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, NeXs Operate System
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "idt.h"
#include "vga.h"
#include "libc.h"

// IDT Table Storage (256 Entries)
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Architecture-specific External Assembly Stubs
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// x86 CPU Exception Names
const char* exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

/**
 * Remap Programmable Interrupt Controller (PIC)
 * Offsets standard IRQs (0-15) to standard CPU interrupts (32-47)
 * to avoid conflict with CPU Exception vectors (0-31).
 */
static void pic_remap(void) {
    // Save previous masks
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);
    
    // Start initialization sequence (ICW1)
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    
    // Set vector offsets (ICW2)
    outb(0x21, 0x20); // Master starts at 32 (0x20)
    io_wait();
    outb(0xA1, 0x28); // Slave starts at 40 (0x28)
    io_wait();
    
    // Set cascading identity (ICW3)
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    
    // Set 8086 mode (ICW4)
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    
    // Restore masks
    outb(0x21, a1);
    outb(0xA1, a2);
}

/**
 * Load IDT Entry into CPU Register
 */
static inline void idt_load(void) {
    asm volatile("lidt (%0)" : : "r"(&idtp));
}

/**
 * Set a Gate in the IDT
 */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].zero = 0;
}

/**
 * Initialize IDT Subsystem
 */
void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    
    // Zero out table
    vga_puts("DEBUG: idt_init start\n");
    memset(&idt, 0, sizeof(idt));
    vga_puts("DEBUG: memset done\n");
    
    // Remap IRQ controllers
    pic_remap();
    vga_puts("DEBUG: pic_remap done\n");
    
    // Install CPU Exception Handlers (0-31)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E);
    
    // Install IRQ Handlers (32-47)
    idt_set_gate(32, (uint64_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint64_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint64_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint64_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint64_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint64_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint64_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint64_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint64_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint64_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint64_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint64_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint64_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint64_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint64_t)irq15, 0x08, 0x8E);
    
    vga_puts("DEBUG: setup gates done\n");

    // Load table pointer
    idt_load();
    vga_puts("DEBUG: idt_load done\n");
}

/**
 * CPU Exception Dispatcher
 * This function is called from the assembly stub when an exception occurs.
 */
void isr_exception_handler(struct interrupt_frame* frame) {
    // Disable interrupts to prevent nested crashes
    cli();
    
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n\n*** KERNEL EXCEPTION ***\n");
    
    // Identify Exception
    if (frame->int_no < 32) {
        vga_puts("Exception: ");
        vga_puts(exception_messages[frame->int_no]);
    } else {
        vga_puts("Unknown Exception ");
        vga_puti(frame->int_no);
    }
    
    vga_puts("\nError Code: ");
    vga_putx(frame->err_code);
    
    // Dump CR2 (Fault Address) if Page Fault
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    vga_puts("  CR2: ");
    vga_putx(cr2);
    
    // Dump CPU State
    vga_puts("\nRIP: "); vga_putx(frame->rip);
    vga_puts("  CS: "); vga_putx(frame->cs);
    vga_puts("  RFLAGS: "); vga_putx(frame->rflags);
    vga_puts("\nRSP: "); vga_putx(frame->rsp);
    vga_puts("  SS: "); vga_putx(frame->ss);
    
    vga_puts("\n\nRegisters:");
    vga_puts("\nRAX: "); vga_putx(frame->rax);
    vga_puts("  RBX: "); vga_putx(frame->rbx);
    vga_puts("  RCX: "); vga_putx(frame->rcx);
    vga_puts("\nRDX: "); vga_putx(frame->rdx);
    vga_puts("  RSI: "); vga_putx(frame->rsi);
    vga_puts("  RDI: "); vga_putx(frame->rdi);
    vga_puts("\nRBP: "); vga_putx(frame->rbp);
    vga_puts("  R8:  "); vga_putx(frame->r8);
    vga_puts("  R9:  "); vga_putx(frame->r9);
    vga_puts("\nR10: "); vga_putx(frame->r10);
    vga_puts("  R11: "); vga_putx(frame->r11);
    vga_puts("  R12: "); vga_putx(frame->r12);
    vga_puts("\nR13: "); vga_putx(frame->r13);
    vga_puts("  R14: "); vga_putx(frame->r14);
    vga_puts("  R15: "); vga_putx(frame->r15);
    
    // Trigger Global Panic (allows potential soft recovery in kernel.c)
    PANIC("Unhandled CPU Exception");
}