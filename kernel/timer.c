/*
 * timer.c - High-Precision Timer Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "timer.h"
#include "vga.h"

// PIT Configuration
#define PIT_FREQUENCY   1193182ULL
#define PIT_HZ          1000        // 1000Hz for scheduler
#define PIT_DIVISOR     (PIT_FREQUENCY / PIT_HZ)

// Timer State
static volatile uint64_t pit_ticks = 0;
static uint64_t tsc_freq_hz = 0;        // TSC frequency in Hz
static uint64_t tsc_freq_khz = 0;       // TSC frequency in kHz (for division)
static uint64_t tsc_boot = 0;           // TSC value at boot

// Configure PIT Channel 0
static void pit_configure(uint16_t divisor) {
    outb(0x43, 0x36);  // Channel 0, lo/hi, mode 3, binary
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

// Calibrate TSC using PIT
// Measures TSC ticks over ~10ms to determine frequency
static void tsc_calibrate(void) {
    vga_puts("      Calibrating TSC...");
    
    // Configure PIT for one-shot timing
    // We'll count TSC ticks during a known PIT interval
    
    // Wait for PIT to settle
    for (volatile int i = 0; i < 10000; i++);
    
    // Read TSC start
    uint64_t tsc_start = rdtsc();
    
    // Wait ~10ms using PIT (read channel 0 directly)
    // Configure PIT channel 2 for timing
    outb(0x61, (inb(0x61) & 0xFD) | 0x01);  // Enable speaker gate
    outb(0x43, 0xB0);  // Channel 2, lo/hi, mode 0, binary
    
    // Set count for ~10ms (11932 ticks at 1.193182 MHz)
    uint16_t count = 11932;
    outb(0x42, count & 0xFF);
    outb(0x42, (count >> 8) & 0xFF);
    
    // Wait for count to reach 0
    while ((inb(0x61) & 0x20) == 0);
    
    // Read TSC end
    uint64_t tsc_end = rdtsc();
    
    // Disable speaker gate
    outb(0x61, inb(0x61) & 0xFC);
    
    // Calculate frequency
    uint64_t tsc_diff = tsc_end - tsc_start;
    tsc_freq_hz = tsc_diff * 100;  // ~10ms = 100 intervals per second
    tsc_freq_khz = tsc_freq_hz / 1000;
    
    // Store boot TSC
    tsc_boot = rdtsc();
    
    vga_puts(" ");
    vga_puti((int)(tsc_freq_hz / 1000000));
    vga_puts(" MHz\n");
}

void timer_init(void) {
    // Calibrate TSC first
    tsc_calibrate();
    
    // Configure PIT for scheduler (1000Hz)
    pit_configure(PIT_DIVISOR);
}

// Called from IRQ0 handler
void timer_tick(void) {
    pit_ticks++;
}

uint64_t timer_get_tsc(void) {
    return rdtsc();
}

uint64_t timer_get_freq(void) {
    return tsc_freq_hz;
}

uint64_t timer_get_ticks(void) {
    return pit_ticks;
}

uint64_t timer_get_ns(void) {
    if (tsc_freq_khz == 0) return 0;
    uint64_t tsc = rdtsc() - tsc_boot;
    // ns = tsc * 1000000 / freq_khz
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

void timer_delay_ns(uint64_t ns) {
    if (tsc_freq_khz == 0) return;
    uint64_t tsc_target = rdtsc() + (ns * tsc_freq_khz) / 1000000ULL;
    while (rdtsc() < tsc_target) {
        asm volatile("pause");
    }
}

void timer_delay_us(uint64_t us) {
    if (tsc_freq_khz == 0) return;
    uint64_t tsc_target = rdtsc() + (us * tsc_freq_khz) / 1000ULL;
    while (rdtsc() < tsc_target) {
        asm volatile("pause");
    }
}

void timer_delay_ms(uint64_t ms) {
    if (tsc_freq_khz == 0) return;
    uint64_t tsc_target = rdtsc() + ms * tsc_freq_khz;
    while (rdtsc() < tsc_target) {
        asm volatile("pause");
    }
}
