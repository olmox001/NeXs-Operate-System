## 2026-01-28 - Internal IPC Buffer Overflow
**Vulnerability:** `msg_receive` copied data to a caller-provided buffer without a size limit, allowing large messages to overflow small stack buffers.
**Learning:** Internal kernel APIs often lack the defensive checks seen in syscalls because they assume "trusted" callers, but this creates latent vulnerabilities if those APIs are ever exposed or used incorrectly.
**Prevention:** Always include `max_size` parameters in any function that writes to a caller-provided buffer, even for internal APIs.
