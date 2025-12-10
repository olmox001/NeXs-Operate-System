/*
 * vmm.h - Virtual Memory Manager Core Definitions
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef VMM_H
#define VMM_H

#include "kernel.h"

/* =============================================================================
 * Paging Constants & Macros (x86_64 4-Level Paging)
 * =============================================================================
 */

#define PAGE_SIZE       4096
#define PAGE_MASK       0xFFFFFFFFFFFFF000
#define PAGE_OFFSET_MASK 0xFFF

// Page Table Entry Flags
#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITETHROUGH (1ULL << 3)
#define PTE_NOCACHE     (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7) // 2MB or 1GB Page
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63) // No Execute

// Custom Flags (Available bits: 9-11, 52-62)
#define PTE_COW         (1ULL << 9)  // Copy-On-Write
#define PTE_SWAPPED     (1ULL << 10) // Page is swapped out

// Address Extraction Macros
// Virtual Address: [Sign Ext:16][PML4:9][PDPT:9][PD:9][PT:9][Offset:12]
#define PML4_INDEX(virt) (((virt) >> 39) & 0x1FF)
#define PDPT_INDEX(virt) (((virt) >> 30) & 0x1FF)
#define PD_INDEX(virt)   (((virt) >> 21) & 0x1FF)
#define PT_INDEX(virt)   (((virt) >> 12) & 0x1FF)

// Type Definitions
typedef uint64_t pte_t;
typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;

/* =============================================================================
 * Address Space Structures
 * =============================================================================
 */

typedef struct vm_space {
    pte_t* pml4;                // Virtual address of PML4 (recursive mapped or temporary mapped)
    phys_addr_t pml4_phys;      // Physical address of PML4 (for CR3)
    uint32_t lock;              // Spinlock for SMP
    // TODO: Add VMA list and stats in later milestones
} vm_space_t;

extern vm_space_t kernel_space;

/* =============================================================================
 * Public API
 * =============================================================================
 */

// Initialization
void vmm_init(struct boot_info* info);

// Page Table Operations
void vmm_map_page(vm_space_t* space, virt_addr_t virt, phys_addr_t phys, uint64_t flags);
void vmm_unmap_page(vm_space_t* space, virt_addr_t virt);
phys_addr_t vmm_get_physical(vm_space_t* space, virt_addr_t virt);

// Address Space Management
vm_space_t* vmm_create_space(void);
void vmm_destroy_space(vm_space_t* space);
void vmm_switch_space(vm_space_t* space);

// TLB Management
void vmm_flush_tlb_single(virt_addr_t addr);
void vmm_flush_tlb_all(void);

#endif // VMM_H
