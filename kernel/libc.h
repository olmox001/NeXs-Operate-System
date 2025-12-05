// libc.h - Minimal C library for kernel
#ifndef LIBC_H
#define LIBC_H

#include "kernel.h"

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* str, int c);
void itoa(int value, char* str, int base);
void uitoa(uint32_t value, char* str, int base);
int atoi(const char* str);

#endif // LIBC_H
