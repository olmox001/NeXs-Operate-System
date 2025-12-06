# NeXs-OS Development Roadmap

## Phase 1: Bootloader & Core ✅
- [x] Stage1 MBR bootloader (512 bytes)
- [x] Stage2 protected mode loader
- [x] A20 gate enable
- [x] Long Mode (x86_64) transition
- [x] GDT for 64-bit code/data segments
- [x] Identity paging (first 16MB, 2MB huge pages)
- [x] Flat binary kernel loading

## Phase 2: Interrupts & Exceptions ✅
- [x] IDT setup (256 entries)
- [x] ISR stubs (exceptions 0-31)
- [x] IRQ stubs (hardware 32-47)
- [x] PIC 8259 remapping
- [x] Exception handler with register dump
- [x] Software interrupt (INT 0x80)

## Phase 3: Memory Management ✅
- [x] Buddy allocator (4KB-16MB blocks)
- [x] E820 BIOS memory map detection
- [x] Dynamic heap sizing from E820
- [x] Secure memory region (64KB hidden)
- [x] Slab allocator for messages
- [x] Global memory statistics

## Phase 4: Timing System ✅
- [x] PIT configuration (1000Hz scheduler tick)
- [x] TSC (Time Stamp Counter) support
- [x] TSC frequency calibration via PIT
- [x] Nanosecond precision timing
- [x] High-resolution delays (ns/us/ms)
- [x] Uptime tracking

## Phase 5: Multitasking ✅
- [x] Task structure with context
- [x] Priority-based scheduler (256 levels)
- [x] Preemptive context switching
- [x] Task states (READY, RUNNING, SLEEPING, WAITING, DEAD)
- [x] UID system (Kernel=0, Root=1, User=2)
- [x] Time quantum per priority
- [x] yield() and sleep() functions
- [x] Idle task

## Phase 6: System Calls ✅
- [x] INT 0x80 syscall mechanism
- [x] POSIX-like syscall numbers
- [x] Syscall dispatcher
- [x] SYS_READ, SYS_WRITE
- [x] SYS_GETPID, SYS_EXIT, SYS_YIELD, SYS_SLEEP
- [x] SYS_UPTIME, SYS_MEMINFO, SYS_TASKINFO
- [x] SYS_GETTIME_NS, SYS_GETFREQ (TSC access)
- [x] SYS_MSGSND, SYS_MSGRCV (IPC)

## Phase 7: IPC & Security ✅
- [x] Message queue per task
- [x] Slab-based message allocation
- [x] Zero-copy pointer messages
- [x] Signed memory blocks (CRC32)
- [x] Capability-based permissions
- [x] UID-based access control
- [x] Permission masks per task

## Phase 8: Drivers ✅
- [x] VGA text mode (80x25)
- [x] Optimized VGA scroll (64-bit copies)
- [x] PS/2 keyboard driver
- [x] Serial port (UART 16550A)
- [x] Module system (Linux-like interface)
- [x] Device abstraction layer

## Phase 9: Shell & UX ✅
- [x] Interactive kernel shell
- [x] Command parsing
- [x] Command history
- [x] Auto-clear on startup
- [x] Memory/task status commands
- [x] Reboot/halt commands

---

## Phase 10: User Mode (Planned)
- [ ] TSS (Task State Segment) setup
- [ ] Ring 3 code/data segments in GDT
- [ ] User-space stack switching
- [ ] IRET to user mode
- [ ] Syscall from Ring 3

## Phase 11: Filesystem (Planned)
- [ ] Ramdisk driver
- [ ] FAT32 or custom FS
- [ ] VFS (Virtual File System) layer
- [ ] File descriptors
- [ ] open/read/write/close syscalls

## Phase 12: ELF Loader (Planned)
- [ ] ELF64 parser
- [ ] Program header loading
- [ ] User process execution
- [ ] Argument passing (argc/argv)

## Phase 13: Advanced Timing (Planned)
- [ ] HPET (High Precision Event Timer)
- [ ] APIC timer
- [ ] Per-CPU timers
- [ ] Real-time clock (RTC)

## Phase 14: Networking (Future)
- [ ] NE2000 or E1000 driver
- [ ] Ethernet frames
- [ ] IP/UDP/TCP stack
- [ ] Sockets API

## Phase 15: Graphics (Future)
- [ ] VESA/VBE framebuffer
- [ ] Linear framebuffer mode
- [ ] Basic 2D primitives
- [ ] Bitmap fonts

---

## Version History

| Version | Date | Highlights |
|---------|------|------------|
| 0.0.1 | 2025-12 | Basic boot, IDT, VGA |
| 0.0.12 | 2025-12 | Scheduler, syscalls, shell |
| 0.0.2 | 2025-12 | E820, TSC timer, slab allocator |
