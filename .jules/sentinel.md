## 2025-05-22 - Buffer Over-read in System Calls
**Vulnerability:** `sys_write` relied on `vga_puts` which expects a null-terminated string, ignoring the provided length. This allows reading past the buffer end. `sys_read` also ignored the length parameter.
**Learning:** Kernels operating in a single address space (or Ring 0) are particularly vulnerable to info leaks if they blindly trust pointers or string formats.
**Prevention:** Always implement `write_len` style functions in drivers that accept an explicit size. Cap all user-provided sizes in the system call dispatcher.
