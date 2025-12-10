/*
 * ioapic.h - I/O APIC Driver (Header)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef IOAPIC_H
#define IOAPIC_H

#include "kernel.h"

// I/O APIC Registers
#define IOAPICID        0x00
#define IOAPICVER       0x01
#define IOAPICARB       0x02
#define IOREDTBL        0x10 // + 2 * index

// Redirection Table Entry Flags
#define IOAPIC_DELIVERY_FIXED   (0 << 8)
#define IOAPIC_DELIVERY_LOWEST  (1 << 8)
#define IOAPIC_DEST_PHYSICAL    (0 << 11)
#define IOAPIC_DEST_LOGICAL     (1 << 11)
#define IOAPIC_POLARITY_HIGH    (0 << 13)
#define IOAPIC_POLARITY_LOW     (1 << 13)
#define IOAPIC_TRIGGER_EDGE     (0 << 15)
#define IOAPIC_TRIGGER_LEVEL    (1 << 15)
#define IOAPIC_MASK             (1 << 16)

void ioapic_init(void);

/**
 * Configure a Redirection Table Entry
 * @param irq           input IRQ number (Global System Interrupt)
 * @param vector        destination CPU vector (e.g., 33 for IRQ 1)
 * @param dest_apic_id  destination LAPIC ID (usually 0 for BSP)
 * @param flags         delivery flags (0 for defaults: Fixed, Edge, High active)
 */
void ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, uint32_t flags);

// Enable standard keyboard IRQ (IRQ 1 -> Vector 33)
void ioapic_enable_keyboard(void);

#endif // IOAPIC_H
