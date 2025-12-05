/* kernel.ld - x86_64 Kernel Linker Script */

ENTRY(kernel_main)
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

SECTIONS {
    /* Kernel loads at 1MB */
    . = 0x100000;
    
    /* Read-only code section */
    .text ALIGN(4K) : {
        *(.text .text.*)
    } :text
    
    /* Read-only data section */
    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }
    
    /* Read-write data section */
    .data ALIGN(4K) : {
        *(.data .data.*)
    } :data
    
    /* BSS (uninitialized data) */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss .bss.*)
    }
    
    /* Stack section (grows down from 2MB) */
    . = ALIGN(4K);
    _kernel_end = .;
}

PHDRS {
    text PT_LOAD FLAGS(5);  /* RX */
    data PT_LOAD FLAGS(6);  /* RW */
}