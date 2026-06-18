## 2026-02-05 - Syscall Argument Validation Gap
**Vulnerability:** `sys_write` ignored the `len` argument, relying on null-terminated buffers. This allowed potential DoS (infinite print loop) or information disclosure (reading past buffer end).
**Learning:** The syscall handler was implemented with `vga_puts` which expects C-strings, ignoring the binary buffer nature of `write` syscalls.
**Prevention:** Always enforce explicit length limits on buffer-based syscalls and use length-aware output functions (`vga_write` instead of `vga_puts`).
