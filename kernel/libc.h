/*
 * libc.h - Minimal Standard C Library
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * This subset of the C standard library provides essential memory and string
 * manipulation functions required by the kernel and drivers.
 * Optimized for x86_64 where possible (e.g., 64-bit word copies).
 */

#ifndef LIBC_H
#define LIBC_H

#include "kernel.h"

// =============================================================================
// Memory Manipulation
// =============================================================================

/**
 * Fill memory with a constant byte.
 * 
 * @param ptr Pointer to memory block
 * @param value Byte value to set
 * @param num Number of bytes to set
 * @return Original pointer 'ptr'
 */
void* memset(void* ptr, int value, size_t num);

/**
 * Copy memory block (non-overlapping).
 * 
 * @param dest Destination pointer
 * @param src Source pointer
 * @param num Number of bytes to copy
 * @return Destination pointer
 */
void* memcpy(void* dest, const void* src, size_t num);

/**
 * Copy memory block (safe for overlapping).
 * 
 * @param dest Destination pointer
 * @param src Source pointer
 * @param num Number of bytes to copy
 * @return Destination pointer
 */
void* memmove(void* dest, const void* src, size_t num);

/**
 * Compare two memory blocks.
 * 
 * @param ptr1 First block
 * @param ptr2 Second block
 * @param num Number of bytes to compare
 * @return 0 if equal, <0 if ptr1 < ptr2, >0 if ptr1 > ptr2
 */
int memcmp(const void* ptr1, const void* ptr2, size_t num);

// =============================================================================
// String Manipulation
// =============================================================================

/**
 * Get string length.
 */
size_t strlen(const char* str);

/**
 * Copy string (including null terminator).
 */
char* strcpy(char* dest, const char* src);

/**
 * Copy at most n characters of string.
 * Pads with null bytes if src is shorter than n.
 */
char* strncpy(char* dest, const char* src, size_t n);

/**
 * Compare two strings.
 */
int strcmp(const char* str1, const char* str2);

/**
 * Compare at most n characters of two strings.
 */
int strncmp(const char* str1, const char* str2, size_t n);

/**
 * Concatenate src to end of dest.
 */
char* strcat(char* dest, const char* src);

/**
 * Locate first occurrence of character in string.
 */
char* strchr(const char* str, int c);

// =============================================================================
// Integer Conversion
// =============================================================================

/**
 * Integer to ASCII.
 * 
 * @param value Integer to convert
 * @param str Buffer to store result (ensure adequate size!)
 * @param base Base (2-36)
 */
void itoa(int value, char* str, int base);

/**
 * Unsigned Integer to ASCII.
 */
void uitoa(uint32_t value, char* str, int base);

/**
 * ASCII to Integer.
 */
int atoi(const char* str);

#endif // LIBC_H
