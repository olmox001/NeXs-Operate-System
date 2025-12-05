/*
 * kernel.ld - x86_64 Kernel Linker Script
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

ENTRY(kernel_main)
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

SECTIONS {
    /* Kernel loads at 1MB (Flat Binary Load Address) */
    . = 0x100000;
    
    /* Code Section (Read-Execute) */
    .text ALIGN(4K) : {
        *(.text .text.*)
    } :text
    
    /* Read-Only Data Section */
    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }
    
    /* Read-Write Data Section */
    .data ALIGN(4K) : {
        *(.data .data.*)
    } :data
    
    /* BSS Section (Uninitialized Data) */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss .bss.*)
    }
    
    /* Stack Marker (Symbol definition for C code) */
    . = ALIGN(4K);
    _kernel_end = .;
}

PHDRS {
    text PT_LOAD FLAGS(5);  /* RX (Read + Execute) */
    data PT_LOAD FLAGS(6);  /* RW (Read + Write) */
}