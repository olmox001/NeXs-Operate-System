## 2025-10-26 - Fixed Buffer Overflow in msg_receive
**Vulnerability:** The IPC function `msg_receive` copied message payloads into a user-provided buffer without checking if the buffer was large enough to hold the data. This could allow an attacker to send a large message and overflow the receiver's stack or heap.
**Learning:** C APIs that populate a buffer must *always* accept a buffer size argument. Relying on "caller must ensure sufficient size" is a recipe for disaster, especially in IPC where message sizes can vary.
**Prevention:** Enforce a coding standard where all output buffer parameters are accompanied by a `size_t max_size` parameter, and validate it before writing.
