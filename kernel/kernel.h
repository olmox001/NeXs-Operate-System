/*
 * kernel.h - Core Kernel Definitions and Types
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef KERNEL_H
#define KERNEL_H

// =============================================================================
// Basic Type Definitions (Standard Ints)
// =============================================================================
// We define standard C99-style types to ensure portability and clarity
// regarding variable sizes across the 64-bit architecture.

typedef unsigned char      uint8_t;     // 8-bit unsigned integer
typedef unsigned short     uint16_t;    // 16-bit unsigned integer
typedef unsigned int       uint32_t;    // 32-bit unsigned integer
typedef unsigned long long uint64_t;    // 64-bit unsigned integer (Long Mode native)
typedef signed char        int8_t;      // 8-bit signed integer
typedef signed short       int16_t;     // 16-bit signed integer
typedef signed int         int32_t;     // 32-bit signed integer
typedef signed long long   int64_t;     // 64-bit signed integer

typedef uint64_t           uintptr_t;   // Unsigned integer capable of holding a pointer


// Types not always in stdint.h
typedef unsigned long      size_t;      // Size type (64-bit on x86_64)
typedef long               ssize_t;     // Signed size type

// Standard Constants
#ifndef NULL
#define NULL ((void*)0)                 // Null pointer definition
#endif
#define true 1                          // Boolean true
#define false 0                         // Boolean false
typedef int bool;                       // Boolean type

// =============================================================================
// E820 Memory Map Definitions
// =============================================================================
// These definitions correspond to the BIOS E820 memory map format, used to
// detect available RAM at boot time.

#define E820_TYPE_USABLE    1           // Region is free for use
#define E820_TYPE_RESERVED  2           // Region is reserved by hardware
#define E820_TYPE_ACPI      3           // ACPI Reclaimable memory
#define E820_TYPE_NVS       4           // ACPI NVS memory
#define E820_TYPE_UNUSABLE  5           // Unusable memory
#define E820_MAX_ENTRIES    32          // Maximum entries we store

// Structure representing a single E820 memory map entry
struct e820_entry {
    uint64_t base;                      // Base physical address
    uint64_t length;                    // Length of the region in bytes
    uint32_t type;                      // Region type (see above)
    uint32_t attrs;                     // ACPI 3.0 Extended Attributes
} __attribute__((packed));              // No padding allowed

// =============================================================================
// Boot Info Structure
// =============================================================================
// This structure is populated by the Stage2 bootloader and passed to the
// kernel entry point. It contains critical system information.
struct boot_info {
    uint64_t magic;             // Magic number (0xDEADBEEF) for validation
    uint16_t e820_count;        // Number of valid E820 entries detected
    uint16_t reserved;          // Padding/Reserved for alignment
    uint32_t total_memory_mb;   // Total detected usable RAM in Megabytes
    uint64_t secure_base;       // Physical address of the 64KB Secure Region
    uint64_t heap_base;         // Physical start address of the Dynamic Heap
    uint64_t heap_size;         // Size of the Dynamic Heap in bytes
} __attribute__((packed));

// =============================================================================
// Kernel Configuration & Constants
// =============================================================================
#define KERNEL_VERSION      "0.0.2"     // Kernel version string
#define MAX_TASKS           64          // Maximum concurrent processes
#define MAX_MESSAGES        256         // Maximum IPC messages in system

// Memory Layout Configuration
#define KERNEL_LOAD_ADDR    0x100000    // Physical load address (1MB mark)
#define DEFAULT_HEAP_SIZE   0x100000    // Fallback heap size (1MB) if detection fails
#define SECURE_REGION_SIZE  0x10000     // 64KB reserved for encryption keys

// E820 Map Storage Location (filled by Stage2 before kernel jump)
#define E820_MAP_ADDR       0x8570      // Hardcoded location (legacy compatibility)

// =============================================================================
// Port I/O Inline Functions
// =============================================================================
// Wrappers for x86 I/O instructions to communicate with hardware ports.

// Write Byte to Port
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Read Byte from Port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Write Word (16-bit) to Port
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Read Word (16-bit) from Port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// I/O Wait (writes to unused port 0x80 to consume ~1-4 microseconds)
static inline void io_wait(void) {
    outb(0x80, 0);
}

// Disable Interrupts (Clear IF)
static inline void cli(void) { asm volatile("cli"); }

// Enable Interrupts (Set IF)
static inline void sti(void) { asm volatile("sti"); }

// Halt CPU (Wait for next interrupt)
static inline void hlt(void) { asm volatile("hlt"); }

// =============================================================================
// Debugging / Assertions
// =============================================================================

// Triggers a kernel panic, printing the message and halting the system
void kernel_panic(const char* message, const char* file, int line);

// Macro to automatically include file and line number in panic calls
#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)

// Assertion macro: If condition is false, panic
#define ASSERT(cond) do { if (!(cond)) PANIC("Assertion failed: " #cond); } while(0)

// =============================================================================
// IRQ Management
// =============================================================================
void irq_init(void);
void irq_set_apic_mode(bool enabled);

// =============================================================================
// Global Memory Variables
// =============================================================================
extern uint64_t g_total_memory;         // Total system RAM size
extern uint64_t g_heap_base;            // Start of kernel heap
extern uint64_t g_heap_size;            // Size of kernel heap
extern uint64_t g_secure_base;          // Start of secure memory region

#endif // KERNEL_H