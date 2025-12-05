/*
 * handlers.c - Interrupt Request (IRQ) Handler Implementations
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

#include "kernel.h"
#include "idt.h"
#include "keyboard.h"
#include "vga.h"

// Forward declaration
extern void keyboard_handler(void);

// Timer Tick Counter
static volatile uint64_t timer_ticks = 0;

// IRQ Handler Function Pointer Type
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[16];

/**
 * Register a custom handler for a given IRQ.
 */
void irq_install_handler(int irq, irq_handler_t handler) {
    ASSERT(handler != NULL);
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    } else {
        PANIC("Invalid IRQ number");
    }
}

/**
 * Unregister a handler for a given IRQ.
 */
void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

/**
 * Send End of Interrupt (EOI) signal to the PIC.
 * Example: If IRQ >= 8, we must send to both Slave and Master PICs.
 */
static void pic_send_eoi(int irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20); // EOI to Slave PIC
    }
    outb(0x20, 0x20); // EOI to Master PIC
}

/**
 * System Timer (IRQ0) Handler.
 */
static void timer_handler(void) {
    timer_ticks++;
}

/**
 * Common IRQ Dispatcher.
 * Called from the assembly IRQ stub.
 */
void irq_common_handler(struct interrupt_frame* frame) {
    int irq = frame->int_no - 32; // Remapped Offset (32)
    
    if (irq >= 0 && irq < 16) {
        // Special Case: Keyboard Driver
        if (irq == 1) {
            keyboard_handler();
        }
        // General Case: Execute registered handler if exists
        else if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
        
        // Acknowledge Interrupt (EOI)
        pic_send_eoi(irq);
    }
}

/**
 * Initialize IRQ Subsystem.
 */
void irq_init(void) {
    // Clear Handler Table
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = NULL;
    }
    
    // Install Timer Handler (IRQ0)
    irq_install_handler(0, timer_handler);
    
    // Unmask Timer IRQ (IRQ0) on Master PIC
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 0);
    outb(0x21, mask);
}

/**
 * Retrieve current uptime in ticks.
 */
uint64_t get_timer_ticks(void) {
    return timer_ticks;
}