// vga.h - VGA Text Mode Driver
#ifndef VGA_H
#define VGA_H

#include "kernel.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

// VGA colors
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

// Initialize VGA driver
void vga_init(void);

// Clear screen
void vga_clear(void);

// Set colors
void vga_set_color(uint8_t fg, uint8_t bg);

// Print string at current cursor
void vga_puts(const char* str);

// Print character at current cursor
void vga_putc(char c);

// Print formatted integer
void vga_puti(int value);
void vga_putx(uint32_t value);

// Move cursor
void vga_set_cursor(int x, int y);

// Get cursor position
void vga_get_cursor(int* x, int* y);

// Scroll screen up
void vga_scroll(void);

#endif // VGA_H