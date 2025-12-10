/*
 * ioapic.c - I/O APIC Driver Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "ioapic.h"
#include "acpi.h"
#include "vmm/vmm.h"
#include "vga.h"

// Access Windows
// The IOAPIC has a memory-mapped Selection Register (Base) and a Data Window (Base + 0x10)
#define IOREGSEL 0x00
#define IOWIN    0x10

static volatile uint32_t* g_ioapic_regs = NULL;

static void ioapic_write(uint8_t reg, uint32_t value) {
    if (!g_ioapic_regs) return;
    
    // Select Register
    g_ioapic_regs[IOREGSEL/4] = reg;
    // Write Data
    g_ioapic_regs[IOWIN/4] = value;
}

static uint32_t ioapic_read(uint8_t reg) {
    if (!g_ioapic_regs) return 0;
    
    // Select Register
    g_ioapic_regs[IOREGSEL/4] = reg;
    // Read Data
    return g_ioapic_regs[IOWIN/4];
}

void ioapic_init(void) {
    uint64_t base = acpi_get_ioapic_base();
    if (!base) {
        vga_puts("ERROR: IOAPIC Base not found via ACPI!\n");
        return;
    }
    
    // Identity Map the IOAPIC base
    // Usually 0xFEC00000. We map it to itself.
    // Ensure we handle page alignment.
    uint64_t page = base & ~0xFFF;
    vmm_map_page(&kernel_space, page, page, PTE_PRESENT | PTE_WRITABLE | PTE_NOCACHE); 
    // Note: PCC/PCD (Cache Disable) is important for MMIO
    
    g_ioapic_regs = (volatile uint32_t*)base;
    
    uint32_t ver = ioapic_read(IOAPICVER);
    uint32_t count = ((ver >> 16) & 0xFF) + 1;
    
    vga_puts("IOAPIC: Initialized at "); vga_putx(base);
    vga_puts(" Max Redir Entries: "); vga_puti(count); vga_puts("\n");
}

void ioapic_set_entry(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, uint32_t flags) {
    if (!g_ioapic_regs) return;
    
    // Each Redirection Entry is 64-bits (2x 32-bit registers)
    // Low 32-bits: Vector, Flags
    // High 32-bits: Destination APIC ID
    
    uint32_t low_index = IOREDTBL + 2 * irq;
    uint32_t high_index = IOREDTBL + 2 * irq + 1;
    
    uint32_t low_val = vector | flags;
    uint32_t high_val = (dest_apic_id << 24); // Destination ID is in bits 56-63 (offset 24 in high dword)
    
    ioapic_write(low_index, low_val);
    ioapic_write(high_index, high_val);
    
    // vga_puts("IOAPIC: Map IRQ "); vga_puti(irq); 
    // vga_puts(" -> Vector "); vga_puti(vector); 
    // vga_puts(" Dest "); vga_puti(dest_apic_id); vga_puts("\n");
}

void ioapic_enable_keyboard(void) {
    // IRQ 1 -> Vector 33
    // Fixed Delivery, Physical Destination, High Active, Edge Triggered (Standard ISA)
    // Destination: BSP (APIC ID 0 usually, but we should use current CPU or 0)
    // WARNING: APIC ID 0 might not be BSP if system is weird, but usually is.
    
    ioapic_set_entry(1, 33, 0, IOAPIC_DELIVERY_FIXED | IOAPIC_DEST_PHYSICAL | 
                               IOAPIC_POLARITY_HIGH | IOAPIC_TRIGGER_EDGE);
                               
    vga_puts("IOAPIC: Keyboard Enabled (IRQ1 -> Vec 33)\n");
}
