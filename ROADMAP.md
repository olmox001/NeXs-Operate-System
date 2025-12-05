### NeXs-OS Development Roadmap

#### Phase 1 – Core Kernel Foundations (already completed)
- Custom 2-stage x86-64 bootloader (Real → Protected → Long Mode)
- Pure 64-bit kernel entry at 0x100000
- Dual VGA + serial console output
- Full IDT with exception & IRQ handling
- PS/2 keyboard driver (IRQ1)
- PIT timer (IRQ0) functional
- Buddy allocator with overflow protection & checksums
- Basic permission system
- Interactive shell with built-in commands

#### Phase 2 – Multitasking & Process Management (current priority)
- Task Control Block (TCB) and process structure
- Separate kernel stack per task
- TSS and hardware context switching support
- Context switch on PIT tick (preemptive)
- Round-robin scheduler
- Voluntary yield() and sleep(ms) primitives
- fork(), exit(), wait() system calls
- Process isolation (separate address spaces – later)

#### Phase 3 – System Call Interface
- Modern syscall/sysret interface (IA32_LSTAR, IA32_STAR, IA32_FMASK)
- Syscall dispatcher table
- Basic syscalls: write, read, sleep, exit, getpid, yield, execve

#### Phase 4 – Storage & Block Layer
- ATA/ATAPI PIO driver (primary master)
- Generic block device abstraction
- Sector read/write cache (simple)
- Support for multiple partitions (MBR)

#### Phase 5 – File System
- FAT12/16/32 read-only support (priority: FAT32)
- Virtual File System (VFS) layer
- File operations: open, read, close, readdir
- execve() loading ELF64 executables from disk
- (Later) FAT32 read-write
- (Future) Custom simple ext2-like FS

#### Phase 6 – Advanced Features
- User mode & ring 3 execution
- Proper memory isolation per process (paging per task)
- Signals
- Basic ELF loader with dynamic linking stubs
- Simple malloc/free in user space
- Multi-core (SMP) bring-up
- USB stack (UHCI/OHCI/EHCI)
- Networking (RTL8139 or e1000)

#### Phase 7 – Polish & Real Hardware
- USB boot support
- APIC & IOAPIC
- ACPI parsing (basic power off / reboot)
- Real hardware testing (multiple machines)
- GRUB fallback bootloader (optional)

Current focus: Phase 2 – Multitasking (active development)
