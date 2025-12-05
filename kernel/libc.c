// libc.c - Robust & Optimized Implementation
#include "libc.h"

// Optimized memset (64-bit aligned writes)
void* memset(void* ptr, int value, size_t num) {
    ASSERT(ptr != NULL);
    
    // Fill 8 bytes at a time if aligned
    uint64_t* p64 = (uint64_t*)ptr;
    uint8_t v = (uint8_t)value;
    uint64_t v64 = 0;
    
    // Create 64-bit pattern
    for (int i = 0; i < 8; i++) {
        v64 = (v64 << 8) | v;
    }
    
    // Unaligned prefix
    uint8_t* p = (uint8_t*)ptr;
    while (num && ((uint64_t)p & 7)) {
        *p++ = v;
        num--;
    }
    
    // Main loop
    p64 = (uint64_t*)p;
    while (num >= 8) {
        *p64++ = v64;
        num -= 8;
    }
    
    // Suffix
    p = (uint8_t*)p64;
    while (num--) {
        *p++ = v;
    }
    
    return ptr;
}

// Optimized memcpy
void* memcpy(void* dest, const void* src, size_t num) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Check for destructive overlap (src < dest < src+n)
    if (d > s && d < s + num) {
        // Overlap! Must use memmove logic (copy backwards)
        // Or panic if strict memcpy is required. Libc usually says undefined.
        // We will just fall back to reverse copy here for safety.
        d += num;
        s += num;
        while (num--) *(--d) = *(--s);
        return dest;
    }

    // 64-bit copy if possible
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

void* memmove(void* dest, const void* src, size_t num) {
    ASSERT(dest != NULL);
    ASSERT(src != NULL);
    
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d < s) {
        // Forward copy
        if ((uint64_t)d % 8 == (uint64_t)s % 8) {
             while (num >= 8) {
                *(uint64_t*)d = *(const uint64_t*)s;
                d += 8; s += 8; num -= 8;
             }
        }
        while (num--) *d++ = *s++;
    } else {
        // Backward copy
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
        int rem = tmp_value - value * base; // Modulo
        if (rem < 0) rem = -rem; // Handle special case min_int
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[rem];
    } while (value);
    
    if (negative) *ptr++ = '-';
    *ptr-- = '\0';
    
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

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