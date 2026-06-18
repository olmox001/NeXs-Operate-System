## 2026-01-27 - Unvalidated System Call Pointers
**Vulnerability:** System calls (`sys_write`, `sys_read`) accepted arbitrary pointers without validation, allowing users to read/write kernel memory. `sys_write` also relied on null-termination (`vga_puts`), risking buffer over-reads.
**Learning:** The kernel lacked a clear user/kernel memory boundary check in syscall handlers.
**Prevention:** Implement `is_safe_ptr` to enforce `ptr >= _kernel_end`. Use length-bounded string functions.
