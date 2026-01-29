## 2025-05-18 - Buffer Over-read in sys_write
**Vulnerability:** The `sys_write` system call (INT 0x80, AX=1) ignored the `len` argument and treated the buffer as a null-terminated string. It also lacked pointer validation and length limits. This allowed reading arbitrary memory until a null byte was found, potentially leaking sensitive kernel data or causing a crash (DoS) if mapped memory ended. It also allowed long interrupt disable times if the string was very long.
**Learning:** System calls must strictly adhere to their interface contracts (like POSIX `write`) and must not trust user-provided data structures (like null-termination) implicitly. Ring 0 code must be especially defensive as it bypasses hardware protections.
**Prevention:**
1. Always respect and validate length arguments in system calls.
2. Implement upper bounds on buffer sizes (e.g., 1024 bytes) to prevent resource exhaustion or latency spikes.
3. Validate all pointers passed from "user" (caller) space, even if currently running in kernel mode.
