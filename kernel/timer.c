/*
 * timer.c - High-Precision Timer Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "timer.h"
#include "vga.h"

// =============================================================================
// Constants & State
// =============================================================================

// Programmable Interval Timer (PIT) Constants
#define PIT_FREQUENCY   1193182ULL  // Base frequency (1.19 MHz)
#define PIT_HZ          1000        // Target frequency for Scheduler (1ms)
#define PIT_DIVISOR     (PIT_FREQUENCY / PIT_HZ)

// Internal State
static volatile uint64_t pit_ticks = 0; // Counts IRQ0 interrupts
static uint64_t tsc_freq_hz = 0;        // Calibrated TSC Frequency
static uint64_t tsc_freq_khz = 0;       // Frequency in kHz (Optimization)
static uint64_t tsc_boot = 0;           // TSC value at end of calibration

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Configure PIT Channel 0
 * Sets the divider to generate IRQ0 at roughly 1000Hz.
 */
static void pit_configure(uint16_t divisor) {
    // Mode 3 (Square Wave), Binary
    outb(0x43, 0x36);
    // Send LSB then MSB
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

/**
 * Calibrate TSC using PIT
 * The PIT has a known fixed frequency. We use it to measure how many 
 * CPU cycles (TSC) occur in a known time interval.
 */
static void tsc_calibrate(void) {
    vga_puts("      Calibrating TSC...");
    
    // 1. Configure PIT Channel 2 for one-shot timing
    // Channel 2 is connected to the PC Speaker, but we can read its status
    // and use it for timing without generating interrupts.
    
    // Enable Speaker Gate (Bit 0 of 0x61)
    outb(0x61, (inb(0x61) & 0xFD) | 0x01);
    
    // Configure Channel 2: Mode 0 (Interrupt on Terminal Count), Binary
    outb(0x43, 0xB0);
    
    // Set Count for ~10ms
    // 1193182 Hz / 100 = 11932 ticks
    uint16_t count = 11932;
    outb(0x42, count & 0xFF);
    outb(0x42, (count >> 8) & 0xFF);
    
    // 2. Read Start TSC
    uint64_t tsc_start = rdtsc();
    
    // 3. Wait for PIT to finish (Bit 5 of 0x61 goes High when output changes)
    // Actually, for Mode 0, output goes high when count reaches 0.
    // wait for output bit to go high.
    while ((inb(0x61) & 0x20) == 0) {
        asm volatile("pause");
    }
    
    // 4. Read End TSC
    uint64_t tsc_end = rdtsc();
    
    // 5. Disable Speaker Gate
    outb(0x61, inb(0x61) & 0xFC);
    
    // 6. Calculate Frequency
    uint64_t tsc_diff = tsc_end - tsc_start;
    
    // We measured over 10ms (1/100th of a second)
    tsc_freq_hz = tsc_diff * 100;
    
    // Store kHz for faster runtime division
    tsc_freq_khz = tsc_freq_hz / 1000;
    
    if (tsc_freq_khz == 0) tsc_freq_khz = 1; // Prevent div/0 panic
    
    // Mark Boot Time
    tsc_boot = rdtsc();
    
    // Report
    vga_puts(" ");
    vga_puti((int)(tsc_freq_hz / 1000000));
    vga_puts(" MHz\n");
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Initialize Timer Subsystem
 */
void timer_init(void) {
    // 1. Calibrate TSC first (uses PIT Channel 2)
    tsc_calibrate();
    
    // 2. Configure PIT Channel 0 for System Tick (1000Hz)
    pit_configure(PIT_DIVISOR);
}

/**
 * Handle System Tick (IRQ0)
 * Called from 'irq_common_handler' (handlers.c)
 */
void timer_tick(void) {
    pit_ticks++;
}

// Get Data
uint64_t timer_get_tsc(void) { return rdtsc(); }
uint64_t timer_get_freq(void) { return tsc_freq_hz; }
uint64_t timer_get_ticks(void) { return pit_ticks; }

// Time Conversions
uint64_t timer_get_ns(void) {
    if (tsc_freq_khz == 0) return 0;
    uint64_t tsc = rdtsc() - tsc_boot;
    // ns = cycles * 1,000,000,000 / freq_hz
    // equivalent: cycles * 1,000,000 / freq_khz
    return (tsc * 1000000ULL) / tsc_freq_khz;
}

uint64_t timer_get_us(void) {
    if (tsc_freq_khz == 0) return 0;
    uint64_t tsc = rdtsc() - tsc_boot;
    return (tsc * 1000ULL) / tsc_freq_khz;
}

uint64_t timer_get_ms(void) {
    if (tsc_freq_khz == 0) return 0;
    uint64_t tsc = rdtsc() - tsc_boot;
    return tsc / tsc_freq_khz;
}

uint64_t timer_get_sec(void) {
    return timer_get_ms() / 1000;
}

// Delays
void timer_delay_ns(uint64_t ns) {
    if (tsc_freq_khz == 0) return;
    uint64_t start = rdtsc();
    uint64_t cycles = (ns * tsc_freq_khz) / 1000000ULL;
    while (rdtsc() < start + cycles) { asm volatile("pause"); }
}

void timer_delay_us(uint64_t us) {
    if (tsc_freq_khz == 0) return;
    uint64_t start = rdtsc();
    uint64_t cycles = (us * tsc_freq_khz) / 1000ULL;
    while (rdtsc() < start + cycles) { asm volatile("pause"); }
}

void timer_delay_ms(uint64_t ms) {
    if (tsc_freq_khz == 0) return;
    uint64_t start = rdtsc();
    uint64_t cycles = ms * tsc_freq_khz;
    while (rdtsc() < start + cycles) { asm volatile("pause"); }
}
