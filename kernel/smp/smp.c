/*
 * smp.c - SMP Initialization and AP Bootstrap
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "smp.h"
#include "vmm/pmm.h"
#include "vmm/vmm.h"
#include "Subsystems/acpi.h"
#include "Subsystems/apic.h"
#include "libx.h"
#include "vga.h"
#include "kernel.h"
#include "idt.h" // For idt_load()
#include "timer.h" // For delay/sleep

// Trampoline Binary Symbols (from linker)
extern char _binary_kernel_smp_trampoline_bin_start[];
extern char _binary_kernel_smp_trampoline_bin_end[];
extern char _binary_kernel_smp_trampoline_bin_size[];

// Configuration Offsets in Trampoline (Must match trampoline.asm)
#define TR_OFFSET_PML4      0x10
#define TR_OFFSET_STACK     0x18
#define TR_OFFSET_CODE      0x20

// State
static volatile int g_cpus_booted = 1; // BSP is already up

// Global Per-CPU State Array
cpu_info_t cpus[MAX_CPUS];

// =============================================================================
// Helpers
// =============================================================================

// Defines 16KB stack per CPU
#define CPU_STACK_SIZE 16384

// =============================================================================
// AP Entry Point (C)
// =============================================================================

void ap_main(void) {
    // VISUAL DEBUG: 'A' (Alive) at 0xB8002 of VGA (Top Left + 2 chars)
    // 0xB8002 = Row 0, Col 1.
    // Use raw pointer to avoid locking issues in vga driver initially
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    vga[1] = 0x0E00 | 'A'; // Yellow 'A' on Black
    
    // DEBUG: Confirm entry via Serial
    vga_puts("DEBUG: AP_MAIN ENTERED\n");
    
    // 1. Load IDT (Use BSP's IDT)
    idt_load();
    vga_puts("d1 ");
    
    // 2. Enable LAPIC
    lapic_init();
    vga_puts("d2 ");
    
    // 3. Mark Online
    // Atomic Increment
    __sync_fetch_and_add(&g_cpus_booted, 1);
    
    vga[2] = 0x0A00 | 'K'; // Green 'K' (Kern)
    
    // 4. Initialize AP Scheduler (Per-Core Idle Task)
    // CRITICAL: Must be done BEFORE enabling interrupts/timer!
    extern void scheduler_ap_init(void);
    scheduler_ap_init();

    // 5. Enable LAPIC Timer (Vector 127)
    // This allows PREEMPTION on this AP!
    // We use Vector 127 to avoid conflict with PIT/IRQ0 (Vector 32)
    lapic_timer_init(127);

    // 5. Enable Interrupts
    asm volatile("sti");
    
    // 5. Idle Loop
    while(1) {
        asm volatile("hlt");
    }
}

void smp_init(void) {
    // 1. Check ACPI for CPU Count
    acpi_init();
    int total_cpus = acpi_get_cpu_count();
    
    if (total_cpus <= 1) {
        vga_puts("SMP: Single Core Detected. Skipping AP Boot.\n");
        return;
    }
    
    // 2. Prepare Trampoline
    // Copy binary to 0x8000
    size_t tr_size = (size_t)_binary_kernel_smp_trampoline_bin_size;
    
    memcpy((void*)TRAMPOLINE_ADDR, _binary_kernel_smp_trampoline_bin_start, tr_size);
    
    // FLUSH CACHE: Ensure APs see the code and data we just wrote!
    asm volatile("wbinvd" ::: "memory");
    
    // 3. Configure Trampoline Defaults
    uint64_t* tr_pml4  = (uint64_t*)(TRAMPOLINE_ADDR + TR_OFFSET_PML4);
    uint64_t* tr_stack = (uint64_t*)(TRAMPOLINE_ADDR + TR_OFFSET_STACK);
    uint64_t* tr_code  = (uint64_t*)(TRAMPOLINE_ADDR + TR_OFFSET_CODE);
    
    // Set Shared Kernel PML4 (BSP's CR3)
    *tr_pml4 = kernel_space.pml4_phys;
    
    // Set Entry Point
    *tr_code = (uint64_t)&ap_main;
    
    // 4. Boot APs Sequentially
    vga_puts("SMP: Booting "); vga_puti(total_cpus - 1); vga_puts(" Application Processors...\n");
    
    // CRITICAL: Initialize LAPIC for BSP first!
    // This sets 'g_lapic_addr' so that lapic_write() actually works.
    lapic_init(); 
    
    // Now that LAPIC is safe and mapped, enable SMP mode in PMM
    // This allows pmm_alloc to use per-cpu caches via lapic_id()
    pmm_enable_smp(); 
    
    int bsp_id = lapic_id();
    
    for (int i = 0; i < total_cpus; i++) {
        uint8_t apic_id = acpi_get_lapic_id(i);
        if (apic_id == bsp_id) continue; // Skip BSP
        
        vga_puts("  - Waking APIC ID "); vga_puti(apic_id); vga_puts("... ");
        
        // Capture start count BEFORE sending signals (APs might boot fast)
        int start_count = g_cpus_booted;
        
        // Allocate Stack for this AP
        void* stack_bottom = pmm_alloc(CPU_STACK_SIZE);
        
        if (!stack_bottom) {
            vga_puts("[FAIL] Stack Alloc\n");
            continue;
        }
        
        // Set Stack Pointer in Trampoline (Top of stack)
        *tr_stack = (uint64_t)stack_bottom + CPU_STACK_SIZE;
        
        // --- INIT IPI ---
        lapic_send_init(apic_id);
        timer_delay_ms(10);
        
        // --- SIPI IPI (Vector 0x08 -> 0x8000) ---
        lapic_send_sipi(apic_id, 0x08);
        timer_delay_ms(200); // Give AP time to process Trampoline (Real Mode is slow)
        
        // --- SIPI IPI (Retry if needed) ---
        if (g_cpus_booted == start_count) {
            lapic_send_sipi(apic_id, 0x08);
            timer_delay_ms(100);
        }
        
        // Wait for check-in
        int timeout = 2000;
        
        while (g_cpus_booted == start_count && timeout > 0) {
            timer_delay_ms(1);
            timeout--;
        }
        
        while (g_cpus_booted == start_count && timeout > 0) {
            timer_delay_ms(1);
            timeout--;
        }
        
        if (g_cpus_booted > start_count) {
             vga_puts("[OK]\n");
        } else {
            vga_puts("[TIMEOUT]\n");
        }
    }
    
    vga_puts("SMP: Total CPUs Online: "); vga_puti(g_cpus_booted); vga_puts("\n");
}

int smp_get_cpu_count(void) {
    return g_cpus_booted;
}

int smp_get_id(void) {
    return lapic_id();
}
