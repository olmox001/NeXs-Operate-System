/*
 * libc.c - Minimal Standard C Library Implementation
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

#include "libc.h"

/**
 * Standard memset with 64-bit optimization
 */
void* memset(void* ptr, int value, size_t num) {
    ASSERT(ptr != NULL);
    
    // Optimization: Fill 8 bytes at a time
    uint64_t* p64 = (uint64_t*)ptr;
    uint8_t v = (uint8_t)value;
    uint64_t v64 = 0;
    
    // Create 64-bit pattern
    for (int i = 0; i < 8; i++) {
        v64 = (v64 << 8) | v;
    }
    
    // Setup unaligned start
    uint8_t* p = (uint8_t*)ptr;
    while (num && ((uint64_t)p & 7)) {
        *p++ = v;
        num--;
    }
    
    // Bulk fill
    p64 = (uint64_t*)p;
    while (num >= 8) {
        *p64++ = v64;
        num -= 8;
    }
    
    // Fill remaining bytes
    p = (uint8_t*)p64;
    while (num--) {
        *p++ = v;
    }
    
    return ptr;
}

/**
 * Standard memcpy with optimization.
 * Warning: Undefined behavior if regions overlap (use memmove).
 */
void* memcpy(void* dest, const void* src, size_t num) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Fallback protection for overlap
    if (d > s && d < s + num) {
        // Overlap detected: Copy backwards
        d += num;
        s += num;
        while (num--) *(--d) = *(--s);
        return dest;
    }

    // Optimization: Copy 8 bytes at a time
    while (num >= 8) {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        num -= 8;
    }
    
    while (num--) {
        *d++ = *s++;
    }
    return dest;
}

/**
 * Standard memmove (handles overlapping regions)
 */
void* memmove(void* dest, const void* src, size_t num) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d < s) {
        // Forward Copy
        // Optimization: if aligned
        if ((uint64_t)d % 8 == (uint64_t)s % 8) {
             while (num >= 8) {
                *(uint64_t*)d = *(const uint64_t*)s;
                d += 8; s += 8; num -= 8;
             }
        }
        while (num--) *d++ = *s++;
    } else {
        // Backward Copy
        d += num;
        s += num;
        while (num--) *(--d) = *(--s);
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    ASSERT(ptr1 != NULL);
    ASSERT(ptr2 != NULL);

    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    
    while (num--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char* str) {
    ASSERT(str != NULL);
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    char* ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char* str1, const char* str2) {
    ASSERT(str1 != NULL);
    ASSERT(str2 != NULL);
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(uint8_t*)str1 - *(uint8_t*)str2;
}

int strncmp(const char* str1, const char* str2, size_t n) {
    ASSERT(str1 != NULL);
    ASSERT(str2 != NULL);
    while (n && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) return 0;
    return *(uint8_t*)str1 - *(uint8_t*)str2;
}

char* strcat(char* dest, const char* src) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    char* ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}

char* strchr(const char* str, int c) {
    ASSERT(str != NULL);
    while (*str) {
        if (*str == (char)c) return (char*)str;
        str++;
    }
    return NULL;
}

/**
 * Integer to String Conversion
 */
void itoa(int value, char* str, int base) {
    ASSERT(str != NULL);
    if (base < 2 || base > 36) { *str = 0; return; }
    
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int tmp_value;
    bool negative = false;

    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }

    do {
        tmp_value = value;
        value /= base;
        int rem = tmp_value - value * base;
        if (rem < 0) rem = -rem; // Min int edge case
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[rem];
    } while (value);
    
    if (negative) *ptr++ = '-';
    *ptr-- = '\0';
    
    // Reverse string
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

/**
 * Unsigned Integer to String Conversion
 */
void uitoa(uint32_t value, char* str, int base) {
    ASSERT(str != NULL);
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    uint32_t tmp_value;
    
    if (base < 2 || base > 36) { *str = 0; return; }
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);
    
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

int atoi(const char* str) {
    ASSERT(str != NULL);
    int res = 0;
    int sign = 1;
    int i = 0;
    
    if (str[0] == '-') { sign = -1; i++; }
    else if (str[0] == '+') { i++; }
    
    for (; str[i] >= '0' && str[i] <= '9'; i++) {
        res = res * 10 + (str[i] - '0');
    }
    return sign * res;
}