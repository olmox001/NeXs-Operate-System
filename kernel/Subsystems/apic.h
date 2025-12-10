/*
 * apic.h - Local APIC Driver
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef APIC_H
#define APIC_H

#include "kernel.h"

// =============================================================================
// APIC Register Offsets (Memory Mapped)
// =============================================================================

#define LAPIC_ID            0x0020
#define LAPIC_VER           0x0030
#define LAPIC_TPR           0x0080
#define LAPIC_EOI           0x00B0
#define LAPIC_SVR           0x00F0  // Spurious Interrupt Vector
#define LAPIC_ESR           0x0280  // Error Status
#define LAPIC_ICR_LOW       0x0300  // Interrupt Command Register (0-31)
#define LAPIC_ICR_HIGH      0x0310  // Interrupt Command Register (32-63)
#define LAPIC_TIMER         0x0320
#define LAPIC_TIMER_INIT    0x0380
#define LAPIC_TIMER_CUR     0x0390
#define LAPIC_TIMER_DIV     0x03E0

// =============================================================================
// Constants
// =============================================================================

#define LAPIC_SVR_ENABLE    0x100
#define LAPIC_SPURIOUS_IRQ  0xFF  // Vector for spurious interrupts

// IPI Delivery Modes
// IPI Delivery Modes (Bits 8-10)
#define IPI_FIXED           0x00000000
#define IPI_INIT            0x00000500
#define IPI_STARTUP         0x00000600

// Flags
#define IPI_ASSERT          0x00004000 // Bit 14
#define IPI_LEVEL           0x00008000 // Bit 15 (Trigger Mode: 1=Level)
#define IPI_DEST_PHYS       0x00000000 // Bit 11 (0=Physical)

// =============================================================================
// Public API
// =============================================================================

// Initialize Local APIC for the current core
void lapic_init(void);

// Get current Core ID (from APIC ID register)
uint32_t lapic_id(void);

// Send End-Of-Interrupt
void lapic_eoi(void);

// Send Init IPI
void lapic_send_init(uint32_t apic_id);

// Send Startup IPI (SIPI) with vector (0x00-0xFF, address = vector * 4096)
void lapic_send_sipi(uint32_t apic_id, uint8_t vector);

// Initialize LAPIC Timer (Periodic 1ms)
void lapic_timer_init(uint8_t vector);

// Re-arm Timer (for One-Shot Mode)
void lapic_timer_rearm(void);

#endif // APIC_H
