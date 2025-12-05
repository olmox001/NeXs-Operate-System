// keyboard.h - PS/2 Keyboard Driver
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel.h"

#define KBD_BUFFER_SIZE 256

// Initialize keyboard driver
void keyboard_init(void);

// Get character from keyboard buffer (blocking)
char keyboard_getchar(void);

// Check if character is available
bool keyboard_available(void);

// Clear keyboard buffer
void keyboard_clear(void);

#endif // KEYBOARD_H