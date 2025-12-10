/*
 * acpi.h - ACPI Table Definitions and Parsing
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef ACPI_H
#define ACPI_H

#include "kernel.h"

// =============================================================================
// ACPI Data Structures
// =============================================================================

// RSDP (Root System Description Pointer)
struct rsdp_descriptor {
    char signature[8];          // "RSD PTR "
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;           // 0 = 1.0, 2 = 2.0+
    uint32_t rsdt_address;      // Physical address of RSDT
    
    // XSDP (Revision 2.0+)
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

// ACPI Table Header (Common)
struct acpi_header {
    char signature[4];          // e.g. "APIC", "RSDT"
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

// RSDT (Root System Description Table)
struct rsdt {
    struct acpi_header header;
    uint32_t pointers[];        // Variable length
} __attribute__((packed));

// MADT (Multiple APIC Description Table)
struct madt {
    struct acpi_header header;
    uint32_t local_apic_address; // Physical address of Local APIC
    uint32_t flags;              // 1 = PCAT_COMPAT
    uint8_t entries[];           // Variable length entries
} __attribute__((packed));

// MADT Entry Types
#define MADT_TYPE_LOCAL_APIC    0
#define MADT_TYPE_IO_APIC       1
#define MADT_TYPE_INT_SRC       2

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_local_apic {
    struct madt_entry_header header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;             // Bit 0 = Processor Enabled
} __attribute__((packed));

struct madt_io_apic {
    struct madt_entry_header header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed));

// =============================================================================
// Public API
// =============================================================================

// Find and Parse ACPI tables to initialize SMP
void acpi_init(void);

// Get number of detected CPUs
int acpi_get_cpu_count(void);

// Get Local APIC Address
uint64_t acpi_get_lapic_base(void);

// Get CPU ID by index (0 to N-1)
uint8_t acpi_get_lapic_id(int index);

// Get IOAPIC Address
uint64_t acpi_get_ioapic_base(void);

#endif // ACPI_H
