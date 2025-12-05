; ==============================================================================
; STAGE1 BOOTLOADER - Master Boot Record (MBR)
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
;   First stage bootloader (MBR) for x86_64 architecture.
;   It fits within 512 bytes, initializes the environment, detects LBA support,
;   loads the second stage (Stage2) from disk, and transfers control to it.
; ==============================================================================

[BITS 16]
[ORG 0x7C00]

; Configuration Constants
STAGE2_LOAD_SEGMENT     equ 0x0000
STAGE2_LOAD_OFFSET      equ 0x7E00
STAGE2_START_SECTOR     equ 1
STAGE2_SECTOR_COUNT     equ 32          ; 16KB for Stage2 (enough for Long Mode setup)

section .text
global _start

_start:
    ; 1. Fast Initialization
    ; Disable interrupts during setup to prevent race conditions
    cli
    
    ; Setup segment registers to 0
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Setup stack pointer to grow down from 0x7C00 (protecting our code)
    mov sp, 0x7C00
    
    ; Re-enable interrupts
    sti
    
    ; Save boot drive number (passed by BIOS in DL)
    mov [boot_drive], dl
    
    ; 2. Print Boot Message
    mov si, msg_boot
    call print
    
    ; 3. Verify INT 13h Extensions (LBA Support)
    ; Required for loading sectors beyond the CHS limit
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc error_int13              ; Carry flag set = Not supported
    cmp bx, 0xAA55
    jne error_int13             ; Signature mismatch
    
    ; 4. Load Stage2 from Disk
    ; Populate Disk Address Packet (DAP)
    mov byte [dap], 0x10        ; Size of packet (16 bytes)
    mov byte [dap+1], 0         ; Reserved (always 0)
    mov word [dap+2], STAGE2_SECTOR_COUNT
    mov word [dap+4], STAGE2_LOAD_OFFSET
    mov word [dap+6], STAGE2_LOAD_SEGMENT
    mov dword [dap+8], STAGE2_START_SECTOR
    mov dword [dap+12], 0       ; Upper 32-bits of LBA (0 for now)
    
    ; Execute Extended Read (0x42)
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc error_disk               ; Carry flag set = Read failure
    
    ; 5. Verify Stage2 Signature
    ; Ensure we loaded a valid bootloader stage
    cmp word [STAGE2_LOAD_OFFSET], 0xAA55
    jne error_s2
    
    ; 6. Transfer Control to Stage2
    mov dl, [boot_drive]        ; Pass boot drive to Stage2
    jmp STAGE2_LOAD_SEGMENT:STAGE2_LOAD_OFFSET

; ==============================================================================
; Utility Functions
; ==============================================================================

; Function: print
; Description: Prints a null-terminated string to the screen using BIOS Teletype.
; Arguments: SI = Pointer to string
print:
    push ax
    push bx
    mov ah, 0x0E        ; BIOS Teletype function
    mov bh, 0           ; Page number
.loop: 
    lodsb               ; Load byte at DS:SI into AL, increment SI
    test al, al         ; Check for null terminator
    jz .done
    int 0x10            ; Print character
    jmp .loop
.done: 
    pop bx
    pop ax
    ret

; ==============================================================================
; Error Handlers
; ==============================================================================

error_int13:
    mov si, err_i13
    call print
    jmp halt

error_disk:
    mov si, err_dsk
    call print
    jmp halt

error_s2:
    mov si, err_s2
    call print
    jmp halt

; Infinite halt loop
halt:
    cli
    hlt
    jmp $

; ==============================================================================
; Data Section
; ==============================================================================

boot_drive:     db 0
dap:            times 16 db 0   ; Disk Address Packet buffer

; Messages
msg_boot:       db '[S1] Boot x64', 0x0D, 0x0A, 0
err_i13:        db 'INT13 FAIL', 0x0D, 0x0A, 0
err_dsk:        db 'DISK ERR', 0x0D, 0x0A, 0
err_s2:         db 'S2 INVALID', 0x0D, 0x0A, 0

; ==============================================================================
; Boot Signature
; ==============================================================================
times 510-($-$$) db 0   ; Pad remaining bytes with zeros

dw 0xAA55               ; Magic Boot Signature
