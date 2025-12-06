/*
 * handlers.c - IRQ Handlers
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "kernel.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"

// Forward declarations
extern void keyboard_handler(void);
extern void timer_tick(void);

// IRQ Handler Table
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[16];

void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

static void pic_send_eoi(int irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void irq_common_handler(struct interrupt_frame* frame) {
    int irq = frame->int_no - 32;
    
    if (irq >= 0 && irq < 16) {
        if (irq == 0) {
            timer_tick();  // PIT tick for scheduler
        } else if (irq == 1) {
            keyboard_handler();
        } else if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
        pic_send_eoi(irq);
    }
}

// Forward declare timer_init from timer.c
extern void timer_init(void);

void irq_init(void) {
    // Clear handlers
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = NULL;
    }
    
    // Initialize high-precision timer (TSC + PIT)
    timer_init();
    
    // Unmask Timer (IRQ0) and Keyboard (IRQ1)
    uint8_t mask = inb(0x21);
    mask &= ~0x03;
    outb(0x21, mask);
}
