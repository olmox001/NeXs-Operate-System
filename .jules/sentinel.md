## 2025-05-24 - Syscall Pointer Validation
**Vulnerability:** System calls (sys_read, sys_write, etc.) accepted user pointers without validation, allowing arbitrary read/write of kernel memory.
**Learning:** In a flat address space, explicit range checks are critical for kernel integrity.
**Prevention:** Implemented is_safe_ptr to enforce ptr >= _kernel_end and !NULL for all syscall pointer arguments.
