; ==============================================================================
; STAGE2 BOOTLOADER - x86_64 Long Mode Loader
; ==============================================================================
; BSD 3-Clause License
;
; Copyright (c) 2025, NeXs Operate System
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; 3. Neither the name of the copyright holder nor the names of its
;    contributors may be used to endorse or promote products derived from
;    this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
; ==============================================================================
; Description:
;   Second stage bootloader for x86_64 architecture.
;   Performs the transition from 16-bit Real Mode to 64-bit Long Mode.
;   Key tasks:
;   - Enable A20 Line.
;   - Load Flat Binary Kernel from disk.
;   - Setup Identity Paging (0-4MB) using Huge Pages.
;   - Setup GDT for Long Mode.
;   - Activate Long Mode and transfer control to Kernel.
; ==============================================================================

[BITS 16]                   ; Start in 16-bit Real Mode
[ORG 0x7E00]                ; Loaded at 0x7E00 logic address

; ------------------------------------------------------------------------------
; Configuration Constants
; ------------------------------------------------------------------------------
KERNEL_START_SECTOR     equ 64          ; Sector index where kernel begins (after MBR & Stage2)
KERNEL_MAX_SECTORS      equ 256         ; Maximum sectors to load (Support up to 128KB kernel)
KERNEL_SECTORS_PER_READ equ 64          ; Number of sectors to read per BIOS call (32KB chunk)
KERNEL_TEMP_ADDR        equ 0x10000     ; Temporary buffer address for kernel load (64KB)
KERNEL_TEMP_SEGMENT     equ 0x1000      ; Segment corresponding to KERNEL_TEMP_ADDR (0x10000 >> 4)
KERNEL_FINAL_ADDR       equ 0x100000    ; Final physical address where kernel will run (1MB)

; Page table addresses (16KB total reserved for tables)
PML4_ADDR               equ 0x1000      ; Page Map Level 4 Table address
PDPT_ADDR               equ 0x2000      ; Page Directory Pointer Table address
PDT_ADDR                equ 0x3000      ; Page Directory Table address

; EFER MSR (Extended Feature Enable Register)
IA32_EFER               equ 0xC0000080  ; MSR index for EFER
EFER_LME                equ 0x00000100  ; Long Mode Enable bit mask

section .text               ; Code section
global _start               ; Entry point symbol

_start:
    dw 0xAA55                           ; Magic Signature for Stage1 validation check
    
    cli                                 ; Disable interrupts immediately
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00                      ; Safe stack at 0x7C00 (grows down)
    mov ds, ax                          ; Explicitly zero DS
    mov es, ax                          ; Explicitly zero ES
    mov fs, ax                          ; Explicitly zero FS
    mov gs, ax                          ; Explicitly zero GS
    mov [boot_drive], dl                ; Save boot drive number passed from Stage1
    
    mov si, msg_s2_start                ; Load start message
    call print                          ; Print message to console
    
    ; ========================================================================
    ; PHASE 1: Enable A20 Line
    ; ========================================================================
    ; Required to access memory above 1MB (20th address bit)
    call enable_a20                     ; Attempt to enable A20
    call test_a20                       ; Verify A20 is enabled
    jz error_a20                        ; Jump if Zero Flag set (A20 disabled)
    
    mov si, msg_a20                     ; Load A20 success message
    call print                          ; Print message
    
    ; ========================================================================
    ; PHASE 1.5: Detect Memory Map (E820)
    ; ========================================================================
    mov si, msg_debug_e820
    call print
    call detect_e820                    ; Call E820 memory detection routine
    
    ; ========================================================================
    ; PHASE 1.8: CPU Validation
    ; ========================================================================
    mov si, msg_debug_cpuid
    call print
    call check_cpuid                    ; Check if CPUID instruction exists
    mov si, msg_debug_lm
    call print
    call check_long_mode                ; Check if Long Mode is supported
    
    ; ========================================================================
    ; PHASE 2: Load Kernel
    ; ========================================================================
    ; Loads the Flat Binary kernel from disk to a temporary buffer
    mov si, msg_loading_kernel          ; Load loading message
    call print                          ; Print message
    
    call load_kernel                    ; function to read kernel sectors from disk
    jc error_kernel_disk                ; Jump if Carry Flag set (Disk error)
    
    mov si, msg_kernel                  ; Load kernel success message part
    call print                          ; Print message
    
    ; ========================================================================
    ; PHASE 3: Validation (Skipped for Flat Binary)
    ; ========================================================================
    ; Note: We skip ELF header validation as we are now using a raw binary format
    
    mov si, msg_elf                     ; Load binary check success message
    call print                          ; Print message
    
    ; ========================================================================
    ; PHASE 4: Setup Page Tables
    ; ========================================================================
    ; Identity map the first 16MB using 2MB huge pages to allow kernel execution
    call setup_paging                   ; Initialize paging structures
    
    mov si, msg_paging                  ; Load paging success message
    call print                          ; Print message
    
    ; ========================================================================
    ; PHASE 5: Setup GDT for Long Mode
    ; ========================================================================
    lgdt [gdt64_descriptor]             ; Load Global Descriptor Table Register (GDTR)
    
    ; ========================================================================
    ; PHASE 6: Enter Long Mode
    ; ========================================================================
    
    ; 1. Enable PAE (Physical Address Extension) in CR4
    mov eax, cr4                        ; Read Control Register 4
    or eax, 1 << 5                      ; Set PAE bit (bit 5)
    mov cr4, eax                        ; Write back to CR4
    
    ; 2. Load PML4 Base Address into CR3
    mov eax, PML4_ADDR                  ; Load PML4 address
    mov cr3, eax                        ; Write to Control Register 3 (Page Directory Base)
    
    ; 3. Enable Long Mode via EFER MSR
    mov ecx, IA32_EFER                  ; Set ECX to EFER MSR index
    rdmsr                               ; Read Model Specific Register
    or eax, EFER_LME                    ; Set Long Mode Enable (LME) bit
    wrmsr                               ; Write Model Specific Register
    
    ; 4. Enable Paging in CR0 (Activates Long Mode)
    mov eax, cr0                        ; Read Control Register 0
    or eax, 0x80000001                  ; Set Paging (PG, bit 31) and Protection Enable (PE, bit 0)
    mov cr0, eax                        ; Write back to CR0
    
    ; 5. Far Jump to flush pipeline and enter 64-bit code segment
    jmp 0x08:long_mode_entry            ; Jump to Code Segment (0x08) at 64-bit entry point

; ==============================================================================
; REAL MODE UTILITY FUNCTIONS
; ==============================================================================

enable_a20:
    ; Method 1: BIOS Int 0x15 Function 0x2401
    mov ax, 0x2401                      ; Function: Enable A20 Gate
    int 0x15                            ; Call BIOS System Services
    jnc .done                           ; If Carry Clear, success
    
    ; Method 2: Keyboard Controller (8042)
    call .wait_in                       ; Wait for input buffer empty
    mov al, 0xAD                        ; Command: Disable Keyboard
    out 0x64, al                        ; Send to Command Port
    call .wait_in                       ; Wait
    mov al, 0xD0                        ; Command: Read Output Port
    out 0x64, al                        ; Send
    call .wait_out                      ; Wait for output data
    in al, 0x60                         ; Read Output Port
    push ax                             ; Save status
    call .wait_in                       ; Wait
    mov al, 0xD1                        ; Command: Write Output Port
    out 0x64, al                        ; Send
    call .wait_in                       ; Wait
    pop ax                              ; Restore status
    or al, 2                            ; Set A20 bit (bit 1)
    out 0x60, al                        ; Write to Data Port
    call .wait_in                       ; Wait
    mov al, 0xAE                        ; Command: Enable Keyboard
    out 0x64, al                        ; Send
    call .wait_in                       ; Wait
    
    ; Method 3: Fast A20 (System Control Port A)
    in al, 0x92                         ; Read System Control Port A
    or al, 2                            ; Set Fast A20 bit
    out 0x92, al                        ; Write back
    
.done:
    ret                                 ; Return from function

.wait_in:
    in al, 0x64                         ; Read Status Register
    test al, 2                          ; Check Input Buffer Full bit
    jnz .wait_in                        ; Loop until clear
    ret

.wait_out:
    in al, 0x64                         ; Read Status Register
    test al, 1                          ; Check Output Buffer Full bit
    jz .wait_out                        ; Loop until set
    ret

test_a20:
    push es                             ; Save ES
    push ds                             ; Save DS
    xor ax, ax                          ; Zero AX
    mov es, ax                          ; ES = 0
    mov di, 0x7DFE                      ; DI = 0x7DFE (Magic number location in MBR)
    mov ax, 0xFFFF                      ; AX = 0xFFFF
    mov ds, ax                          ; DS = 0xFFFF
    mov si, 0x7E0E                      ; SI = 0x7E0E (0xFFFF:0x7E0E -> 0x107DFE)
    mov al, [es:di]                     ; Read byte at 0x0000:0x7DFE
    mov ah, [ds:si]                     ; Read byte at 0xFFFF:0x7E0E (Should be alias if A20 disabled)
    cmp al, ah                          ; Compare bytes
    jne .enabled                        ; If different, A20 is definitely enabled
    mov byte [es:di], 0x00              ; Try to modify memory at low address
    mov byte [ds:si], 0xFF              ; Modify memory at high address
    mov al, [es:di]                     ; Read back low address
    cmp al, 0xFF                        ; Did high write affect low read?
    je .disabled                        ; If yes, memory wraps -> A20 disabled
.enabled:
    or ax, 1                            ; Set ZF=0 (Success)
    jmp .done                           ; Jump to cleanup
.disabled:
    xor ax, ax                          ; Set ZF=1 (Failure)
.done:
    pop ds                              ; Restore DS
    pop es                              ; Restore ES
    ret                                 ; Return

; ==============================================================================
; E820 Memory Map Detection
; Uses BIOS INT 15h, EAX=0xE820
; Stores entries in e820_map, count in boot_info
; ==============================================================================
detect_e820:
    push es                             ; Save registers
    push di
    push ebx
    push ecx
    push edx
    
    ; Print start marker
    mov al, '>'
    call print_char

    xor ebx, ebx                        ; Continuation value (must be 0 to start)
    mov di, e820_map                    ; ES:DI = destination buffer
    
    ; Ensure ES is 0 for BIOS call transparency
    xor ax, ax
    mov es, ax 
    
    mov word [boot_info + 8], 0         ; Clear e820_count
    xor bp, bp                          ; Valid Entry counter = 0
    
.loop:
    ; Progress marker: '.'
    mov al, '.'
    call print_char

    mov eax, 0xE820                     ; Function code (Must be reset every time)
    mov ecx, 24                         ; Request 24 bytes (ACPI 3.0)
    mov edx, 0x534D4150                 ; 'SMAP' signature
    mov dword [di + 20], 1              ; Force ACPI 3.0 flag to 1 (valid) just in case
    
    int 0x15                            ; Call BIOS
    
    jc .done                            ; CF set = List Done or Error
    
    mov edx, 0x534D4150                 ; Expected signature
    cmp eax, edx                        ; Check if EAX == 'SMAP'
    jne .done                           ; If not 'SMAP', unexpected behavior -> stop

    ; VALIDATION: Check for zero length
    mov ecx, [di + 8]                   ; Low 32 bits of length
    or ecx, [di + 12]                   ; High 32 bits of length
    jz .skip_entry                      ; If length is 0, skip this entry
    
    ; Entry is valid, keep it
    mov al, '+'                         ; Print '+' for kept entry
    call print_char
    
    inc bp                              ; Increment valid count
    add di, 24                          ; Point to next slot
    
    cmp bp, 32                          ; Max entries safety check
    jge .done
    
.skip_entry:
    test ebx, ebx                       ; Check continuation value
    jnz .loop                           ; If EBX != 0, get next entry
    
.done:
    ; Print Done marker
    mov al, ' '
    call print_char
    mov al, 'O'
    call print_char
    mov al, 'K'
    call print_char
    mov si, msg_newline
    call print

    ; Store entry count
    mov word [boot_info + 8], bp
    
    ; Calculate total usable memory (Summary)
    xor eax, eax                        ; Total bytes accumulator
    mov cx, bp                          ; Count
    mov di, e820_map                    ; Reset pointer
    
    test cx, cx
    jz .sum_done
    
.sum_loop:
    cmp dword [di + 16], 1              ; Type 1 = Usable
    jne .next_sum
    add eax, [di + 8]                   ; Add Low 32-bit length
.next_sum:
    add di, 24
    dec cx
    jnz .sum_loop
    
.sum_done:
    shr eax, 20                         ; Bytes -> MB
    mov [boot_info + 12], eax
    
    pop edx
    pop ecx
    pop ebx
    pop di
    pop es
    ret

print_char:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    pop bx
    pop ax
    ret

; ==============================================================================
; CPU Validation Functions
; ==============================================================================

check_cpuid:
    pushfd                              ; Save EFLAGS
    pop eax                             ; Store EFLAGS in EAX
    mov ecx, eax                        ; Save original EFLAGS in ECX
    xor eax, 1 << 21                    ; Flip ID bit (bit 21)
    push eax                            ; Store modified EFLAGS on stack
    popfd                               ; Load modified EFLAGS to CPU
    pushfd                              ; Save EFLAGS again
    pop eax                             ; Store EFLAGS in EAX
    push ecx                            ; Restore original EFLAGS
    popfd                               ; Load original EFLAGS
    cmp eax, ecx                        ; Compare modified vs original
    je .no_cpuid                        ; If equal, ID bit didn't flip -> No CPUID
    ret
.no_cpuid:
    mov si, err_cpuid                   ; "No CPUID"
    jmp error

check_long_mode:
    ; 1. Check if Extended Functions are supported
    mov eax, 0x80000000                 ; CPUID Function 0x80000000
    cpuid                               ; Call CPUID
    cmp eax, 0x80000001                 ; Compare Max Extended Function
    jb .no_long_mode                    ; If < 0x80000001, no Long Mode info
    
    ; 2. Check for Long Mode Support
    mov eax, 0x80000001                 ; CPUID Function 0x80000001
    cpuid                               ; Call CPUID (Result in EDX)
    test edx, 1 << 29                   ; Test bit 29 (LM - Long Mode)
    jz .no_long_mode                    ; If zero, no Long Mode
    ret
.no_long_mode:
    mov si, err_long_mode               ; "No Long Mode"
    jmp error

load_kernel:
    push ax                             ; Save registers
    push bx
    push cx
    push dx
    push si
    
    xor bx, bx                          ; BX = chunk index (0)
    mov cx, KERNEL_MAX_SECTORS          ; CX = remaining sectors to read
    
.load_chunk:
    ; Determine sectors to read in this pass (Max KERNEL_SECTORS_PER_READ)
    mov ax, cx                          ; Copy remaining count
    cmp ax, KERNEL_SECTORS_PER_READ     ; Compare with max per chunk
    jbe .last_chunk                     ; If less or equal, use remaining
    mov ax, KERNEL_SECTORS_PER_READ     ; Cap at max
.last_chunk:
    
    ; Setup Disk Address Packet (DAP)
    mov byte [dap], 0x10                ; Packet Size
    mov byte [dap+1], 0                 ; Reserved
    mov word [dap+2], ax                ; Sectors to read
    mov word [dap+4], 0                 ; Offset (always 0, using Segment for address)
    
    ; Calculate Segment: KERNEL_TEMP_SEGMENT + (chunk * 0x800)
    ; 0x800 paragraphs = 32KB
    push ax                             ; Save sector count
    mov ax, bx                          ; Get chunk index
    shl ax, 11                          ; Multiply by 2048 (paragraphs in 32KB)
    add ax, KERNEL_TEMP_SEGMENT         ; Add base segment
    mov word [dap+6], ax                ; Store Segment in DAP
    pop ax                              ; Restore sector count
    
    ; Calculate LBA start sector
    push ax                             ; Save sector count
    push dx                             ; Save DX
    mov ax, bx                          ; Chunk index
    mov dx, KERNEL_SECTORS_PER_READ     ; Size of chunk
    mul dx                              ; DX:AX = chunk * 64
    add ax, KERNEL_START_SECTOR         ; Add global start offset
    adc dx, 0                           ; Add carry to high word
    mov word [dap+8], ax                ; Store LBA Low
    mov word [dap+10], dx               ; Store LBA High
    mov dword [dap+12], 0               ; Store LBA Higher (0)
    pop dx                              ; Restore DX
    pop ax                              ; Restore sector count
    
    ; Save loop counters before INT call
    push bx
    push cx
    push ax
    
    ; Execute Read
    mov ah, 0x42                        ; Extended Read Function
    mov dl, [boot_drive]                ; Drive number
    mov si, dap                         ; Pointer to DAP
    int 0x13                            ; Call BIOS
    
    ; Restore counters
    pop ax                              ; Popped sector count read
    pop cx                              ; Popped remaining count
    pop bx                              ; Popped chunk index
    
    jc .error                           ; If CF set, error happened
    
    ; Update counters
    sub cx, ax                          ; Decrement remaining sectors by amount read
    inc bx                              ; Increment chunk index
    
    ; Continue if needed
    test cx, cx                         ; Check if remaining sectors > 0
    jnz .load_chunk                     ; Loop
    
    ; Success
    clc                                 ; Clear Carry Flag
    jmp .done                           ; Exit
    
.error:
    ; Print Error Code if needed
    push ax                             ; Save error code
    mov si, msg_disk_err                ; Load error message
    call print                          ; Print message
    pop ax                              ; Restore error code
    mov al, ah                          ; Error code is in AH
    call print_hex                      ; Print hex value
    mov si, msg_newline                 ; Newline
    call print                          ; Print
    stc                                 ; Set Carry Flag (Failure)
    
.done:
    pop si                              ; Restore registers
    pop dx
    pop cx
    pop bx
    pop ax
    ret                                 ; Return

setup_paging:
    ; Zero out all page tables first to prevent garbage data
    xor eax, eax                        ; Clear EAX
    mov edi, PML4_ADDR                  ; Destination: PML4 base
    mov ecx, 4096 * 3 / 4               ; Size: 3 tables (PML4, PDPT, PDT) in dwords
    rep stosd                           ; Store EAX (0) repeatedly
    
    ; PML4[0] points to PDPT
    mov dword [PML4_ADDR], PDPT_ADDR | 3  ; Set address combined with flags (Present + Writable)
    
    ; PDPT[0] points to PDT
    mov dword [PDPT_ADDR], PDT_ADDR | 3   ; Set address combined with flags (Present + Writable)
    
    ; PDT entries: Identity Map first 1GB using Huge Pages (2MB)
    ; This maps virtual 0-1GB to physical 0-1GB
    mov edi, PDT_ADDR                   ; Destination: PDT base
    mov eax, 0x00000083                 ; Base Addr 0 + Present + Writable + Huge (2MB) bit
    mov ecx, 512                        ; Map 512 entries (512 * 2MB = 1GB total coverage)

.map_pd_loop:
    mov [edi], eax                      ; Store Page Directory Entry (Low 32-bits)
    mov [edi + 4], dword 0              ; Store High 32-bits (0)
    
    add eax, 0x200000                   ; Increment Physical Address by 2MB (0x200000)
    add edi, 8                          ; Move to next Page Table Entry (8 bytes)
    dec ecx                             ; Decrement loop counter
    jnz .map_pd_loop                    ; Repeat until 512 entries done
    
    ret                                 ; Return

print:
    push ax                             ; Save registers
    push bx
    mov ah, 0x0E                        ; Teletype function
    mov bh, 0                           ; Page 0
.l: lodsb                               ; Load byte
    test al, al                         ; Check null
    jz .d                               ; Done if zero
    int 0x10                            ; Print
    jmp .l                              ; Loop
.d: pop bx                              ; Restore registers
    pop ax
    ret                                 ; Return

print_hex:
    push ax                             ; Save registers
    push cx
    mov cl, al                          ; Save value
    shr al, 4                           ; Get high nibble
    call .nibble                        ; Print high nibble
    mov al, cl                          ; Restore value
    and al, 0x0F                        ; Get low nibble
    call .nibble                        ; Print low nibble
    pop cx                              ; Restore
    pop ax
    ret                                 ; Return
.nibble:
    add al, '0'                         ; ASCII offset
    cmp al, '9'                         ; Check if digit
    jle .out                            ; If digit, done
    add al, 7                           ; If letter, add offset (A-F)
.out:
    mov ah, 0x0E                        ; Print char
    int 0x10
    ret

error_a20:
    mov si, err_a20                     ; Load message
    jmp error                           ; Jump common error
error_kernel_disk:
    mov si, err_kern_disk               ; Load message
    jmp error                           ; Jump common error
error_elf:
    mov si, err_elf                     ; Load message
    jmp error                           ; Jump common error
error:
    call print                          ; Print specific error msg
    cli                                 ; Disable ints
    hlt                                 ; Halt
    jmp $                               ; Infinite loop

; ==============================================================================
; LONG MODE CODE (64-bit)
; ==============================================================================
[BITS 64]                               ; Switch assembler to 64-bit mode
long_mode_entry:
    ; Setup Data Segments with the new Data Selector (0x10)
    mov ax, 0x10                        ; Data Segment Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup Stack (2MB) - Sets stack pointer to grow down from 2MB mark
    mov rsp, 0x200000
    
    ; Setup IDT (Exceptions 0-31)
    call setup_idt                      ; Initialize Interrupt Descriptor Table
    
    ; Relocate Kernel to Final Address
    ; Source: 0x10000 (KERNEL_TEMP_ADDR), Dest: 0x100000 (KERNEL_FINAL_ADDR)
    ; Size: 128KB (KERNEL_MAX_SECTORS * 512)
    mov rsi, KERNEL_TEMP_ADDR           ; Source
    mov rdi, KERNEL_FINAL_ADDR          ; Destination
    mov rcx, (KERNEL_MAX_SECTORS * 512) / 8  ; Count in qwords (8 bytes)
    rep movsq                           ; Copy memory
    
    ; Clear screen (Blue background, White text)
    ; Note: This uses RDI and increments it, so we must reset RDI later
    push rax                            ; Save RAX
    mov rdi, 0xB8000                    ; Video Memory Address
    mov rcx, 80 * 25                    ; Screen size (chars)
    mov ax, 0x1F20                      ; Attribute: Blue(1)BG+White(F)FG, Char: Space(20)
    rep stosw                           ; Store words
    pop rax                             ; Restore RAX
    
    ; Pass Boot Info Struct pointer in RDI (System V AMD64 ABI 1st Argument)
    ; CRITICAL: Must be done AFTER screen clear, as stosw modifies RDI
    mov rdi, boot_info
    
    ; Re-verify Stack (Redundant but safe)
    mov rsp, 0x200000
    mov rbp, rsp                        ; Setup Base Pointer
    
    ; Jump to Kernel Entry Point (Flat Binary at 0x100000)
    mov rax, KERNEL_FINAL_ADDR          ; Load address
    call rax                            ; Absolute Call to kernel
    
    ; Should never return
    jmp $                               ; Loop if kernel returns

setup_idt:
    ; Initialize IDT for the first 32 exceptions
    mov rdi, idt_start                  ; IDT storage base
    mov rcx, 32                         ; Number of entries
    mov rbx, isr_table                  ; Table of ISR stub addresses
    
.loop:
    ; Load ISR address from table
    mov rax, [rbx]
    
    ; Build 16-byte Gate Descriptor in [RDI]
    ; Structure:
    ; Offset Low (16), Selector (16), IST/Types(16), Offset Mid(16), Offset High(32), Reserved(32)
    
    mov word [rdi], ax                  ; 0-15: Offset Low
    mov word [rdi+2], 0x08              ; 16-31: Segment Selector (0x08 = Kernel Code)
    
    ; 32-47: Flags (P=1, DPL=00, Type=0xE Interrupt Gate, IST=0) -> 0x8E00
    ; NOTE: User noted incorrect 32-bit IDT. 64-bit IDT is 16 bytes.
    ; This implementation seems correct for 64-bit:
    ; [Offset Low 16] [Selector 16] [IST 8] [Type/Attr 8] [Offset Mid 16] [Offset High 32] [Reserved 32]
    ; 0x8E00 maps to: P=1, DPL=0, Type=1110 (Int Gate). Correct.
    mov word [rdi+4], 0x8E00
    
    shr rax, 16                         ; Shift down for middle bits
    mov word [rdi+6], ax                ; 48-63: Offset Mid
    
    shr rax, 16                         ; Shift down for high bits
    mov dword [rdi+8], eax              ; 64-95: Offset High
    
    mov dword [rdi+12], 0               ; 96-127: Reserved (must be 0)
    
    ; Next entry
    add rdi, 16                         ; Advance IDT pointer 16 bytes
    add rbx, 8                          ; Advance ISR table 8 bytes (pointer size)
    dec rcx                             ; Decrement counter
    jnz .loop                           ; Loop
    
    lidt [idt_descriptor64]             ; Load IDT Register
    ret                                 ; Return

; Macro for generating ISR Stubs
%macro ISR_NOERR 1
    isr_%1:
        push 0                  ; Push dummy error code for consistency
        push %1                 ; Push Interrupt number
        jmp exception_common    ; Jump to common handler
%endmacro

%macro ISR_ERR 1
    isr_%1:
        ; Error code already pushed by CPU
        push %1                 ; Push Interrupt number
        jmp exception_common    ; Jump to common handler
%endmacro

; Define first 32 Exception Handlers
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

exception_common:
    ; Stack layout: [Ret IP], [CS], [RFLAGS], [RSP], [SS], [ERR_CODE], [INT_NUM]
    
    ; Simple VGA Exception Reporter: "EXC xx ERR yy"
    mov rdi, 0xB8000                    ; Video Memory
    mov rax, 0x4F204F204F204F20         ; Clear first line (Red background, White text)
    mov [rdi], rax                      ; Clear
    mov [rdi+8], rax                    ; Clear
    
    ; Print "EXC "
    mov byte [rdi], 'E'
    mov byte [rdi+1], 0x4F
    mov byte [rdi+2], 'X'
    mov byte [rdi+3], 0x4F
    mov byte [rdi+4], 'C'
    mov byte [rdi+5], 0x4F
    
    ; Print Interrupt Number (hex)
    mov rax, [rsp]                      ; Get Interrupt Num from stack
    mov rbx, 0xB8008                    ; Screen Offset
    call print_hex_byte                 ; Print Byte
    
    ; Print " ERR "
    mov rbx, 0xB800E                    ; Screen Offset
    mov word [rbx], 0x4F45              ; E
    mov word [rbx+2], 0x4F52            ; R
    mov word [rbx+4], 0x4F52            ; R
    
    ; Print Error Code (hex)
    mov rax, [rsp+8]                    ; Get Error Code from stack
    mov rbx, 0xB8016                    ; Screen Offset
    call print_hex_byte                 ; Print Byte
    
    cli                                 ; Disable Interrupts
    hlt                                 ; Halt CPU
    jmp $                               ; Loop

; Utility to print a byte in AH to video memory at [RBX]
print_hex_byte:
    push rax                            ; Save RAX
    shr al, 4                           ; High nibble
    call .nib                           ; Print
    mov [rbx], ax                       ; Store to video memory
    pop rax                             ; Restore RAX
    and al, 0xF                         ; Low nibble
    call .nib                           ; Print
    mov [rbx+2], ax                     ; Store to video memory
    ret                                 ; Return
.nib:
    cmp al, 9
    jbe .num
    add al, 7
.num:
    add al, '0'
    mov ah, 0x4F                        ; Red Background Attribute
    ret

align 8
isr_table:
    %assign i 0
    %rep 32
        dq isr_%+i                      ; Generate pointers to ISR stubs
        %assign i i+1
    %endrep

align 16
idt_descriptor64:
    dw 32*16 - 1                        ; Limit (Size - 1)
    dq idt_start                        ; Base Address

align 16
idt_start:
    times 32*16 db 0                    ; Reserve space for IDT entries

; ==============================================================================
; DATA SECTION
; ==============================================================================
[BITS 16]                               ; Data valid in Real Mode

boot_drive:     db 0
dap:            times 16 db 0

; GDT for Long Mode
align 16
gdt64_start:
    dq 0                                ; Null descriptor
    
    ; Code Segment (0x08): 64-bit, Present, Ring 0, Exec/Read
    ; Access: 10011010b (0x9A), Flags: 0010b (Long Mode)
    dq 0x00209A0000000000
    
    ; Data Segment (0x10): Present, Ring 0, Read/Write
    ; Access: 10010010b (0x92)
    dq 0x0000920000000000

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

; Boot Info Structure passed to Kernel
align 8
boot_info:
    dq 0xDEADBEEF                       ; Magic Signature
    dw 0                                ; e820_count (filled by detect_e820)
    dw 0                                ; reserved
    dd 0                                ; total_memory_mb (filled by detect_e820)
    dq 0                                ; secure_base (set by kernel)
    dq 0                                ; heap_base
    dq 0                                ; heap_size

; E820 Memory Map (up to 32 entries, 24 bytes each)
align 8
e820_map:
    times 32 * 24 db 0                  ; 768 bytes for E820 entries

; Messages
msg_s2_start:   db '[S2] x64 init', 0x0D, 0x0A, 0
msg_a20:        db '[S2] A20 OK', 0x0D, 0x0A, 0
msg_debug_e820: db '[S2] Detect E820...', 0x0D, 0x0A, 0
msg_debug_cpuid:db '[S2] Check CPUID...', 0x0D, 0x0A, 0
msg_debug_lm:   db '[S2] Check Long Mode...', 0x0D, 0x0A, 0
msg_loading_kernel: db '[S2] Loading kernel...', 0
msg_kernel:     db ' OK', 0x0D, 0x0A, 0
msg_elf:        db '[S2] BINARY OK', 0x0D, 0x0A, 0
msg_paging:     db '[S2] Paging OK', 0x0D, 0x0A, 0
msg_disk_err:   db ' ERR 0x', 0
msg_newline:    db 0x0D, 0x0A, 0
err_a20:        db 'A20 FAIL', 0x0D, 0x0A, 0
err_kern_disk:  db 'DISK READ FAIL', 0x0D, 0x0A, 0
err_elf:        db 'ELF64 FAIL', 0x0D, 0x0A, 0
err_cpuid:      db 'NO CPUID', 0x0D, 0x0A, 0
err_long_mode:  db 'NO LONG MODE', 0x0D, 0x0A, 0

; Padding
times (32 * 512)-($-$$) db 0            ; Pad to 16KB