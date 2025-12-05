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

// Scancode Set 1 (US QWERTY) Mapping
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

// Scancode Set 1 (Shifted) Mapping
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

// Keyboard State Management
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_read_pos = 0;
static volatile uint32_t kbd_write_pos = 0;
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

/**
 * Keyboard IRQ Handler (Called from ISR Stub)
 */
void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    
    // Check for Modifier Keys
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }
    if (scancode == 0x1D) {
        ctrl_pressed = true;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = false;
        return;
    }
    if (scancode == 0x38) {
        alt_pressed = true;
        return;
    }
    if (scancode == 0xB8) {
        alt_pressed = false;
        return;
    }
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }
    
    // Ignore Key Release Events (Bit 7 Set)
    if (scancode & 0x80) {
        return;
    }
    
    // Map Scancode to ASCII
    char c = 0;
    if (shift_pressed || caps_lock) {
        c = scancode_to_ascii_shift[scancode];
        // Handle Caps Lock logic for letters (only letters flip if shift is off)
        if (caps_lock && !shift_pressed && c >= 'A' && c <= 'Z') {
            c = scancode_to_ascii[scancode];
        }
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    if (c) {
        // Enqueue to Ring Buffer
        uint32_t next_pos = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
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
    kbd_read_pos = 0;
    kbd_write_pos = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;
    
    // Unmask IRQ1 (Keyboard Line) on PIC
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
}

/**
 * Get Character (Blocking)
 */
char keyboard_getchar(void) {
    // Blocking Wait
    while (kbd_read_pos == kbd_write_pos) {
        hlt(); // Halt CPU to save energy until interrupt fires
    }
    
    char c = kbd_buffer[kbd_read_pos];
    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
    
    ASSERT(kbd_read_pos < KBD_BUFFER_SIZE);
    
    return c;
}

/**
 * Check if Character is Available
 */
bool keyboard_available(void) {
    return kbd_read_pos != kbd_write_pos;
}

/**
 * Clear Keyboard Buffer
 */
void keyboard_clear(void) {
    kbd_read_pos = 0;
    kbd_write_pos = 0;
}