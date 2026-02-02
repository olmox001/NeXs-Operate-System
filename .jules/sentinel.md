## 2025-05-18 - [IPC Buffer Overflow]
**Vulnerability:** The `msg_receive` function in `kernel/Subsystems/messages.c` was blindly copying message payloads into a user-provided buffer without verifying the buffer size, leading to potential buffer overflows in kernel space.
**Learning:** Even in a Ring 0 kernel environment without user-space separation, internal APIs must enforce bounds checking to prevent privilege escalation or system crashes from buggy or malicious tasks.
**Prevention:** Always include a `max_size` parameter in any function that writes to a buffer provided by the caller, and validate the size before copying.
