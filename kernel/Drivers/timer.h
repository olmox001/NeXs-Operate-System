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

// =============================================================================
// Timer Configuration
// =============================================================================

#define TIMER_Src_PIT   0   // Programmable Interval Timer (Legacy)
#define TIMER_Src_TSC   1   // Time Stamp Counter (CPU Cycle Counter)
#define TIMER_Src_HPET  2   // High Precision Event Timer (Modern)

// Nanosecond Multipliers
#define NS_PER_US   1000ULL
#define NS_PER_MS   1000000ULL
#define NS_PER_SEC  1000000000ULL

// =============================================================================
// TSC Helpers
// =============================================================================

/**
 * Read Time Stamp Counter (RDTSC)
 * Returns the raw 64-bit cycle count since CPU reset.
 */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * Read Time Stamp Counter with Serialization (RDTSCP)
 * Ensures no out-of-order execution affects timing measurement.
 */
static inline uint64_t rdtscp(void) {
    uint32_t lo, hi, aux;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return ((uint64_t)hi << 32) | lo;
}

// =============================================================================
// Public API
// =============================================================================

// Initialization
void timer_init(void);

// Time Retrieval (Relative to Boot)
uint64_t timer_get_ns(void);       // Nanoseconds
uint64_t timer_get_us(void);       // Microseconds
uint64_t timer_get_ms(void);       // Milliseconds
uint64_t timer_get_sec(void);      // Seconds

// Raw Hardware Stats
uint64_t timer_get_tsc(void);
uint64_t timer_get_freq(void);     // TSC Frequency in Hz
uint64_t timer_get_ticks(void);    // PIT Tick Count (scheduler ticks)

// Delays (Blocking Spins)
void timer_delay_ns(uint64_t ns);
void timer_delay_us(uint64_t us);
void timer_delay_ms(uint64_t ms);

// Legacy/Convenience Aliases
#define get_timer_ticks() timer_get_ticks()
#define delay_ms(ms) timer_delay_ms(ms)

#endif // TIMER_H
