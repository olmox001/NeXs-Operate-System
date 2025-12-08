/*
 * keyboard.h - PS/2 Keyboard Driver Definitions
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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel.h"

// Ring Buffer Size for Keyboard Input
// 256 bytes is sufficient for most typing bursts
#define KBD_BUFFER_SIZE 256

// =============================================================================
// Driver Interface
// =============================================================================

/**
 * Initialize Keyboard Subsystem.
 * Resets state, clears buffer, and unmasks IRQ1.
 */
void keyboard_init(void);

/**
 * Get a character from the keyboard buffer.
 * Blocking Call: Waits until a key is pressed if buffer is empty.
 * 
 * @return ASCII character of the key pressed.
 */
char keyboard_getchar(void);

/**
 * Check if characters are available in the buffer.
 * Non-blocking check.
 * 
 * @return true if buffer is not empty, false otherwise.
 */
bool keyboard_available(void);

/**
 * Clear the keyboard buffer.
 * Useful when switching contexts or clearing stale input.
 */
void keyboard_clear(void);

#endif // KEYBOARD_H