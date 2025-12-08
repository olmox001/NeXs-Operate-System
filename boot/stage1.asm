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

[BITS 16]                   ; Specify the code generates 16-bit instructions
[ORG 0x7C00]                ; BIOS loads MBR at physical address 0x7C00

; Configuration Constants
STAGE2_LOAD_SEGMENT     equ 0x0000      ; Segment where Stage2 will be loaded
STAGE2_LOAD_OFFSET      equ 0x7E00      ; Offset where Stage2 will be loaded (0x7C00 + 512 bytes)
STAGE2_START_SECTOR     equ 1           ; LBA Sector index to start reading (Sector 1 = 2nd sector)
STAGE2_SECTOR_COUNT     equ 32          ; Number of sectors to read (16KB for Stage2)

section .text               ; Standard code section
global _start               ; Export entry symbol (good practice for linking/debugging)

_start:
    ; 1. Fast Initialization
    ; Disable interrupts during critical setup to prevent race conditions
    cli                     ; Clear Interrupt Flag (IF)

    ; Setup segment registers to 0 for a flat memory model within Real Mode
    xor ax, ax              ; Clar AX register (AX = 0)
    mov ds, ax              ; Set Data Segment (DS) to 0
    mov es, ax              ; Set Extra Segment (ES) to 0
    mov ss, ax              ; Set Stack Segment (SS) to 0

    ; Setup stack pointer to grow down from 0x7C00 (protecting our code at 0x7C00+)
    mov sp, 0x7C00          ; Stack Pointer (SP) = 0x7C00

    ; Re-enable interrupts now that stack and segments are stable
    sti                     ; Set Interrupt Flag (IF)

    ; Save boot drive number (passed by BIOS in DL register)
    mov [boot_drive], dl    ; Store DL into memory variable 'boot_drive'

    ; 2. Print Boot Message
    mov si, msg_boot        ; Load address of boot message into Source Index (SI)
    call print              ; Call utility function to print string

    ; 3. Verify INT 13h Extensions (LBA Support)
    ; Required for loading sectors beyond the CHS limit (8GB barrier)
    mov ah, 0x41            ; Function 0x41: Check Extensions Present
    mov bx, 0x55AA          ; Magic value required for identification
    mov dl, [boot_drive]    ; Boot drive to check
    int 0x13                ; Call BIOS Disk Service
    jc error_int13          ; Jump if Carry Flag is set (Extension not supported)
    cmp bx, 0xAA55          ; Verify magic return value
    jne error_int13         ; Jump if signature mismatch (Extension not present)

    ; 4. Load Stage2 from Disk
    ; Populate Disk Address Packet (DAP) structure in memory
    mov byte [dap], 0x10          ; Size of packet (16 bytes)
    mov byte [dap+1], 0           ; Reserved (always 0)
    mov word [dap+2], STAGE2_SECTOR_COUNT ; Number of sectors to read
    mov word [dap+4], STAGE2_LOAD_OFFSET  ; Destination Offset (0x7E00)
    mov word [dap+6], STAGE2_LOAD_SEGMENT ; Destination Segment (0x0000)
    mov dword [dap+8], STAGE2_START_SECTOR ; Lower 32-bits of LBA start sector
    mov dword [dap+12], 0         ; Upper 32-bits of LBA start sector (0)

    ; Execute Extended Read (0x42) using the DAP
    mov ah, 0x42            ; Function 0x42: Extended Read Sectors from Drive
    mov dl, [boot_drive]    ; Drive number
    mov si, dap             ; DS:SI points to Disk Address Packet
    int 0x13                ; Call BIOS Disk Service
    jc error_disk           ; Jump if Carry Flag set (Disk Read Error)

    ; 5. Verify Stage2 Signature
    ; Ensure we loaded a valid bootloader stage before jumping
    cmp word [STAGE2_LOAD_OFFSET], 0xAA55 ; Check first word of loaded data for signature
    jne error_s2            ; Jump if signature invalid

    ; 6. Transfer Control to Stage2
    mov dl, [boot_drive]                      ; Pass boot drive to Stage2 (via DL)
    jmp STAGE2_LOAD_SEGMENT:STAGE2_LOAD_OFFSET ; Far jump to Stage2 entry point

; ==============================================================================
; Utility Functions
; ==============================================================================

; Function: print
; Description: Prints a null-terminated string to the screen using BIOS Teletype.
; Arguments: SI = Pointer to string
print:
    push ax                 ; Save AX register
    push bx                 ; Save BX register
    mov ah, 0x0E            ; BIOS Teletype function
    mov bh, 0               ; Page number 0
.loop:
    lodsb                   ; Load byte at DS:SI into AL, increment SI
    test al, al             ; Check if AL is zero (null terminator)
    jz .done                ; If zero, we are done
    int 0x10                ; Call BIOS Video Service (Print char in AL)
    jmp .loop               ; Repeat for next character
.done:
    pop bx                  ; Restore BX register
    pop ax                  ; Restore AX register
    ret                     ; Return from function

; ==============================================================================
; Error Handlers
; ==============================================================================

error_int13:
    mov si, err_i13         ; Load address of INT13 fail message
    call print              ; Print message
    jmp halt                ; Halt system

error_disk:
    mov si, err_dsk         ; Load address of Disk Error message
    call print              ; Print message
    jmp halt                ; Halt system

error_s2:
    mov si, err_s2          ; Load address of Stage2 invalid message
    call print              ; Print message
    jmp halt                ; Halt system

; Infinite halt loop for critical errors
halt:
    cli                     ; Disable interrupts
    hlt                     ; Halt CPU (waits for interrupt, but they are disabled)
    jmp $                   ; Infinite loop if NMI occurs

; ==============================================================================
; Data Section
; ==============================================================================

boot_drive:     db 0            ; Storage for boot drive number
dap:            times 16 db 0   ; Disk Address Packet buffer (16 bytes)

; Messages
msg_boot:       db '[S1] Boot x64', 0x0D, 0x0A, 0  ; Success + CRLF
err_i13:        db 'INT13 FAIL', 0x0D, 0x0A, 0     ; Error INT13
err_dsk:        db 'DISK ERR', 0x0D, 0x0A, 0       ; Error Disk
err_s2:         db 'S2 INVALID', 0x0D, 0x0A, 0     ; Error Stage2

; ==============================================================================
; Boot Signature
; ==============================================================================
times 510-($-$$) db 0       ; Pad remaining bytes with zeros to reach 510 bytes
dw 0xAA55                   ; Magic Boot Signature (0x55, 0xAA) required by BIOS