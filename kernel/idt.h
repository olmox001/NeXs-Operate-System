/*
 * idt.h - Interrupt Descriptor Table Definitions
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

#ifndef IDT_H
#define IDT_H

#include "kernel.h"

// =============================================================================
// IDT Entry Structure (Gate Descriptor)
// =============================================================================
// Represents a single entry in the Interrupt Descriptor Table.
// Size: 16 bytes (128 bits) - Required for x86_64 Long Mode.
// Previous 32-bit entries were only 8 bytes.
struct idt_entry {
    uint16_t offset_low;    // Target Address bits 0-15
    uint16_t selector;      // Code Segment Selector in GDT (usually 0x08 for Kernel Code)
    uint8_t  ist;           // Interrupt Stack Table offset (0 = Legacy Stack Switching)
    uint8_t  type_attr;     // Type and Attributes (P, DPL, Gate Type)
                            // 0x8E = Pres=1, DPL=0, Type=Interrupt Gate (Disable Ints)
                            // 0xEE = Pres=1, DPL=3, Type=Trap Gate (Allow Ints, Callable by User)
    uint16_t offset_mid;    // Target Address bits 16-31
    uint32_t offset_high;   // Target Address bits 32-63
    uint32_t zero;          // Reserved High 32-bits (Must be 0)
} __attribute__((packed));  // Ensure compiler adds no padding

// =============================================================================
// IDT Pointer Structure
// =============================================================================
// Loaded into the IDTR register using the 'lidt' instruction.
struct idt_ptr {
    uint16_t limit;         // Size of IDT in bytes - 1
    uint64_t base;          // Virt/Phys address of the IDT array
} __attribute__((packed));

// =============================================================================
// CPU State Frame (Context)
// =============================================================================
// This structure maps exactly to the stack layout created by:
// 1. The CPU pushing SS, RSP, RFLAGS, CS, RIP (and Error Code)
// 2. The assembly ISR stub pushing GPRs (RAX...R15) and Segment types
struct interrupt_frame {
    // Pushed manually by ISR macros/common stub
    uint64_t gs, fs, es, ds;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Pushed by ISR specific stub
    uint64_t int_no;        // Interrupt Vector Number (0-255)
    uint64_t err_code;      // Error Code (0 if dummy)
    
    // Pushed automatically by CPU on interrupt
    uint64_t rip;           // Return Instruction Pointer
    uint64_t cs;            // Return Code Segment
    uint64_t rflags;        // Process Flags
    uint64_t rsp;           // Stack Pointer (at time of interrupt)
    uint64_t ss;            // Stack Segment
} __attribute__((packed));

// =============================================================================
// Function Prototypes
// =============================================================================

// Initialize the IDT and remap the PIC
void idt_init(void);

/**
 * Set a specific IDT Gate.
 * 
 * @param num Interrupt Vector Number (0-255)
 * @param handler Address of the ISR handler function (usually assembly stub)
 * @param selector Code Segment Selector (0x08)
 * @param flags Type/Attribute flags (0x8E for Kernel, 0xEE for User Syscall)
 */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

// Standard Exception Messages array (defined in idt.c)
extern const char* exception_messages[32];

#endif // IDT_H