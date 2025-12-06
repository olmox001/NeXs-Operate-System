/*
 * kernel.h - Core Kernel Definitions
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef KERNEL_H
#define KERNEL_H

// =============================================================================
// Basic Types
// =============================================================================
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef unsigned long      size_t;
typedef long               ssize_t;

#define NULL ((void*)0)
#define true 1
#define false 0
typedef int bool;

// =============================================================================
// E820 Memory Map Entry
// =============================================================================
#define E820_TYPE_USABLE    1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI      3
#define E820_TYPE_NVS       4
#define E820_TYPE_UNUSABLE  5
#define E820_MAX_ENTRIES    32

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attrs;
} __attribute__((packed));

// =============================================================================
// Boot Info Structure (Must match Stage2 definition)
// =============================================================================
struct boot_info {
    uint64_t magic;             // 0xDEADBEEF
    uint16_t e820_count;        // Number of E820 entries
    uint16_t reserved;
    uint32_t total_memory_mb;   // Total usable RAM in MB
    uint64_t secure_base;       // Secure key storage base (set by kernel)
    uint64_t heap_base;         // Heap start address
    uint64_t heap_size;         // Heap size in bytes
} __attribute__((packed));

// =============================================================================
// Kernel Configuration
// =============================================================================
#define KERNEL_VERSION      "0.0.2"
#define MAX_TASKS           64
#define MAX_MESSAGES        256

// Memory Layout
#define KERNEL_LOAD_ADDR    0x100000    // 1MB
#define DEFAULT_HEAP_SIZE   0x100000    // 1MB fallback
#define SECURE_REGION_SIZE  0x10000     // 64KB for keys

// E820 map location (filled by Stage2)
#define E820_MAP_ADDR       0x8570      // After boot_info

// =============================================================================
// Port I/O
// =============================================================================
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

// =============================================================================
// CPU Control
// =============================================================================
static inline void cli(void) { asm volatile("cli"); }
static inline void sti(void) { asm volatile("sti"); }
static inline void hlt(void) { asm volatile("hlt"); }

// =============================================================================
// Debugging / Assertions
// =============================================================================
void kernel_panic(const char* message, const char* file, int line);

#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)
#define ASSERT(cond) do { if (!(cond)) PANIC("Assertion failed: " #cond); } while(0)

// =============================================================================
// Global Memory Info (set at boot)
// =============================================================================
extern uint64_t g_total_memory;
extern uint64_t g_heap_base;
extern uint64_t g_heap_size;
extern uint64_t g_secure_base;

#endif // KERNEL_H