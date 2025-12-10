/*
 * vmm.c - VMM Initialization and Address Space Management
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "vmm.h"
#include "pmm.h"
#include "libx.h"
#include "kernel.h"
#include "vga.h"

// Kernel Address Space (bootstrapped)
vm_space_t kernel_space;

// Forward declaration
void vmm_map_huge_page(vm_space_t* space, virt_addr_t virt, phys_addr_t phys, uint64_t flags);

// Constants
#define LAPIC_BASE_DEFAULT  0xFEE00000
#define IOAPIC_BASE_DEFAULT 0xFEC00000

/*
 * vmm_flush_tlb_single - Invalidate TLB entry for a single page
 */
void vmm_flush_tlb_single(virt_addr_t addr) {
    asm volatile("invlpg (%0)" :: "r" (addr) : "memory");
}

/*
 * vmm_flush_tlb_all - Reload CR3 to flush entire TLB
 */
void vmm_flush_tlb_all(void) {
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r" (val));
    asm volatile("mov %0, %%cr3" :: "r" (val) : "memory");
}

/*
 * vmm_create_space - Create a new address space (PML4)
 */
vm_space_t* vmm_create_space(void) {
    vm_space_t* space = (vm_space_t*)pmm_alloc(sizeof(vm_space_t));
    if (!space) return NULL;

    // Allocate PML4 Page
    // NOTE: pmm_alloc returns physical address (implied identity map)
    void* pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        pmm_free(space);
        return NULL;
    }
    
    memset(pml4_phys, 0, PAGE_SIZE);
    
    space->pml4 = (pte_t*)pml4_phys;
    space->pml4_phys = (phys_addr_t)pml4_phys;
    space->lock = 0;

    // TODO: Map Kernel Space into new PML4 (Higher half kernel usually, 
    // or copy top 256 entries for kernel)
    // For now, we are in identity map land, so maybe copy entire kernel mapping?
    // In strict VMM, we should have a 'kernel_pml4' template.
    
    // Copy kernel mappings from current CR3 (Quick hack for now)
    // In a real impl, we'd have a canonical kernel PML4 to copy from
    
    return space;
}

/*
 * vmm_destroy_space - Free address space resources
 */
void vmm_destroy_space(vm_space_t* space) {
    if (!space) return;
    // TODO: Recursive free of Page Tables
    // For now just free the PML4 and the struct
    // For now just free the PML4 and the struct
    // For now just free the PML4 and the struct
    pmm_free_page((void*)space->pml4_phys);
    pmm_free(space);
}

/*
 * vmm_switch_space - Load CR3 with new address space
 */
void vmm_switch_space(vm_space_t* space) {
    if (!space) return;
    asm volatile("mov %0, %%cr3" :: "r" (space->pml4_phys) : "memory");
}

/*
 * vmm_init - Initialize VMM subsystem (Rebuilds Page Tables)
 */
// Static Boot Page Tables (Guaranteed safe memory)
// 1 PML4 + 1 PDPT + 4 PDs (Covers 4GB)
static uint64_t boot_pml4[512] __attribute__((aligned(4096)));
static uint64_t boot_pdpt[512] __attribute__((aligned(4096)));
static uint64_t boot_pd[4][512] __attribute__((aligned(4096)));

/*
 * vmm_init - Initialize VMM subsystem (Rebuilds Page Tables)
 * Uses Static buffers for the initial kernel map to ensure stability.
 */
void vmm_init(struct boot_info* info) {
    // 1. Clear Static Tables
    memset(boot_pml4, 0, sizeof(boot_pml4));
    memset(boot_pdpt, 0, sizeof(boot_pdpt));
    memset(boot_pd, 0, sizeof(boot_pd));

    // 2. Setup Hierarchy Manually (Stitch Static Tables)
    uint64_t flags_tables = PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    
    // PML4[0] -> PDPT
    boot_pml4[0] = (uint64_t)boot_pdpt | flags_tables;
    
    // PDPT[0..3] -> PD[0..3] (Covers 0-4GB)
    for (int i = 0; i < 4; i++) {
        boot_pdpt[i] = (uint64_t)boot_pd[i] | flags_tables;
    }

    // 3. Initialize Kernel Space Object with Static Root
    kernel_space.pml4 = (pte_t*)boot_pml4;
    kernel_space.pml4_phys = (phys_addr_t)boot_pml4;
    kernel_space.lock = 0;

    // 4. Fill Map (Identity)
    uint64_t flags_leaf = PTE_PRESENT | PTE_WRITABLE; 
    vga_puts("[VMM] Rebuilding Page Tables (Static Bootstrap)...\n");
    
    // Map 0-32MB explicitly (Essential)
    for (uint64_t addr = 0; addr < 32 * 1024 * 1024; addr += 0x200000) {
        vmm_map_huge_page(&kernel_space, addr, addr, flags_leaf);
    }

    // Dynamic Mapping from E820 (Fills rest)
    if (info) {
        struct e820_entry* map = (struct e820_entry*)((uint64_t)info + sizeof(struct boot_info));
        for (int i = 0; i < info->e820_count; i++) {
            if (map[i].type == E820_TYPE_USABLE || map[i].type == E820_TYPE_ACPI) {
                uint64_t base = map[i].base;
                uint64_t len = map[i].length;
                uint64_t end = base + len;
                
                // Align to 2MB boundaries
                base &= ~0x1FFFFFULL;
                end = (end + 0x1FFFFFULL) & ~0x1FFFFFULL;
                
                // Skip low 32MB as already mapped
                if (base < 32 * 1024 * 1024) base = 32 * 1024 * 1024;
                
                for (uint64_t addr = base; addr < end; addr += 0x200000) {
                    vmm_map_huge_page(&kernel_space, addr, addr, flags_leaf);
                }
            }
        }
    }
    
    // 5. Map MMIO Regions (LAPIC, IOAPIC)
    vmm_map_huge_page(&kernel_space, LAPIC_BASE_DEFAULT, LAPIC_BASE_DEFAULT, flags_leaf | PTE_NOCACHE);
    vmm_map_huge_page(&kernel_space, IOAPIC_BASE_DEFAULT, IOAPIC_BASE_DEFAULT, flags_leaf | PTE_NOCACHE);
    
    vga_puts("DEBUG: Static Map Built. Switching CR3...\n");
    
    // 6. Switch to New Page Tables
    asm volatile("mov %0, %%cr3" :: "r" (kernel_space.pml4_phys) : "memory");

    vga_puts("[VMM] Switched to new CR3: ");
    vga_putx(kernel_space.pml4_phys);
    vga_puts("\n");
}
