// handlers.c - IRQ Handler Implementations
#include "kernel.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"

// Forward declaration (in case header order causes issues)
extern void keyboard_handler(void);

// Timer tick counter
static volatile uint64_t timer_ticks = 0;

// IRQ handler function pointer array
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[16];

// Register IRQ handler
void irq_install_handler(int irq, irq_handler_t handler) {
    ASSERT(handler != NULL);
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    } else {
        PANIC("Invalid IRQ number");
    }
}

// Unregister IRQ handler
void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

// Send EOI (End of Interrupt) to PIC
static void pic_send_eoi(int irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20); // EOI to slave PIC
    }
    outb(0x20, 0x20); // EOI to master PIC
}

// Timer (IRQ0) handler
static void timer_handler(void) {
    timer_ticks++;
}

// Common IRQ handler (called from assembly)
void irq_common_handler(struct interrupt_frame* frame) {
    int irq = frame->int_no - 32;
    
    if (irq >= 0 && irq < 16) {
        // Handle keyboard separately
        if (irq == 1) {
            keyboard_handler();
        }
        // Call registered handler
        else if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
        
        // Send EOI
        pic_send_eoi(irq);
    }
}

// Initialize IRQ handlers
void irq_init(void) {
    // Clear handler array
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = NULL;
    }
    
    // Install timer handler
    irq_install_handler(0, timer_handler);
    
    // Unmask timer IRQ (IRQ0)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 0);
    outb(0x21, mask);
}

// Get timer ticks
uint64_t get_timer_ticks(void) {
    return timer_ticks;
}