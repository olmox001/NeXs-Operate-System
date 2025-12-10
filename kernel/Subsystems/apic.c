/*
 * apic.c - Local APIC Driver Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "apic.h"
#include "acpi.h"
#include "kernel.h" // For panic
#include "vga.h"    // For debug prints
#include "vmm/vmm.h" // For vmm_map_page

// Base address of Local APIC (found via ACPI)
volatile uint32_t* g_lapic_addr = NULL;

// Helper to read register
static uint32_t lapic_read(uint32_t reg) {
    if (!g_lapic_addr) return 0;
    return g_lapic_addr[reg / 4];
}

// Helper to write register
static void lapic_write(uint32_t reg, uint32_t value) {
    if (!g_lapic_addr) return;
    g_lapic_addr[reg / 4] = value;
}

// =============================================================================
// Public API
// =============================================================================

void lapic_init(void) {
    if (!g_lapic_addr) {
        uint64_t base = acpi_get_lapic_base();
        g_lapic_addr = (volatile uint32_t*)base; // Physical Address
        
        // CRITICAL: Map the LAPIC MMIO region (4KB)
        // Ensure it's Writable and Present.
        // On real hardware we probably want Cache Disable too, but standard is fine for QEMU.
        // Identity Map: Virt = Phys
        vmm_map_page(&kernel_space, base, base, PTE_PRESENT | PTE_WRITABLE);
    }
    
    // Enable Local APIC
    // Set SVR (Spurious Interrupt Vector) to 0x1FF (Vector 255 + Enable Bit 8)
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_IRQ);
    
    // Disable Legacy PIC (Mask all interrupts)
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    
    // Switch Interrupt Handler to use APIC EOI
    irq_set_apic_mode(true);
}

uint32_t lapic_id(void) {
    if (!g_lapic_addr) return 0;
    // APIC ID is in bits 24-31
    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

// =============================================================================
// IPI Functions (For SMP)
// =============================================================================

void lapic_send_init(uint32_t apic_id) {
    // Write High: Destination APIC ID
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    
    // Write Low: Init Command
    // INIT | Level ( Assert | Trigger Level )
    lapic_write(LAPIC_ICR_LOW, IPI_INIT | IPI_ASSERT | IPI_LEVEL);
}



void lapic_send_sipi(uint32_t apic_id, uint8_t vector) {
    // Write High: Destination APIC ID
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    
    // Write Low: Startup IPI Command + Vector
    // SIPI is Edge Triggered. No Assert/Level flags needed/checked.
    lapic_write(LAPIC_ICR_LOW, IPI_STARTUP | vector);
}

// =============================================================================
// Local APIC Timer
// =============================================================================

#define LAPIC_TIMER_MODE_ONESHOT    0x00000
#define LAPIC_TIMER_MODE_PERIODIC   0x20000
#define LAPIC_TIMER_DIV_16          0x3

static uint32_t g_lapic_ticks_per_ms = 0;

// Need external delay function (from timer.h) to calibrate against TSC/PIT
extern void timer_delay_ms(uint64_t ms);

// Calibrate LAPIC Timer (Run on BSP or once)
static void lapic_timer_calibrate(void) {
    if (g_lapic_ticks_per_ms > 0) return; // Already calibrated via another core? (Shared variable)

    // 1. Initialize Timer: Divide by 16, One-Shot, Masked (wait, we want to count)
    lapic_write(LAPIC_TIMER_DIV, LAPIC_TIMER_DIV_16);
    lapic_write(LAPIC_TIMER, 0x10000); // Masked (Bit 16) | One Shot (0)
    
    // 2. Set Max Count
    lapic_write(LAPIC_TIMER_INIT, 0xFFFFFFFF);
    
    // 3. Wait 10ms (using trusted source: PIT/TSC)
    timer_delay_ms(10);
    
    // 4. Read Current Count
    uint32_t current = lapic_read(LAPIC_TIMER_CUR);
    
    // 5. Calculate Ticks elapsed
    uint32_t ticks = 0xFFFFFFFF - current;
    
    // 6. Calculate Ticks per MS
    g_lapic_ticks_per_ms = ticks / 10;
    
    vga_puts("DEBUG: LAPIC Timer Calibrated: "); vga_puti(g_lapic_ticks_per_ms); vga_puts(" ticks/ms\n");
    
    // Stop Timer
    lapic_write(LAPIC_TIMER_INIT, 0);
}

// Enable LAPIC Timer interrupt (Periodic 1ms)
// Vector: The interrupt vector to trigger (usually 32 for IRQ0 emulation or separate)
void lapic_timer_init(uint8_t vector) {
    if (g_lapic_ticks_per_ms == 0) {
        lapic_timer_calibrate();
    }
    
    // 1. Set Divider (16)
    lapic_write(LAPIC_TIMER_DIV, LAPIC_TIMER_DIV_16);
    
    // 2. Set Vector & Mode (One-Shot)
    // Note: We unmask it here (Bit 16 is 0)
    lapic_write(LAPIC_TIMER, vector | LAPIC_TIMER_MODE_ONESHOT);
    
    // 3. Set Initial Count (Triggers start)
    // vga_puts("LAPIC Timer Init: "); vga_puti(g_lapic_ticks_per_ms); vga_puts("\n");
    lapic_write(LAPIC_TIMER_INIT, g_lapic_ticks_per_ms); // 1ms interval
}

// Re-arm Timer (Called from ISR)
void lapic_timer_rearm(void) {
    if (g_lapic_ticks_per_ms > 0) {
        lapic_write(LAPIC_TIMER_INIT, g_lapic_ticks_per_ms);
    }
}

