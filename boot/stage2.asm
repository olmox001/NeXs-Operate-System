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

[BITS 16]
[ORG 0x7E00]

; ------------------------------------------------------------------------------
; Configuration Configuration
; ------------------------------------------------------------------------------
KERNEL_START_SECTOR     equ 64
KERNEL_MAX_SECTORS      equ 256         ; Support up to 128KB kernel
KERNEL_SECTORS_PER_READ equ 64          ; Read 64 sectors (32KB) per operation
KERNEL_TEMP_ADDR        equ 0x10000     ; Temporary load address (64KB)
KERNEL_TEMP_SEGMENT     equ 0x1000      ; Segment for temp address (KERNEL_TEMP_ADDR >> 4)
KERNEL_FINAL_ADDR       equ 0x100000    ; Final destination address (1MB)

; Page table addresses (16KB total for 2MB identity mapping)
PML4_ADDR               equ 0x1000
PDPT_ADDR               equ 0x2000
PDT_ADDR                equ 0x3000

; EFER MSR (Extended Feature Enable Register)
IA32_EFER               equ 0xC0000080
EFER_LME                equ 0x00000100  ; Long Mode Enable bit

section .text
global _start

_start:
    dw 0xAA55                           ; Signature for Stage1 validation
    
    cli                                 ; Disable interrupts
    mov [boot_drive], dl                ; Save boot drive number
    
    mov si, msg_s2_start
    call print
    
    ; ========================================================================
    ; PHASE 1: Enable A20 Line
    ; ========================================================================
    ; Required to access memory above 1MB
    call enable_a20
    call test_a20
    jz error_a20                        ; Fail if A20 is not enabled
    
    mov si, msg_a20
    call print
    
    ; ========================================================================
    ; PHASE 1.5: Detect Memory Map (E820)
    ; ========================================================================
    call detect_e820
    
    ; ========================================================================
    ; PHASE 2: Load Kernel
    ; ========================================================================
    ; Loads the Flat Binary kernel from disk to a temporary buffer
    mov si, msg_loading_kernel
    call print
    
    call load_kernel
    jc error_kernel_disk                ; Fail if disk read error
    
    mov si, msg_kernel
    call print
    
    ; ========================================================================
    ; PHASE 3: Validation (Skipped for Flat Binary)
    ; ========================================================================
    ; We skip ELF validation as we are now using a raw binary format
    
    mov si, msg_elf
    call print
    
    ; ========================================================================
    ; PHASE 4: Setup Page Tables
    ; ========================================================================
    ; Identity map the first 16MB using 2MB huge pages
    call setup_paging
    
    mov si, msg_paging
    call print
    
    ; ========================================================================
    ; PHASE 5: Setup GDT for Long Mode
    ; ========================================================================
    lgdt [gdt64_descriptor]
    
    ; ========================================================================
    ; PHASE 6: Enter Long Mode
    ; ========================================================================
    
    ; 1. Enable PAE (Physical Address Extension) in CR4
    mov eax, cr4
    or eax, 1 << 5                      ; Set PAE bit
    mov cr4, eax
    
    ; 2. Load PML4 Base Address into CR3
    mov eax, PML4_ADDR
    mov cr3, eax
    
    ; 3. Enable Long Mode via EFER MSR
    mov ecx, IA32_EFER
    rdmsr
    or eax, EFER_LME                    ; Set LME bit
    wrmsr
    
    ; 4. Enable Paging in CR0 (Activates Long Mode)
    mov eax, cr0
    or eax, 0x80000001                  ; Set PG (bit 31) and PE (bit 0)
    mov cr0, eax
    
    ; 5. Far Jump to flush pipeline and enter 64-bit code segment
    jmp 0x08:long_mode_entry

; ==============================================================================
; REAL MODE UTILITY FUNCTIONS
; ==============================================================================

enable_a20:
    ; Method 1: BIOS
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; Method 2: Keyboard Controller
    call .wait_in
    mov al, 0xAD
    out 0x64, al
    call .wait_in
    mov al, 0xD0
    out 0x64, al
    call .wait_out
    in al, 0x60
    push ax
    call .wait_in
    mov al, 0xD1
    out 0x64, al
    call .wait_in
    pop ax
    or al, 2
    out 0x60, al
    call .wait_in
    mov al, 0xAE
    out 0x64, al
    call .wait_in
    
    ; Method 3: Fast A20
    in al, 0x92
    or al, 2
    out 0x92, al
    
.done:
    ret

.wait_in:
    in al, 0x64
    test al, 2
    jnz .wait_in
    ret

.wait_out:
    in al, 0x64
    test al, 1
    jz .wait_out
    ret

test_a20:
    push es
    push ds
    xor ax, ax
    mov es, ax
    mov di, 0x7DFE
    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x7E0E
    mov al, [es:di]
    mov ah, [ds:si]
    cmp al, ah
    jne .enabled
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
    mov al, [es:di]
    cmp al, 0xFF
    je .disabled
.enabled:
    or ax, 1
    jmp .done
.disabled:
    xor ax, ax
.done:
    pop ds
    pop es
    ret

; ==============================================================================
; E820 Memory Map Detection
; Uses BIOS INT 15h, EAX=0xE820
; Stores entries in e820_map, count in boot_info
; ==============================================================================
detect_e820:
    push es
    push di
    push ebx
    push ecx
    push edx
    
    xor ebx, ebx                        ; Continuation value (0 to start)
    mov di, e820_map                    ; ES:DI = destination buffer
    xor ax, ax
    mov es, ax                          ; ES = 0 (Real mode addressing)
    add di, 0x7E00                      ; Adjust for ORG offset
    sub di, 0x7E00                      ; Actually just use absolute
    mov word [boot_info + 8], 0         ; Clear e820_count
    xor bp, bp                          ; Entry counter
    
.loop:
    mov eax, 0xE820                     ; Function code
    mov ecx, 24                         ; Entry size (24 bytes)
    mov edx, 0x534D4150                 ; 'SMAP' signature
    int 0x15
    
    jc .done                            ; CF set = error or done
    cmp eax, 0x534D4150                 ; Must return 'SMAP'
    jne .done
    
    ; Valid entry received
    inc bp                              ; Increment counter
    add di, 24                          ; Move to next entry slot
    
    cmp bp, 32                          ; Max 32 entries
    jge .done
    
    test ebx, ebx                       ; EBX=0 means last entry
    jnz .loop
    
.done:
    ; Store entry count
    mov word [boot_info + 8], bp
    
    ; Calculate total usable memory (simplified: sum type=1 entries)
    xor eax, eax                        ; Total in bytes (low 32 bits)
    mov cx, bp                          ; Entry count
    mov di, e820_map
    
.sum_loop:
    test cx, cx
    jz .sum_done
    
    cmp dword [di + 16], 1              ; Type == 1 (usable)?
    jne .skip_entry
    
    ; Add length (simplified, assumes < 4GB)
    add eax, [di + 8]                   ; Add length low dword
    
.skip_entry:
    add di, 24
    dec cx
    jmp .sum_loop
    
.sum_done:
    ; Convert to MB and store
    shr eax, 20                         ; Divide by 1MB
    mov [boot_info + 12], eax           ; Store total_memory_mb
    
    pop edx
    pop ecx
    pop ebx
    pop di
    pop es
    ret

load_kernel:
    push ax
    push bx
    push cx
    push dx
    push si
    
    xor bx, bx                          ; BX = chunk index
    mov cx, KERNEL_MAX_SECTORS          ; CX = remaining sectors
    
.load_chunk:
    ; Determine sectors to read in this pass
    mov ax, cx
    cmp ax, KERNEL_SECTORS_PER_READ
    jbe .last_chunk
    mov ax, KERNEL_SECTORS_PER_READ
.last_chunk:
    
    ; Setup Disk Address Packet (DAP)
    mov byte [dap], 0x10
    mov byte [dap+1], 0
    mov word [dap+2], ax                ; Sectors count
    mov word [dap+4], 0                 ; Offset (always 0)
    
    ; Calculate Segment: KERNEL_TEMP_SEGMENT + (chunk * 0x800)
    push ax
    mov ax, bx
    shl ax, 11                          ; chunk * 2048 paragraphs
    add ax, KERNEL_TEMP_SEGMENT
    mov word [dap+6], ax                ; Segment
    pop ax
    
    ; Calculate LBA start sector
    push ax
    push dx
    mov ax, bx
    mov dx, KERNEL_SECTORS_PER_READ
    mul dx                              ; DX:AX = chunk * 64
    add ax, KERNEL_START_SECTOR
    adc dx, 0
    mov word [dap+8], ax
    mov word [dap+10], dx
    mov dword [dap+12], 0
    pop dx
    pop ax
    
    ; Save loop counters
    push bx
    push cx
    push ax
    
    ; Execute Read
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    
    ; Restore counters
    pop ax
    pop cx
    pop bx
    
    jc .error
    
    ; Update counters
    sub cx, ax                          ; Decrement remaining sectors
    inc bx                              ; Next chunk index
    
    ; Continue if needed
    test cx, cx
    jnz .load_chunk
    
    ; Success
    clc
    jmp .done
    
.error:
    ; Print Error Code
    push ax
    mov si, msg_disk_err
    call print
    pop ax
    mov al, ah
    call print_hex
    mov si, msg_newline
    call print
    stc
    
.done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

setup_paging:
    ; Zero out all page tables first
    xor eax, eax
    mov edi, PML4_ADDR
    mov ecx, 4096 * 3 / 4
    rep stosd
    
    ; PML4[0] points to PDPT
    mov dword [PML4_ADDR], PDPT_ADDR | 3  ; Present + Writable
    
    ; PDPT[0] points to PDT
    mov dword [PDPT_ADDR], PDT_ADDR | 3   ; Present + Writable
    
    ; PDT entries: Identity Map first 16MB using Huge Pages (2MB)
    mov edi, PDT_ADDR
    mov eax, 0x00000083     ; Present + Writable + Huge (2MB)
    mov ecx, 8              ; Map 8 entries (16MB total)

.map_pd_loop:
    mov [edi], eax          ; Low 32-bits
    mov [edi + 4], dword 0  ; High 32-bits
    
    add eax, 0x200000       ; Increment Physical Address by 2MB
    add edi, 8              ; Next Entry
    dec ecx
    jnz .map_pd_loop
    
    ret

print:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
.l: lodsb
    test al, al
    jz .d
    int 0x10
    jmp .l
.d: pop bx
    pop ax
    ret

print_hex:
    push ax
    push cx
    mov cl, al
    shr al, 4
    call .nibble
    mov al, cl
    and al, 0x0F
    call .nibble
    pop cx
    pop ax
    ret
.nibble:
    add al, '0'
    cmp al, '9'
    jle .out
    add al, 7
.out:
    mov ah, 0x0E
    int 0x10
    ret

error_a20:
    mov si, err_a20
    jmp error
error_kernel_disk:
    mov si, err_kern_disk
    jmp error
error_elf:
    mov si, err_elf
    jmp error
error:
    call print
    cli
    hlt
    jmp $

; ==============================================================================
; LONG MODE CODE (64-bit)
; ==============================================================================
[BITS 64]
long_mode_entry:
    ; Setup Data Segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup Stack (2MB)
    mov rsp, 0x200000
    
    ; Setup IDT (Exceptions 0-31)
    call setup_idt
    
    ; Relocate Kernel to Final Address
    ; Source: 0x10000, Dest: 0x100000, Size: 128KB
    mov rsi, KERNEL_TEMP_ADDR
    mov rdi, KERNEL_FINAL_ADDR
    mov rcx, (KERNEL_MAX_SECTORS * 512) / 8  ; Copy in qwords (8 bytes)
    rep movsq
    
    ; Clear screen (Blue background, White text)
    ; Note: This uses RDI and increments it
    push rax
    mov rdi, 0xB8000
    mov rcx, 80 * 25
    mov ax, 0x1F20              ; Blue bg (1), White fg (F), Space (20)
    rep stosw
    pop rax
    
    ; Pass Boot Info Struct pointer in RDI
    ; CRITICAL: Must be done AFTER screen clear, as stosw modifies RDI
    mov rdi, boot_info
    
    ; Re-verify Stack (Redundant but safe)
    mov rsp, 0x200000
    mov rbp, rsp

    ; Jump to Kernel Entry Point (Flat Binary at 0x100000)
    mov rax, KERNEL_FINAL_ADDR
    call rax
    
    ; Should never return
    jmp $

setup_idt:
    ; Initialize IDT for the first 32 exceptions
    mov rdi, idt_start
    mov rcx, 32
    mov rbx, isr_table
    
.loop:
    ; Load ISR address from table
    mov rax, [rbx]
    
    ; Build 16-byte Gate Descriptor in [RDI]
    ; 0-15: Offset Low
    mov word [rdi], ax
    ; 16-31: Segment Selector (0x08 = Kernel Code)
    mov word [rdi+2], 0x08
    ; 32-47: Flags (P=1, DPL=00, Type=0xE Interrupt Gate, IST=0) -> 0x8E00
    mov word [rdi+4], 0x8E00
    ; 48-63: Offset Mid
    shr rax, 16
    mov word [rdi+6], ax
    ; 64-95: Offset High
    shr rax, 16
    mov dword [rdi+8], eax
    ; 96-127: Reserved
    mov dword [rdi+12], 0
    
    ; Next entry
    add rdi, 16
    add rbx, 8
    dec rcx
    jnz .loop
    
    lidt [idt_descriptor64]
    ret

; Macro for generating ISR Stubs
%macro ISR_NOERR 1
    isr_%1:
        push 0                  ; Dummy error code
        push %1                 ; Interrupt number
        jmp exception_common
%endmacro

%macro ISR_ERR 1
    isr_%1:
        ; Error code already pushed by CPU
        push %1                 ; Interrupt number
        jmp exception_common
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
    mov rdi, 0xB8000
    mov rax, 0x4F204F204F204F20 ; Clear first line (Red background)
    mov [rdi], rax
    mov [rdi+8], rax
    
    ; Print "EXC "
    mov byte [rdi], 'E'
    mov byte [rdi+1], 0x4F
    mov byte [rdi+2], 'X'
    mov byte [rdi+3], 0x4F
    mov byte [rdi+4], 'C'
    mov byte [rdi+5], 0x4F
    
    ; Print Interrupt Number (hex)
    mov rax, [rsp]      ; Int num
    mov rbx, 0xB8008
    call print_hex_byte
    
    ; Print " ERR "
    mov rbx, 0xB800E
    mov word [rbx], 0x4F45 ; E
    mov word [rbx+2], 0x4F52 ; R
    mov word [rbx+4], 0x4F52 ; R
    
    ; Print Error Code (hex)
    mov rax, [rsp+8]    ; Err code
    mov rbx, 0xB8016
    call print_hex_byte ; Show low byte only for compactness
    
    cli
    hlt

; Utility to print a byte in AH to video memory at [RBX]
print_hex_byte:
    push rax
    shr al, 4
    call .nib
    mov [rbx], ax
    pop rax
    and al, 0xF
    call .nib
    mov [rbx+2], ax
    ret
.nib:
    cmp al, 9
    jbe .num
    add al, 7
.num:
    add al, '0'
    mov ah, 0x4F
    ret

align 8
isr_table:
    %assign i 0
    %rep 32
        dq isr_%+i
        %assign i i+1
    %endrep

align 16
idt_descriptor64:
    dw 32*16 - 1
    dq idt_start

align 16
idt_start:
    times 32*16 db 0

; ==============================================================================
; DATA SECTION
; ==============================================================================
[BITS 16]

boot_drive:     db 0
dap:            times 16 db 0

; GDT for Long Mode
align 16
gdt64_start:
    dq 0                                ; Null descriptor
    
    ; Code Segment (0x08): 64-bit, Present, Ring 0
    dq 0x00209A0000000000
    
    ; Data Segment (0x10): Present, Ring 0
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
msg_loading_kernel: db '[S2] Loading kernel...', 0
msg_kernel:     db ' OK', 0x0D, 0x0A, 0
msg_elf:        db '[S2] BINARY OK', 0x0D, 0x0A, 0
msg_paging:     db '[S2] Paging OK', 0x0D, 0x0A, 0
msg_disk_err:   db ' ERR 0x', 0
msg_newline:    db 0x0D, 0x0A, 0
err_a20:        db 'A20 FAIL', 0x0D, 0x0A, 0
err_kern_disk:  db 'DISK READ FAIL', 0x0D, 0x0A, 0
err_elf:        db 'ELF64 FAIL', 0x0D, 0x0A, 0

; Padding
times (32 * 512)-($-$$) db 0