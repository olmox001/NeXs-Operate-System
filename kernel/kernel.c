/*
 * kernel.c - x86_64 Kernel Main Entry Point
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

#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "libc.h"
#include "buddy.h"
#include "idt.h"
#include "keyboard.h"
#include "messages.h"
#include "permissions.h"
#include "shell.h"
#include "process.h"
#include "syscall.h"

// External IRQ initialization (defined in handlers.c or interrupts.asm)
void irq_init(void);

// Print startup banner to console
static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("\n");
    vga_puts("  ========================================\n");
    vga_puts("   x86_64 Kernel ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_puts("  ========================================\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

// Print initialization step status
static void print_init(const char* component, bool success) {
    vga_puts("  [");
    if (success) {
        vga_set_color(VGA_GREEN, VGA_BLACK);
        vga_puts(" OK ");
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("FAIL");
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("] ");
    vga_puts(component);
    vga_putc('\n');
}

/**
 * Main Kernel Entry Point
 * This function is called from the assembly stub `_start` in kernel_entry.asm.
 */
void kernel_main(struct boot_info* info) {
    // 1. Initialize Serial Port FIRST for debug logging (Headless support)
    serial_init();
    
    // 2. Initialize VGA Driver (now mirrors to serial)
    vga_init();
    
    // Dual log to confirm entry
    vga_puts("DEBUG: Entered kernel_main\n");
    
    // 3. Validate Boot Information Structure
    // This structure is passed in RDI by the bootloader (Stage 2)
    if (info == NULL) {
        vga_puts("WARNING: info is NULL (Bootloader Issue?)\n");
    } else {
        vga_puts("Info Ptr: "); vga_putx((uint64_t)info); vga_puts("\n");
        vga_puts("Magic: "); vga_putx(info->magic); vga_puts("\n");
        vga_puts("Expected: "); vga_putx(0xDEADBEEF); vga_puts("\n");
    }

    if (!info || info->magic != 0xDEADBEEF) {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("ERROR: Invalid boot info magic (soft pass)\n");
        // We continue anyway to see how far we can get
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        vga_puts("Boot Info OK\n");
    }
    
    // 4. Display Welcome Banner
    print_banner();
    
    // DEBUG: Direct write to video memory (Green 'X' visualization check)
    *((volatile uint16_t*)0xB8050) = 0x2F58; // 2F = White on Green, 58 = 'X'
    
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Initializing kernel subsystems...\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    // 5. Initialize Interrupt Descriptor Table (IDT)
    vga_puts("DEBUG: Init IDT...\n");
    idt_init();
    print_init("Interrupt Descriptor Table", true);
    
    // 6. Initialize Interrupt Requests (IRQ)
    vga_puts("DEBUG: Init IRQ...\n");
    irq_init();
    print_init("IRQ Handlers", true);
    
    // 7. Initialize Memory Manager (Buddy Allocator with E820)
    vga_puts("DEBUG: Init Buddy...\n");
    
    // Get E820 map from boot_info
    struct boot_info* boot = info;
    struct e820_entry* e820_entries = (struct e820_entry*)((uint64_t)info + sizeof(struct boot_info));
    
    // Display detected memory
    vga_puts("      E820 entries: ");
    vga_puti(boot->e820_count);
    vga_puts(", Total: ");
    vga_puti(boot->total_memory_mb);
    vga_puts(" MB\n");
    
    // Initialize buddy with E820 (reserves secure region)
    uint64_t secure_base = 0;
    if (boot->e820_count > 0) {
        buddy_init_e820(e820_entries, boot->e820_count, &secure_base);
    } else {
        // Fallback to static allocation
        extern uint64_t _kernel_end;
        uint64_t heap_start_addr = ((uint64_t)&_kernel_end + 4095) & ~4095;
        buddy_init((void*)heap_start_addr, 0x80000);
    }
    
    print_init("Memory Allocator (Buddy)", true);
    
    // Display Memory Statistics
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);
    vga_puts("      Heap: ");
    vga_puti(total / 1024);
    vga_puts(" KB");
    if (secure_base) {
        vga_puts(" | Secure: 64 KB");
    }
    vga_putc('\n');
    
    // 8. Initialize Drivers & Subsystems
    vga_puts("DEBUG: Init Keyboard...\n");
    keyboard_init();
    print_init("PS/2 Keyboard Driver", true);
    
    msg_init();
    print_init("IPC Message System", true);
    
    perm_init();
    print_init("Capability System (Royalty)", true);
    vga_puts("      Task 0 (kernel): All permissions\n");
    
    // Create a demo task with user permissions
    int result = perm_create_task(1, 0, 
        PERM_MEMORY_ALLOC | PERM_MEMORY_FREE | 
        PERM_MSG_SEND | PERM_MSG_RECEIVE |
        PERM_SHELL_ACCESS);
    if (result == 0) {
        vga_puts("      Task 1 created with user permissions\n");
    }
    
    vga_putc('\n');
    vga_set_color(VGA_GREEN, VGA_BLACK);
    vga_puts("==> Kernel initialization complete!\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    // 9. Start Multitasking (Priority Scheduler)
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("DEBUG: Scheduler Init...\n");
    scheduler_init();
    print_init("Priority Scheduler", true);
    
    // Initialize Syscalls (INT 0x80)
    syscall_init();
    print_init("Syscall Interface (POSIX)", true);
    
    // Initialize Shell State
    shell_init();
    
    // Spawn Shell as HIGH priority task (interactive)
    struct task* shell_task = task_create_priority(shell_run, PRIORITY_HIGH);
    if (shell_task) {
        shell_task->perm_mask = PERM_SHELL_ACCESS | PERM_MSG_SEND | PERM_MSG_RECEIVE;
        vga_puts("      Shell Task (PID ");
        vga_puti(shell_task->pid);
        vga_puts(") Priority: HIGH\n");
    }
    print_init("Multitasking System", true);

    vga_puts("Enabling Interrupts...\n");
    sti();
    
    // The Kernel Main (Task 0) becomes the Idle task
    // Priority IDLE, runs only when no other task is READY
    vga_puts("Ready.\n\n");
    while(1) {
        hlt();
    }
}

/**
 * Global Kernel Panic Handler.
 * Called when a critical error occurs.
 */
void kernel_panic(const char* message, const char* file, int line) {
    // Disable interrupts to stop the world
    cli();

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n\n!! KERNEL PANIC !!\n");
    vga_puts("Reason: ");
    vga_puts(message);
    vga_puts("\nFile:   ");
    vga_puts(file);
    vga_puts("\nLine:   ");
    vga_puti(line);
    vga_puts("\n\n");
    
    vga_puts("Attempting soft recovery...\n");
    
    // Busy wait delay
    for(volatile int i = 0; i < 10000000; i++); 

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Restarting Shell...\n");
    
    // Re-enable interrupts
    sti();
    
    // Try to restart the shell
    shell_init();
    shell_run();
    
    // If recovery fails, hard halt
    vga_set_color(VGA_RED, VGA_BLACK);
    vga_puts("System Halted (Recovery Failed).");
    while(1) hlt();
}