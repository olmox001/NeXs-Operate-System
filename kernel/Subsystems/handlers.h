/*
 * handlers.h - IRQ Handler Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef HANDLERS_H
#define HANDLERS_H

#include "kernel.h"

// =============================================================================
// IRQ Subsystem Interface
// =============================================================================

/**
 * Initialize the IRQ Subsystem
 * Clears handlers and sets up timer, PIC masks, etc.
 */
void irq_init(void);

/**
 * Install a custom handler for a specific IRQ.
 * 
 * @param irq IRQ Number (0-15)
 * @param handler Function pointer to the handler (void func(void))
 */
void irq_install_handler(int irq, void (*handler)(void));

/**
 * Remove a handler for a specific IRQ.
 * 
 * @param irq IRQ Number (0-15)
 */
void irq_uninstall_handler(int irq);

/**
 * Enable/Disable APIC EOI Mode
 * @param active true for LAPIC EOI, false for Legacy PIC EOI
 */
void irq_set_apic_mode(bool active);

// Include timer definitions as timing is tightly coupled with IRQ0
#include "timer.h"

#endif // HANDLERS_H
