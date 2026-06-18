## 2025-05-23 - Buffer Overflow in IPC Message System
**Vulnerability:** The `msg_receive` function in `kernel/Subsystems/messages.c` blindly copied the full message payload into a user-provided buffer without checking if the buffer was large enough. This could allow a malicious sender to send a large message and corrupt the stack of the receiver.
**Learning:** IPC mechanisms that involve data copying must always validate the destination buffer size. In kernel development, functions often assume "caller knows best" (as noted in the code comments), but this assumption is dangerous for security-critical subsystems like IPC.
**Prevention:** Always require a `max_size` argument for any function that writes to a caller-provided buffer, and enforce this limit strictly.
