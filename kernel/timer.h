/*
 * timer.h - High-Precision Timer Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Uses TSC (Time Stamp Counter) for nanosecond precision.
 * PIT is used only for scheduler preemption and TSC calibration.
 */

#ifndef TIMER_H
#define TIMER_H

#include "kernel.h"

// Timer Sources
#define TIMER_SRC_PIT   0   // Programmable Interval Timer (legacy)
#define TIMER_SRC_TSC   1   // Time Stamp Counter (CPU clock)
#define TIMER_SRC_HPET  2   // High Precision Event Timer (future)

// Time Units
#define NS_PER_US   1000ULL
#define NS_PER_MS   1000000ULL
#define NS_PER_SEC  1000000000ULL

// =============================================================================
// TSC (Time Stamp Counter) - CPU Clock Precision
// =============================================================================

// Read TSC (64-bit cycle counter)
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Read TSC with serialization (more accurate)
static inline uint64_t rdtscp(void) {
    uint32_t lo, hi, aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((uint64_t)hi << 32) | lo;
}

// =============================================================================
// Timer API
// =============================================================================

// Initialize timer subsystem (calibrates TSC using PIT)
void timer_init(void);

// Get current time in various units
uint64_t timer_get_ns(void);       // Nanoseconds since boot
uint64_t timer_get_us(void);       // Microseconds since boot
uint64_t timer_get_ms(void);       // Milliseconds since boot
uint64_t timer_get_sec(void);      // Seconds since boot

// Get raw TSC value
uint64_t timer_get_tsc(void);

// Get TSC frequency (Hz)
uint64_t timer_get_freq(void);

// High-precision delays
void timer_delay_ns(uint64_t ns);
void timer_delay_us(uint64_t us);
void timer_delay_ms(uint64_t ms);

// PIT tick count (for scheduler, 1000Hz)
uint64_t timer_get_ticks(void);

// =============================================================================
// Legacy Compatibility
// =============================================================================
#define get_timer_ticks() timer_get_ticks()
#define delay_ms(ms) timer_delay_ms(ms)

#endif // TIMER_H
