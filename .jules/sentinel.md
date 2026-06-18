## 2026-01-24 - Kernel IPC Buffer Overflow
**Vulnerability:** `msg_receive` in `kernel/Subsystems/messages.c` blindly copied payload based on sender's size into receiver's buffer without size validation.
**Learning:** Internal kernel APIs often assume trusted callers, but when exposed to complex subsystems (like shell or future syscalls), this trust breaks down. Explicitly handling buffer sizes is critical in OS development.
**Prevention:** Always enforce `max_size` arguments for output buffers in C APIs, even for internal functions.
