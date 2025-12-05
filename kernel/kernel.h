#ifndef KERNEL_H
#define KERNEL_H

// Standard types for x86_64 kernel
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

// Boot info structure from bootloader
struct boot_info {
    uint64_t magic;      // 0xDEADBEEF
    uint64_t reserved;
} __attribute__((packed));

// Kernel configuration
#define KERNEL_VERSION "1.0.0"
// Move heap lower to ensure it's mapped (assuming identity map covers first ~2-4MB)
#define HEAP_START     0x200000    // 2MB (just after stack)
#define HEAP_SIZE      0x100000    // 1MB heap
#define MAX_TASKS      64
#define MAX_MESSAGES   256

// Port I/O operations
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

// Halt CPU
static inline void hlt(void) {
    asm volatile("hlt");
}

// Disable interrupts
static inline void cli(void) {
    asm volatile("cli");
}

// Enable interrupts
static inline void sti(void) {
    asm volatile("sti");
}

// Global Panic/Assert (Defined in kernel.c)
extern void kernel_panic(const char* message, const char* file, int line);

#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)
#define ASSERT(b) ((b) ? (void)0 : kernel_panic(#b, __FILE__, __LINE__))

#endif // KERNEL_H