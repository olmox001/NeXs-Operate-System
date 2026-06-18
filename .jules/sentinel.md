# Sentinel's Journal

## 2025-10-27 - [CRITICAL] Buffer Overflow in IPC msg_receive
**Vulnerability:** The `msg_receive` function copied IPC message payloads into a caller-provided buffer without checking if the buffer was large enough. This could lead to kernel heap/stack corruption if a message was larger than the receiver expected.
**Learning:** APIs that write to caller-provided buffers MUST accept a `max_size` argument. Even "internal" or currently unused functions in a kernel are attack surfaces if exposed later (e.g. via modules or syscalls).
**Prevention:** Refactor all data copy functions (`memcpy` wrappers, `strcpy`, etc.) to require destination size arguments. Return explicit error codes (e.g., `MSG_ERR_BUFFER_TOO_SMALL`) for buffer checks.
