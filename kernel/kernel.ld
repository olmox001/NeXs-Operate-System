/*
 * kernel.ld - x86_64 Kernel Linker Script
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Defines the memory layout of the kernel binary.
 * Ensures the kernel is loaded at 1MB (0x100000) and sections are aligned.
 */

/* Entry Point Symbol */
ENTRY(kernel_main)

/* Output Architecture */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

SECTIONS {
    /* 
     * Load Address: 1MB (0x100000) 
     * This avoids low memory (BIOS IVT, BDA) and fits within standard RAM.
     */
    . = 0x100000;
    
    /* Code Section (Read-Execute) */
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    } :text
    
    /* Read-Only Data Section (Read-Only) */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    } :text
    
    /* Data Section (Read-Write) */
    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    } :data
    
    /* BSS Section (Uninitialized Data, Zeroed at load) */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data
    
    /* Kernel End Symbol (used by memory manager to detect heap start) */
    . = ALIGN(4K);
    _kernel_end = .;
    
    /* 
     * Note: We discard .eh_frame (exception handling frames) 
     * to reduce binary size unless C++ exceptions are needed.
     */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}

/* Program Headers (Segments) */
PHDRS {
    text PT_LOAD FLAGS(5);  /* RX (Read + Execute) */
    data PT_LOAD FLAGS(6);  /* RW (Read + Write) */
}