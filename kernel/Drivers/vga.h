/*
 * vga.h - VGA Text Mode Driver Definitions
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

#ifndef VGA_H
#define VGA_H

#include "kernel.h"

// Standard VGA Text Mode Dimensions
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

// =============================================================================
// Standard VGA Color Palette (4-bit CGA)
// =============================================================================
enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

// =============================================================================
// Initialization
// =============================================================================

// Reset VGA Driver, clear screen, and set default colors
void vga_init(void);

// =============================================================================
// Basic Operations
// =============================================================================

// Clear the screen (fill with spaces)
void vga_clear(void);

// Set current drawing color (Foreground, Background)
void vga_set_color(uint8_t fg, uint8_t bg);

// =============================================================================
// Output Functions
// =============================================================================

// Print String
void vga_puts(const char* str);

// Print Character
void vga_putc(char c);

// Print Integer (Decimal)
void vga_puti(int value);

// Print Integer (Hexadecimal)
void vga_putx(uint32_t value);

// =============================================================================
// Cursor and Screen Control
// =============================================================================

// Move hardware cursor to specific coordinate
void vga_set_cursor(int x, int y);

// Get current cursor position
void vga_get_cursor(int* x, int* y);

// Scroll the text buffer up by one line
void vga_scroll(void);

// =============================================================================
// Virtual Console System
// =============================================================================

#define VGA_NUM_CONSOLES 4

// Switch the visible console on the monitor
void vga_set_active_console(int index);

// Get current active console index
int vga_get_active_console(void);

// Write to a specific console (0=Kernel, 1=Shell, etc.)
void vga_write_console(int index, const char* str);

// Clear specific console
void vga_clear_console(int index);

#endif // VGA_H