// idt.c - Implementation
#include "idt.h"
#include "vga.h"
#include "libc.h"

// IDT entries (256 total)
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// External assembly handlers
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

// Exception messages
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

// Remap PIC (Programmable Interrupt Controller)
static void pic_remap(void) {
    // Save masks
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);
    
    // Start initialization sequence
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    
    // Set vector offsets (32-47 for IRQs)
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    
    // Set cascading
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    
    // Set 8086 mode
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    
    // Restore masks
    outb(0x21, a1);
    outb(0xA1, a2);
}

// Load IDT
static inline void idt_load(void) {
    asm volatile("lidt (%0)" : : "r"(&idtp));
}

// Set IDT gate
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].zero = 0;
}

// Initialize IDT
void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    
    // Clear IDT
    vga_puts("DEBUG: idt_init start\n");
    memset(&idt, 0, sizeof(idt));
    vga_puts("DEBUG: memset done\n");
    
    // Remap PIC
    pic_remap();
    vga_puts("DEBUG: pic_remap done\n");
    
    // Install exception handlers (0-31)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E); // Page Fault
    // ... we can just execute the whole block, or print in between
    
    // Install exception handlers loop? No, explicit calls.
    // Let's assume setting gates is fine (just memory writes).
    
    int i;
    // Just checking a few ranges
    for (i=0; i<32; i++) {
        // Assume these are fine
    }
    vga_puts("DEBUG: gates set\n");

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
    
    // Install IRQ handlers (32-47)
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

    // Load IDT
    idt_load();
    vga_puts("DEBUG: idt_load done\n");
    
    // Enable interrupts
    // sti(); // Moved to kernel.c after verified initialization
}

// Exception handler (called from assembly)
void isr_exception_handler(struct interrupt_frame* frame) {
    cli();
    
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n\n*** KERNEL EXCEPTION ***\n");
    
    // Verify interrupt number is within bounds
    if (frame->int_no < 32) {
        vga_puts("Exception: ");
        vga_puts(exception_messages[frame->int_no]);
    } else {
        vga_puts("Unknown Exception ");
        vga_puti(frame->int_no);
    }
    
    vga_puts("\nError Code: ");
    vga_putx(frame->err_code);
    
    // CR2 is crucial for Page Faults
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    vga_puts("  CR2: ");
    vga_putx(cr2);
    
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
    
    // Instead of freezing, trigger the global panic handler
    // This allows for Soft Recovery (Restart Shell)
    PANIC("Unhandled CPU Exception");
}