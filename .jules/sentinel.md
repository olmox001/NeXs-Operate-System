## 2025-05-24 - Syscall Buffer Validation
**Vulnerability:** `sys_write` ignored `len` argument and used `vga_puts`, which assumes a null-terminated string. This allowed reading past buffer boundaries (out-of-bounds read) and potentially locking the kernel in an interrupt-disabled loop (DoS) if the string was very long or missing a null terminator.
**Learning:** In a Ring 0 kernel without user/kernel isolation, standard library functions like `puts` are dangerous when handling "user" input because they assume well-formedness. Always use length-delimited functions (`write` vs `puts`) at the system call boundary.
**Prevention:** Implement `vga_write` and `serial_write` that take explicit lengths. Cap read/write sizes in syscall handlers to prevent DoS.
