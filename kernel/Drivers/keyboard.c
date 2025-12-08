/*
 * keyboard.c - PS/2 Keyboard Driver Implementation
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

#include "keyboard.h"
#include "idt.h"

// =============================================================================
// Scancode Maps (Set 1)
// =============================================================================

// Standard US QWERTY Look-up Table
static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Shifted Look-up Table (e.g., '1' -> '!')
static const char scancode_to_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// =============================================================================
// State Management
// =============================================================================
static char kbd_buffer[KBD_BUFFER_SIZE];    // Ring Buffer
static volatile uint32_t kbd_read_pos = 0;  // Head
static volatile uint32_t kbd_write_pos = 0; // Tail
static bool shift_pressed = false;
static bool ctrl_pressed = false;           // Track Ctrl key (future use)
static bool alt_pressed = false;            // Track Alt key (future use)
static bool caps_lock = false;              // Track Caps Lock toggle

/**
 * Keyboard IRQ Handler (IRQ1)
 * Called directly from 'irq_common_handler' in handlers.c
 */
void keyboard_handler(void) {
    // Read Scancode from Keyboard Controller Data Port (0x60)
    uint8_t scancode = inb(0x60);
    
    // Handle Special Keys (Shift, Ctrl, Alt, Caps)
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = true; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = false; return; }
    if (scancode == 0x1D) { ctrl_pressed = true; return; }
    if (scancode == 0x9D) { ctrl_pressed = false; return; }
    if (scancode == 0x38) { alt_pressed = true; return; }
    if (scancode == 0xB8) { alt_pressed = false; return; }
    if (scancode == 0x3A) { caps_lock = !caps_lock; return; }
    
    // Ignore Key Release Events (Bit 7 Set) for normal keys
    if (scancode & 0x80) {
        return;
    }
    
    // Map Scancode to ASCII
    char c = 0;
    if (shift_pressed || caps_lock) {
        c = scancode_to_ascii_shift[scancode];
        
        // Correct Caps Lock behavior:
        // Caps Lock inverts Shift for LETTERS only.
        // Symbols (e.g. '1' -> '!') are ONLY affected by Shift, not Caps.
        // If Caps is ON and Shift is OFF => Upper Case Letters
        // If Caps is ON and Shift is ON  => Lower Case Letters (Invert)
        // Implemented logic:
        // table_shift used. If caps_lock=1, shift=0, letter -> use table_normal??
        // Wait, standard logic:
        // Caps off, Shift off: a
        // Caps off, Shift on:  A
        // Caps on,  Shift off: A
        // Caps on,  Shift on:  a
        
        // Simplified Logic Fix:
        if (caps_lock && !shift_pressed && c >= 'A' && c <= 'Z') {
             // Already got upper from shift table?
             // No, ascii_shift has 'Q', 'W' etc.
             // If Caps is ON and Shift is OFF, we want 'Q'. Table gives 'Q'. Correct.
        }
        else if (caps_lock && !shift_pressed && !(c >= 'A' && c <= 'Z')) {
            // Caps ON, Shift OFF, Symbol (e.g. '1'). Table gives '!'.
            // Caps should NOT affect numbers. Should get '1'.
            c = scancode_to_ascii[scancode];
        }
        else if (caps_lock && shift_pressed && c >= 'A' && c <= 'Z') {
             // Caps ON, Shift ON. Should be 'q'.
             // Table gives 'Q'. We need 'q'.
             c = scancode_to_ascii[scancode];
        }
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    // Store in Buffer if valid char
    if (c) {
        // Calculate next position
        uint32_t next_pos = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
        
        // Check for overflow (drop key if full)
        if (next_pos != kbd_read_pos) {
            kbd_buffer[kbd_write_pos] = c;
            kbd_write_pos = next_pos;
        }
    }
}

/**
 * Initialize Keyboard
 */
void keyboard_init(void) {
    // Reset State
    kbd_read_pos = 0;
    kbd_write_pos = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;
    
    // Unmask IRQ1 (Keyboard Line) on PIC Master
    // 0xFD = 1111 1101 (Bit 1 cleared)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
}

/**
 * Get Character (Blocking)
 */
char keyboard_getchar(void) {
    // Spin/Halt until data available
    while (kbd_read_pos == kbd_write_pos) {
        hlt(); // Halt CPU to save energy until interrupt fires
    }
    
    // Retrieve char
    char c = kbd_buffer[kbd_read_pos];
    
    // Advance Read Pointer
    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
    
    return c;
}

/**
 * Check if Character is Available (Non-blocking)
 */
bool keyboard_available(void) {
    return kbd_read_pos != kbd_write_pos;
}

/**
 * Clear Keyboard Buffer
 */
void keyboard_clear(void) {
    // Atomic reset (interrupt safe ideally, but simple assignment is atomic enough for 32-bit aligned)
    kbd_read_pos = 0;
    kbd_write_pos = 0;
}