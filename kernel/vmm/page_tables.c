/*
 * page_tables.c - x86_64 Page Table Operations
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "vmm.h"
#include "pmm.h"  // For physical page allocation
#include "kernel.h" // For PANIC, ASSERT
#include "libx.h"   // For memset

// Helper to access physical memory (Identity Mapped in High Half or direct)
// NOTE: Since we are identity mapping 1:1 in kernel space for now,
// we can use physical addresses directly as pointers if within mapped range.
// In a real higher-half kernel, we'd need a PHYS_TO_VIRT macro.
// For NeXs OS 0.0.2, we assume 1:1 mapping for kernel structures/heap.
#define PHYS_TO_VIRT(phys) ((void*)(phys))
#define VIRT_TO_PHYS(virt) ((uintptr_t)(virt))

// Extract physical address from PTE (mask out flags)
#define PTE_ADDR(pte) ((phys_addr_t)((pte) & 0x000FFFFFFFFFF000ULL))

/*
 * vmm_get_physical - Walk the page tables to translate virtual -> physical
 */
phys_addr_t vmm_get_physical(vm_space_t* space, virt_addr_t virt) {
    if (!space || !space->pml4) return 0;

    uint16_t pml4_idx = PML4_INDEX(virt);
    uint16_t pdpt_idx = PDPT_INDEX(virt);
    uint16_t pd_idx   = PD_INDEX(virt);
    uint16_t pt_idx   = PT_INDEX(virt);

    // Level 4 (PML4)
    pte_t pml4e = space->pml4[pml4_idx];
    if (!(pml4e & PTE_PRESENT)) return 0;

    // Level 3 (PDPT)
    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pml4e));
    pte_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & PTE_PRESENT)) return 0;
    
    // Check for 1GB Huge Page
    if (pdpte & PTE_HUGE) {
        return PTE_ADDR(pdpte) + (virt & 0x3FFFFFFF);
    }

    // Level 2 (PD)
    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pdpte));
    pte_t pde = pd[pd_idx];
    if (!(pde & PTE_PRESENT)) return 0;

    // Check for 2MB Huge Page
    if (pde & PTE_HUGE) {
        return PTE_ADDR(pde) + (virt & 0x1FFFFF);
    }

    // Level 1 (PT)
    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pde));
    pte_t pte = pt[pt_idx];
    if (!(pte & PTE_PRESENT)) return 0;

    // 4KB Page
    return PTE_ADDR(pte) + (virt & PAGE_OFFSET_MASK);
}

/*
 * vmm_map_page - Map a single 4KB page
 */
void vmm_map_page(vm_space_t* space, virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    ASSERT(space && space->pml4);

    uint16_t pml4_idx = PML4_INDEX(virt);
    uint16_t pdpt_idx = PDPT_INDEX(virt);
    uint16_t pd_idx   = PD_INDEX(virt);
    uint16_t pt_idx   = PT_INDEX(virt);

    // 1. Walk PML4 -> PDPT
    pte_t* pml4e = &space->pml4[pml4_idx];
    if (!(*pml4e & PTE_PRESENT)) {
        void* new_pdpt = pmm_alloc_page(); // Allocate 1 page
        if (!new_pdpt) PANIC("VMM: Out of memory for PDPT");
        memset(new_pdpt, 0, PAGE_SIZE);
        
        // Present, Writable, User (if needed heirarchically)
        *pml4e = VIRT_TO_PHYS(new_pdpt) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    
    // 2. Walk PDPT -> PD
    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(*pml4e));
    pte_t* pdpte = &pdpt[pdpt_idx];
    if (!(*pdpte & PTE_PRESENT)) {
        void* new_pd = pmm_alloc_page();
        if (!new_pd) PANIC("VMM: Out of memory for PD");
        memset(new_pd, 0, PAGE_SIZE);

        *pdpte = VIRT_TO_PHYS(new_pd) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    // 3. Walk PD -> PT
    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(*pdpte));
    pte_t* pde = &pd[pd_idx];
    if (!(*pde & PTE_PRESENT)) {
        void* new_pt = pmm_alloc_page();
        if (!new_pt) PANIC("VMM: Out of memory for PT");
        memset(new_pt, 0, PAGE_SIZE);

        *pde = VIRT_TO_PHYS(new_pt) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else if (*pde & PTE_HUGE) {
        // ERROR: Trying to map a 4KB page into a region already covered by a 2MB Huge Page.
        // For NeXs 0.0.2, if it's identity mapped, we assume it's fine and skip.
        // TODO: Split huge page if we need to change permissions/mappings.
        // vga_puts("[VMM] Warning: Skipping map on Huge Page region\n");
        return; 
    }

    // 4. Set PT Entry
    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(*pde));
    pte_t* pte = &pt[pt_idx];

    // TODO: Handle remapping (TLB flush needed)
    
    // Combine physical address with desired flags (Present is mandatory)
    *pte = (phys & PAGE_MASK) | flags | PTE_PRESENT;
    
    // If updating current address space, flush TLB for this page
    vmm_flush_tlb_single(virt); // Call this outside or check if active space
}

/*
 * vmm_unmap_page - Remove mapping for a virtual address
 */
void vmm_unmap_page(vm_space_t* space, virt_addr_t virt) {
    ASSERT(space && space->pml4);

    uint16_t pml4_idx = PML4_INDEX(virt);
    uint16_t pdpt_idx = PDPT_INDEX(virt);
    uint16_t pd_idx   = PD_INDEX(virt);
    uint16_t pt_idx   = PT_INDEX(virt);

    // Walk...
    pte_t pml4e = space->pml4[pml4_idx];
    if (!(pml4e & PTE_PRESENT)) return;

    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pml4e));
    pte_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & PTE_PRESENT)) return;

    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pdpte));
    pte_t pde = pd[pd_idx];
    if (!(pde & PTE_PRESENT)) return;

    pte_t* pt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(pde));
    
    // Unmap
    pt[pt_idx] = 0;
    
    // Invalidate TLB
    vmm_flush_tlb_single(virt);
}

/*
 * vmm_map_huge_page - Map a 2MB huge page (Level 2)
 */
void vmm_map_huge_page(vm_space_t* space, virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    ASSERT(space && space->pml4);

    uint16_t pml4_idx = PML4_INDEX(virt);
    uint16_t pdpt_idx = PDPT_INDEX(virt);
    uint16_t pd_idx   = PD_INDEX(virt);

    // 1. Walk PML4 -> PDPT
    pte_t* pml4e = &space->pml4[pml4_idx];
    if (!(*pml4e & PTE_PRESENT)) {
        void* new_pdpt = pmm_alloc_page();
        if (!new_pdpt) PANIC("VMM: Out of memory for PDPT");
        memset(new_pdpt, 0, PAGE_SIZE);
        *pml4e = VIRT_TO_PHYS(new_pdpt) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    // 2. Walk PDPT -> PD
    pte_t* pdpt = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(*pml4e));
    pte_t* pdpte = &pdpt[pdpt_idx];
    if (!(*pdpte & PTE_PRESENT)) {
        void* new_pd = pmm_alloc_page();
        if (!new_pd) PANIC("VMM: Out of memory for PD");
        memset(new_pd, 0, PAGE_SIZE);
        *pdpte = VIRT_TO_PHYS(new_pd) | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    // 3. Set PD Entry (Level 2) with HUGE bit
    pte_t* pd = (pte_t*)PHYS_TO_VIRT(PTE_ADDR(*pdpte));
    pte_t* pde = &pd[pd_idx];
    
    // Combine physical address with flags and HUGE bit
    // Note: phys must be 2MB aligned usually, but x86 ignores lower bits for huge pages anyway?
    // Safety: ensure phys is aligned.
    *pde = (phys & 0x000FFFFFE00000ULL) | flags | PTE_PRESENT | PTE_HUGE;
    
    // Invalidate TLB (optimization: huge page might need invalidation)
    vmm_flush_tlb_single(virt);
}
