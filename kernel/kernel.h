/*
 * kernel.h - Core Kernel Definitions
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, NeXs Operate System
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KERNEL_H
#define KERNEL_H

// Standard Fixed-Width Integer Types
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

// Standard Constants and Boolean Type
#define NULL ((void*)0)
#define true 1
#define false 0
typedef int bool;

// Boot Info Structure (Must match Stage 2 definition)
struct boot_info {
    uint64_t magic;      // Magic Signature (0xDEADBEEF)
    uint64_t reserved;   // Reserved / Padding
} __attribute__((packed));

// Kernel Configuration Headers
#define KERNEL_VERSION "0.0.1"

// Memory Layout Constants
// Note: Heap is dynamically placed after kernel code in kernel_main
#define HEAP_SIZE      0x100000    // 1MB default heap size
#define MAX_TASKS      64
#define MAX_MESSAGES   256

/*
 * Port I/O Inline Functions
 */

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

// IO Wait (Small delay for slow hardware)
static inline void io_wait(void) {
    outb(0x80, 0); // Write to unused port 0x80
}

/*
 * CPU Control Intrinsics
 */

// Halt CPU (Wait for Interrupt)
static inline void hlt(void) {
    asm volatile("hlt");
}

// Disable Interrupts
static inline void cli(void) {
    asm volatile("cli");
}

// Enable Interrupts
static inline void sti(void) {
    asm volatile("sti");
}

/*
 * Debugging / Error Handling Macros
 */

// External Panic Handler Definition
extern void kernel_panic(const char* message, const char* file, int line);

#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)
#define ASSERT(b) ((b) ? (void)0 : kernel_panic(#b, __FILE__, __LINE__))


#endif // KERNEL_H
