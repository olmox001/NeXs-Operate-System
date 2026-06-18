## 2026-01-25 - Arbitrary Kernel Memory Read/Write via Syscalls
**Vulnerability:** System calls (`sys_read`, `sys_write`, etc.) accepted user-supplied pointers without validation, allowing arbitrary read/write access to the static kernel image (`0x100000` - `_kernel_end`).
**Learning:** In a flat-memory model OS where user/kernel separation is not enforced by paging hardware, explicit range checks in software are critical. The kernel image itself is a sensitive target.
**Prevention:** Implemented `is_safe_ptr` helper to validate that all user pointers fall outside the protected kernel image range. Enforced maximum length on `sys_write` to prevent DoS via interrupt disabling.
