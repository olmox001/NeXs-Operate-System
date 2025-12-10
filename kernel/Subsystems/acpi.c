/*
 * acpi.c - ACPI Table Parsing Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "acpi.h"
#include "kernel.h"
#include "libx.h"
#include "vga.h"
#include "vmm/vmm.h" // For memory mapping if needed (using identity map for now)

// =============================================================================
// Global State
// =============================================================================

#define MAX_CPUS 32

static uint64_t g_lapic_base = 0;
static uint64_t g_ioapic_base = 0;
static uint8_t g_cpu_ids[MAX_CPUS];
static int g_cpu_count = 0;

// =============================================================================
// Helper Functions
// =============================================================================

// Checksum validation
static bool acpi_checksum(void* ptr, int length) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)ptr;
    for (int i = 0; i < length; i++) {
        sum += bytes[i];
    }
    return sum == 0;
}

// Helper to map an ACPI table into virtual memory (identitiy map for now)
static void* acpi_map_table(uint64_t phys_addr) {
    uint64_t page_addr = phys_addr & ~0xFFF;
    
    // Force map it regardless to be safe (idempotent usually)
    vmm_map_page(&kernel_space, page_addr, page_addr, PTE_PRESENT | PTE_WRITABLE);
    
    // Return virtual address (which is same as phys in identity map)
    return (void*)phys_addr;
}

// =============================================================================
// Main Parsing Logic
// =============================================================================

void acpi_init(void) {
    vga_puts("DEBUG: Searching for RSDP...\n");
    g_cpu_count = 0;

    // 1. Search for RSDP in BIOS Read-Only Memory (0xE0000 - 0xFFFFF)
    // Signature: "RSD PTR " (8 bytes)
    // Alignment: 16 bytes
    
    struct rsdp_descriptor* rsdp = NULL;
    uint64_t search_start = 0xE0000;
    uint64_t search_end = 0xFFFFF;
    
    vga_puts("DEBUG: ACPI Search Range: "); vga_putx(search_start); vga_puts(" - "); vga_putx(search_end); vga_puts("\n");

    for (uint64_t addr = search_start; addr < search_end; addr += 16) {
        // vga_puts("DEBUG: Checking "); vga_putx(addr); vga_puts("\n"); // Very verbose
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            vga_puts("DEBUG: Potential RSDP at "); vga_putx(addr); vga_puts("\n");
            if (acpi_checksum((void*)addr, 20)) { // Validate checksum of v1.0 part
                rsdp = (struct rsdp_descriptor*)addr;
                break;
            }
        }
    }
    
    if (!rsdp) {
        vga_puts("ERROR: RSDP Not Found! (SMP Disabled)\n");
        return;
    }
    
    // vga_puts("DEBUG: Found RSDP at "); vga_putx((uint64_t)rsdp); vga_puts("\n");
    
    // 2. Locate RSDT (Root System Description Table)
    // Map the RSDT header first (might cross page boundary? let's assume valid aligned access or handle)
    // The RSDT address is physical.
    uint64_t rsdt_phys = (uint64_t)rsdp->rsdt_address;
    struct rsdt* rsdt = (struct rsdt*)acpi_map_table(rsdt_phys);
    
    if (!acpi_checksum(rsdt, rsdt->header.length)) {
        vga_puts("ERROR: RSDT Checksum failed\n");
        return;
    }
    
    // 3. Find MADT (Multiple APIC Description Table)
    // Entries in RSDT are 32-bit pointers to other tables
    // Header is 36 bytes. Pointers start after header.
    
    int entries = (rsdt->header.length - sizeof(struct acpi_header)) / 4;
    struct madt* madt = NULL;
    
    for (int i = 0; i < entries; i++) {
        uint64_t table_phys = (uint64_t)rsdt->pointers[i];
        struct acpi_header* table = (struct acpi_header*)acpi_map_table(table_phys);
        
        if (memcmp(table->signature, "APIC", 4) == 0) {
            madt = (struct madt*)table;
            break;
        }
    }
    
    if (!madt) {
        vga_puts("ERROR: MADT Not Found! (SMP Disabled)\n");
        return;
    }
    
    // vga_puts("DEBUG: Found MADT at "); vga_putx((uint64_t)madt); vga_puts("\n");
    
    // 4. Parse MADT for Local APIC Address and Processor Cores
    g_lapic_base = madt->local_apic_address;
    
    uint8_t* ptr = (uint8_t*)madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.length;
    
    while (ptr < end) {
        struct madt_entry_header* header = (struct madt_entry_header*)ptr;
        
        if (header->type == MADT_TYPE_LOCAL_APIC) {
            struct madt_local_apic* lapic = (struct madt_local_apic*)ptr;
            
            // Flags Bit 0 = Enabled
            if (lapic->flags & 1) {
                if (g_cpu_count < MAX_CPUS) {
                    g_cpu_ids[g_cpu_count++] = lapic->apic_id;
                    // vga_puts("DEBUG: Found CPU APIC ID: "); vga_puti(lapic->apic_id); vga_puts("\n");
                }
            }
        }
        else if (header->type == MADT_TYPE_IO_APIC) {
            struct madt_io_apic* ioapic = (struct madt_io_apic*)ptr;
            g_ioapic_base = ioapic->io_apic_address;
            vga_puts("ACPI: Found IOAPIC at "); vga_putx(g_ioapic_base); vga_puts("\n");
        }
        
        ptr += header->length;
    }
    
    vga_puts("ACPI: Found "); vga_puti(g_cpu_count); vga_puts(" CPUs.\n");
    vga_puts("ACPI: LAPIC Base: "); vga_putx(g_lapic_base); vga_puts("\n");
}

uint64_t acpi_get_ioapic_base(void) {
    return g_ioapic_base;
}


 
// =============================================================================
// Public Getters
// =============================================================================

int acpi_get_cpu_count(void) {
    return g_cpu_count;
}

uint64_t acpi_get_lapic_base(void) {
    return g_lapic_base;
}

uint8_t acpi_get_lapic_id(int index) {
    if (index >= 0 && index < g_cpu_count) {
        return g_cpu_ids[index];
    }
    return 0xFF; // Invalid
}
