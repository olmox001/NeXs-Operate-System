/*
 * handlers.c - High-Level IRQ Handlers
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "kernel.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"
#include "Subsystems/apic.h" // For lapic_eoi

// Global flag to switch between PIC and APIC EOI mode
bool g_using_apic = false;

void irq_set_apic_mode(bool active) {
    g_using_apic = active;
}

// =============================================================================
// Forward Declarations
// =============================================================================
extern void keyboard_handler(void);
extern void timer_tick(void);

// =============================================================================
// IRQ Handler Table
// =============================================================================
// Stores function pointers to registered IRQ handlers (0-15)
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[16];

/**
 * Install a handler for a specific IRQ.
 */
void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

/**
 * Remove a handler for a specific IRQ.
 */
void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

/**
 * Send End-of-Interrupt (EOI) to Interrupt Controller (PIC or APIC).
 * Must be done at the end of every IRQ handler.
 */
static void irq_send_eoi(int irq) {
    if (g_using_apic) {
        // Local APIC EOI (Write 0 to EOI Register)
        // Note: LAPIC handles the vector internally
        lapic_eoi();
    } else {
        // 8259 PIC EOI
        // If IRQ >= 8, it came from the Slave PIC, so we must signal it.
        if (irq >= 8) outb(0xA0, 0x20);
        
        // Always signal the Master PIC
        outb(0x20, 0x20);
    }
}

/**
 * Common IRQ Handler
 * Called from the assembly IRQ stub. Dispatches to specific handlers.
 * 
 * @param frame Stack frame containing CPU state
 */
void irq_common_handler(struct interrupt_frame* frame) {
    // Calculate IRQ number (Int No - 32)
    int irq = frame->int_no - 32;
    
    // Validate IRQ range
    if (irq >= 0 && irq < 16) {
        // Special case dispatching for critical system drivers
        if (irq == 0) {
            timer_tick();       // IRQ0: PIT tick for scheduler and timekeeping
        } else if (irq == 1) {
            keyboard_handler(); // IRQ1: PS/2 Keyboard Input
        } else if (irq_handlers[irq]) {
            irq_handlers[irq](); // Dispatch to registered handler
        }
        
        // Acknowledge Interrupt
        irq_send_eoi(irq);
    }
}

// Forward declare timer_init from timer.c
extern void timer_init(void);

/**
 * Initialize IRQ Subsystem
 */
void irq_init(void) {
    // Clear all handlers initially
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = NULL;
    }
    
    // Initialize high-precision timer (TSC + PIT)
    // This is critical for the system tick (IRQ0)
    timer_init();
    
    // Unmask Timer (IRQ0) and Keyboard (IRQ1) on Master PIC
    // MOVED: We do this later in kernel_main to prevent premature interrupts during VMM init
    // uint8_t mask = inb(0x21);
    // mask &= ~0x03;          // Enable bit 0 (Timer) and bit 1 (Keyboard)
    // outb(0x21, mask);
    
    // Ensure all masked for safety
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}


