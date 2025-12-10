# 📚 DOCUMENTAZIONE IMPLEMENTAZIONE VMM MULTI-CORE per NeXs OS

## 🎯 OBIETTIVI ARCHITETTURALI

### Principi di Design

1. **Separazione delle Responsabilità**: Buddy Allocator gestisce frame fisici; VMM gestisce spazi virtuali
1. **Scalabilità Multi-Core**: Lock-free per operazioni comuni, sincronizzazione minimale
1. **Uniformità dell’Accesso**: Tutti i core vedono lo stesso spazio virtuale tramite Page Tables condivise
1. **Modularità**: Ogni componente (VMM, VFS, SMP) è indipendente e sostituibile

-----

## 📐 ARCHITETTURA DEL SISTEMA DI PAGING

### Livelli di Astrazione

```
┌─────────────────────────────────────────────────────────┐
│ LIVELLO 5: USER API (malloc, mmap, brk) │
├─────────────────────────────────────────────────────────┤
│ LIVELLO 4: VFS (Virtual File System) │
│ - Mappatura file → pagine virtuali │
│ - Cache unificata (page cache) │
├─────────────────────────────────────────────────────────┤
│ LIVELLO 3: VMM (Virtual Memory Manager) │
│ - Gestione Address Spaces (vm_space_t) │
│ - Page Table Walking/Mapping │
│ - TLB Shootdown per SMP │
│ - Copy-on-Write / Demand Paging │
├─────────────────────────────────────────────────────────┤
│ LIVELLO 2: PMM (Physical Memory Manager) │
│ - Buddy Allocator (Per-Core, Lock-Free) │
│ - Zone Management (DMA, Normal, High) │
│ - Page Frame Database (struct page) │
├─────────────────────────────────────────────────────────┤
│ LIVELLO 1: HARDWARE (MMU, TLB, Page Tables) │
│ - x86_64 4-Level Paging (PML4→PDPT→PD→PT) │
│ - CR3 register (Physical address of PML4) │
│ - Page Fault Exception (#PF) │
└─────────────────────────────────────────────────────────┘
```

-----

## 🔄 FLUSSO DI ALLOCAZIONE MEMORIA (Request → Physical Frame)

### Scenario: Processo User richiede memoria

```
1. USER SPACE: malloc(4096)
↓
2. SYSCALL: sys_brk() o sys_mmap()
↓
3. VMM: vmm_alloc_pages(vm_space, virt_addr, count, flags)
├─→ Controlla se indirizzo è valido nel VMA
├─→ Per ogni pagina:
│ ├─→ Richiede frame fisico: phys = pmm_alloc_page()
│ └─→ Mappa virtuale→fisico: vmm_map_page(virt, phys, flags)
└─→ Aggiorna statistiche (resident_pages++)
↓
4. PMM (Buddy): pmm_alloc_page()
├─→ Identifica CPU corrente: cpu_id = get_cpu_id()
├─→ Accede al Buddy locale: buddy = &per_cpu_buddy[cpu_id]
├─→ Alloca frame da lista free (NO LOCK se disponibile)
└─→ Restituisce indirizzo fisico (uint64_t phys)
↓
5. VMM: vmm_map_page(vm_space, virt, phys, flags)
├─→ Walk Page Table: PML4 → PDPT → PD → PT
├─→ Alloca tabelle intermedie se necessarie (ricorsivo)
├─→ Scrive PTE: *pt_entry = phys | flags | PTE_PRESENT
└─→ Flush TLB: invlpg(virt) o TLB shootdown se SMP
↓
6. HARDWARE (MMU): Virtual → Physical translation attiva
└─→ Processo user può accedere alla memoria
```

-----

## 🧩 COMPONENTI PRINCIPALI

### 1. **Virtual Memory Manager (VMM)**

#### Responsabilità

- **Address Space Management**: Creazione/Distruzione/Clonazione di `vm_space_t`
- **Page Table Operations**: Map/Unmap/Walk delle Page Tables
- **VMA Management**: Tracciamento regioni di memoria (heap, stack, mmap)
- **Page Fault Handling**: Demand paging, COW, swap-in
- **TLB Synchronization**: Invalidazione coerente tra CPU

#### Strutture Dati Chiave

**vm_space_t** (Per-Process Address Space)

- `pml4`: Puntatore fisico alla Page Table root (PML4)
- `vma_list`: Lista VMAs (regioni di memoria virtuale)
- `lock`: Spinlock per proteggere modifiche concurrent
- `statistics`: Contatori (total_pages, resident_pages, swapped_pages)

**vm_area_t** (Virtual Memory Area)

- `start/end`: Range di indirizzi virtuali [start, end)
- `flags`: Permessi (READ, WRITE, EXEC, SHARED)
- `type`: ANONYMOUS (heap), FILE (mmap), DEVICE (MMIO)
- `file`: Metadata per mapping file-backed

**Page Table Entry (pte_t = uint64_t)**

- Bits 0-11: Flags (PRESENT, WRITABLE, USER, NX, COW, SWAPPED)
- Bits 12-51: Physical Frame Number (40 bit → 256TB addressable)
- Bit 63: NX (No Execute)

#### Algoritmi Critici

**Page Table Walk** (4-level traversal)

```
Input: vm_space, virtual_address
Output: physical_address o NULL

1. Estrai indici da virtual_address:
- PML4_idx = (virt >> 39) & 0x1FF
- PDPT_idx = (virt >> 30) & 0x1FF
- PD_idx = (virt >> 21) & 0x1FF
- PT_idx = (virt >> 12) & 0x1FF

2. Walk:
pml4_entry = vm_space->pml4[PML4_idx]
if !(pml4_entry & PRESENT): return NULL

pdpt = phys_to_virt(pml4_entry & ADDR_MASK)
pdpt_entry = pdpt[PDPT_idx]
if !(pdpt_entry & PRESENT): return NULL
if (pdpt_entry & HUGE): return 1GB page

pd = phys_to_virt(pdpt_entry & ADDR_MASK)
pd_entry = pd[PD_idx]
if !(pd_entry & PRESENT): return NULL
if (pd_entry & HUGE): return 2MB page

pt = phys_to_virt(pd_entry & ADDR_MASK)
pt_entry = pt[PT_idx]
if !(pt_entry & PRESENT): return NULL

return (pt_entry & ADDR_MASK) | (virt & 0xFFF)
```

**TLB Shootdown** (Multi-Core Invalidation)

```
Quando CPU_A modifica Page Table condivisa:

1. Preparazione:
- Crea tlb_shootdown_t con addr, count, cpu_mask
- cpu_mask = bitmap dei core da notificare (escluso CPU_A)
- ack_count = 0

2. Invio IPI (Inter-Processor Interrupt):
- Per ogni bit settato in cpu_mask:
- Invia IPI al core target
- IPI vettore: VECTOR_TLB_SHOOTDOWN

3. Handler IPI su CPU target:
- Legge tlb_shootdown_t
- Esegue invlpg(addr) per ogni pagina
- Incrementa atomicamente ack_count

4. Wait su CPU_A:
- Spin-wait finché ack_count == numero_cpu_in_mask
- Barrier di memoria
- Completa operazione
```

-----

### 2. **Physical Memory Manager (PMM)**

#### Responsabilità

- **Frame Allocation**: Fornire frame fisici 4KB al VMM
- **Zone Management**: Gestire DMA, Normal, High memory
- **Per-Core Buddy**: Allocator locale per scalabilità
- **Page Frame Metadata**: Tracking refcount, flags, reverse mapping

#### Buddy Allocator - Architettura Distribuita

**Attuale (Centralizzato)**

```
┌────────────────────────────┐
│ Global Buddy Allocator │
│ - Single Lock │ ← Contention alta su multi-core
│ - Free lists 4KB→16MB │
└────────────────────────────┘
```

**Target (Distribuito)**

```
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Buddy_0 │ │ Buddy_1 │ │ Buddy_2 │ │ Buddy_3 │
│ (Core 0) │ │ (Core 1) │ │ (Core 2) │ │ (Core 3) │
│ No Lock │ │ No Lock │ │ No Lock │ │ No Lock │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
↓ ↓ ↓ ↓
┌────────────────────────────────────────────────┐
│ Global Balance Manager (Slow Path) │
│ - Triggered when local buddy exhausted │
│ - Redistribution algorithm (steal from rich) │
└────────────────────────────────────────────────┘
```

**Algoritmo di Allocazione**

```
pmm_alloc_page():
1. cpu_id = get_cpu_id()
2. buddy = &per_cpu_buddy[cpu_id]

3. Fast Path (No Lock):
- Se buddy->free_list[ORDER_0] non vuoto:
- Pop frame dalla lista
- return frame

4. Slow Path (Richiede Coordinazione):
- Se buddy locale esaurito:
a) Tenta steal da buddy vicini (cache-local)
b) Se fallisce, richiedi al Global Manager
c) Global Manager bilancia: redistribuisce blocchi

5. OOM (Out of Memory):
- Trigger page reclaim (swap-out, cache eviction)
- Se fallisce, ritorna NULL (kill process)
```

#### Metadata: struct page

Ogni frame fisico ha una struct page associata:

```
struct page {
atomic_t refcount; // Reference count (0 = free)
uint32_t flags; // PG_LOCKED, PG_DIRTY, PG_SLAB, etc.
void *virtual; // Virtual address (se mappato)

union {
struct { // Se PAGE CACHE
void *mapping; // Puntatore a inode
uint64_t index; // Offset nel file
};
struct { // Se SLAB
void *slab; // Puntatore al slab
void *freelist; // Freelist interna
};
};

struct list_head lru; // LRU list per page reclaim
};
```

**Array Globale**: `struct page mem_map[]` indicizzato da PFN (Page Frame Number)

-----

### 3. **VFS (Virtual File System)**

#### Responsabilità

- **Astrazione Uniforme**: Interfaccia comune per filesystem diversi (ext2, tmpfs, procfs)
- **Page Cache**: Cache unificata per file in memoria
- **File Mapping**: Supporto per mmap() di file
- **Path Resolution**: Traduzione percorsi → inodes

#### Integrazione con VMM

**mmap() di un file**

```
Scenario: processo mappa file.txt (1MB) in memoria

1. VFS: sys_mmap(file_fd, offset, length, PROT_READ)
↓
2. VFS ottiene inode del file
↓
3. VMM: vmm_mmap(vm_space, virt_addr, length, VMA_FILE)
- Crea VMA con type=FILE, file=inode, offset=0
- NON alloca frame fisici subito (lazy)
↓
4. Processo accede alla memoria → Page Fault
↓
5. Page Fault Handler:
- Identifica VMA associata
- Se VMA_FILE:
a) Calcola offset nel file: file_offset = vma->offset + (fault_addr - vma->start)
b) Controlla Page Cache: page = find_page_cache(inode, file_offset)
c) Se miss:
- Alloca frame: phys = pmm_alloc_page()
- Legge da disco: vfs_read(inode, phys, file_offset, PAGE_SIZE)
- Inserisci in cache: add_page_cache(inode, file_offset, phys)
d) Mappa pagina: vmm_map_page(vm_space, fault_addr, phys, flags)
```

**Page Cache** (Shared tra processi)

```
Struttura:
- Hash Table: (inode, offset) → struct page
- Tutti i processi che mappano lo stesso file condividono le stesse pagine fisiche
- Scritture: se PROT_WRITE, marca PG_DIRTY e flush lazy su disco
```

-----

### 4. **SMP (Symmetric Multi-Processing)**

#### Bootstrap Sequence

```
1. BSP (Bootstrap Processor) Init:
- vmm_init() // Setup kernel address space
- pmm_init() // Initialize buddy allocators
- per_cpu_buddy[0] inizializzato

2. AP (Application Processor) Wakeup:
- Per ogni AP (CPU 1, 2, 3...):
a) Invia INIT-SIPI-SIPI IPI
b) AP salta a trampoline code (Real Mode)
c) Transizione a Long Mode
d) Salta a ap_entry()

3. AP Init (ap_entry):
- Carica kernel CR3 (condiviso con BSP)
- vmm_init_ap() // Setup per-cpu VMM state
- pmm_init_ap() // Inizializza buddy locale
- scheduler_init_ap() // Registra CPU nello scheduler
- Abilita interrupts
- Entra in idle loop
```

#### Per-CPU Data Structures

**Approccio: Thread-Local Storage (TLS)**

```
- GCC supporta __thread per variabili per-CPU
- Ogni CPU ha il proprio GS base register
- Accesso: %gs:offset

Esempio:
__thread vm_space_t *current_vm_space;
__thread task_t *current_task;
__thread buddy_allocator_t local_buddy;
```

#### Sincronizzazione

**Spinlock** (Busy-Wait)

```
Uso: Proteggere sezioni critiche brevi (<10 istruzioni)

typedef struct {
volatile uint32_t lock; // 0=unlocked, 1=locked
uint32_t cpu_id; // Owner CPU (debug)
} spinlock_t;

Implementazione:
- spin_lock(): LOCK XCHG (atomic test-and-set)
- spin_unlock(): MOV 0 (rilascio)
- Disabilita interrupts durante lock (evita deadlock)
```

**RW Lock** (Reader-Writer)

```
Uso: Molte letture, poche scritture (es. Page Tables)

Semantica:
- Molti lettori contemporaneamente
- Un solo scrittore (esclusione totale)
```

**RCU (Read-Copy-Update)** - Avanzato

```
Uso: Strutture dati read-mostly (es. VMA list)

Idea:
- Letture senza lock (atomic load)
- Scritture: copia, modifica, sostituisci puntatore (atomic)
- Defer free fino a quiescence point (tutti i lettori sono usciti)
```

-----

## 🛠️ IMPLEMENTAZIONE: ROADMAP DETTAGLIATA

### FASE 1: Foundation VMM (2-3 settimane)

**Milestone 1.1: Strutture Dati Core**

- Definire `vmm.h` con tipi: `vm_space_t`, `vm_area_t`, `pte_t`
- Implementare utility: `vmm_pml4_index()`, `vmm_pdpt_index()`, etc.
- Test: Compilazione senza warning

**Milestone 1.2: Page Table Operations**

- `vmm_map_page()`: Map single virtual → physical
- `vmm_unmap_page()`: Unmap e free frame
- `vmm_get_physical()`: Page table walk
- Test: Map 100 pagine random, verifica traduzione corretta

**Milestone 1.3: Address Space Management**

- `vmm_create_space()`: Alloca PML4 vuoto
- `vmm_destroy_space()`: Free ricorsivo di tutte le page tables
- `vmm_switch_space()`: Load CR3
- Test: Crea/Distruggi 1000 address spaces, verifica no leak

**Milestone 1.4: TLB Management**

- `vmm_flush_tlb_single()`: INVLPG
- `vmm_flush_tlb_all()`: Reload CR3
- Test: Modifica mapping, verifica flush necessario

-----

### FASE 2: PMM Refactoring (2 settimane)

**Milestone 2.1: Per-CPU Buddy**

- Refactor `buddy.c`: Creare `buddy_allocator_t` type
- Allocare array: `buddy_allocator_t per_cpu_buddy[MAX_CPUS]`
- Partizionare memoria E820 tra buddy (round-robin)
- Test: Alloca da buddy[0], verifica buddy[1] non affetto

**Milestone 2.2: Global Balancer**

- Implementare `pmm_balance()`: Redistribuzione blocchi
- Trigger: Quando buddy locale ha <10% free
- Algoritmo: Steal metà dei blocchi da buddy più ricco
- Test: Esaurisci buddy[0], verifica steal automatico

**Milestone 2.3: struct page Database**

- Definire `struct page`
- Allocare `mem_map[]`: array di struct page per ogni frame fisico
- Implementare `pfn_to_page()`, `page_to_pfn()`
- Test: Verifica corrispondenza PFN ↔ struct page

-----

### FASE 3: VMA Management (1-2 settimane)

**Milestone 3.1: VMA Operations**

- `vmm_insert_vma()`: Inserimento ordinato nella lista
- `vmm_find_vma()`: Binary search per indirizzo
- `vmm_remove_vma()`: Rimozione e free
- Test: Inserisci 50 VMAs random, verifica find corretto

**Milestone 3.2: mmap() Syscall**

- `sys_mmap()`: Crea VMA, restituisce indirizzo virtuale
- `sys_munmap()`: Rimuovi VMA, unmap pagine
- Test: mmap anonymous 1MB, scrivi, verifica lettura corretta

**Milestone 3.3: Page Fault Handler (Basic)**

- Registra handler per exception #PF (vettore 14)
- Handler legge CR2 (fault address), error code
- Se fault in VMA valida: demand paging
- Test: mmap senza MAP_POPULATE, accesso lazy trigger page fault

-----

### FASE 4: Copy-on-Write (1 settimana)

**Milestone 4.1: COW su fork()**

- `vmm_clone_space()`: Copia Page Tables, marca tutte PTE come COW
- Incrementa refcount in struct page
- Test: fork(), modifica memoria parent, verifica child non affetto

**Milestone 4.2: COW Fault Handler**

- `vmm_handle_cow()`: Su write fault di pagina COW:
- Se refcount==1: flip WRITABLE flag
- Se refcount>1: alloca nuovo frame, copia dati, decrementa refcount
- Test: fork(), scrivi in child, verifica copy avviene

-----

### FASE 5: VFS Integration (2-3 settimane)

**Milestone 5.1: Page Cache**

- Hash table: `(inode*, offset) → struct page`
- `find_page_cache()`, `add_page_cache()`, `remove_page_cache()`
- Test: Cache hit/miss rate

**Milestone 5.2: File mmap()**

- `sys_mmap()` con fd valido: Crea VMA_FILE
- Page fault handler: Leggi da VFS, popola cache
- Test: mmap file 10MB, verifica lettura lazy

**Milestone 5.3: tmpfs (RAM-based)**

- Filesystem in-memory per `/tmp`
- Operazioni: create, write, read, unlink
- Test: Creare file in `/tmp`, mmap, modificare

-----

### FASE 6: SMP Support (3-4 settimane)

**Milestone 6.1: AP Bootstrap**

- Trampoline code (16-bit Real Mode → 64-bit Long Mode)
- `ap_entry()`: Init per-CPU structures
- Test: Boot 4 CPU, verifica tutti eseguono idle loop

**Milestone 6.2: TLB Shootdown**

- IPI infrastructure: `send_ipi(cpu_id, vector)`
- Handler `VECTOR_TLB_SHOOTDOWN`: INVLPG e ACK
- `vmm_tlb_shootdown()`: Broadcast + wait
- Test: CPU0 modifica Page Table, verifica CPU1-3 vedono update

**Milestone 6.3: Per-CPU Scheduler**

- Run queue per CPU
- Load balancing tra CPU
- Test: 16 task, verifica distribuzione uniforme

-----

### FASE 7: Advanced Features (2-3 settimane)

**Milestone 7.1: Swap Support**

- Swap partition o file
- `swap_out()`: Scrive pagina su disco, marca PTE_SWAPPED
- `swap_in()`: Legge da disco, alloca frame, update PTE
- Test: Alloca 150% RAM, verifica swap attivo

**Milestone 7.2: Huge Pages (2MB, 1GB)**

- Supporto PSE (Page Size Extension)
- `vmm_map_huge()`: Map con PTE_HUGE
- Test: mmap con MAP_HUGETLB, verifica TLB miss ridotti

**Milestone 7.3: NUMA Awareness**

- Detect NUMA topology (ACPI SRAT)
- Preferenza allocazione locale al CPU
- Test: Misura latenza memory access locale vs remoto

-----

## 📊 METRICHE DI SUCCESSO

### Performance Targets

- **Page Fault Latency**: <5 μs (demand paging)
- **TLB Shootdown**: <10 μs per 4 CPU
- **Allocation Throughput**: >1M pages/sec per CPU
- **Context Switch**: <2 μs (include CR3 reload)

### Scalabilità

- Supporto fino a 64 CPU
- Memory overhead: <2% per metadata (struct page)
- Lock contention: <1% del tempo totale

-----

## 🧪 STRATEGIA DI TESTING

### Unit Tests

- Test per ogni funzione VMM/PMM
- Mock hardware (emulato Page Tables in RAM)
- Verifica leak detection (valgrind-like)

### Integration Tests

- Scenario completi: fork() → exec() → mmap() → exit()
- Stress test: 1000 processi concorrenti
- Fuzz testing: Indirizzi random, syscall malformate

### Performance Tests

- Benchmark: lmbench, stream
- Profiling: perf, flamegraphs
- Regression: CI con reference baseline

-----

## 📁 STRUTTURA FILE CONSIGLIATA

```
kernel/
├── vmm/
│ ├── vmm.h # Header pubblico
│ ├── vmm.c # Core VMM
│ ├── page_tables.c # Page table operations
│ ├── vma.c # VMA management
│ ├── cow.c # Copy-on-Write
│ ├── mmap.c # mmap/munmap syscalls
│ └── page_fault.c # #PF handler
├── pmm/
│ ├── pmm.h
│ ├── buddy.c # Per-CPU buddy
│ ├── page.c # struct page operations
│ └── zones.c # DMA/Normal/High zones
├── vfs/
│ ├── vfs.h
│ ├── vfs.c # Core VFS
│ ├── page_cache.c # Page cache
│ ├── tmpfs.c # RAM filesystem
│ └── ext2.c # (Future) Disk filesystem
├── smp/
│ ├── smp.h
│ ├── ap_boot.S # AP trampoline (16-bit → 64-bit)
│ ├── ipi.c # Inter-Processor Interrupts
│ └── percpu.c # Per-CPU data management
└── tests/
├── vmm_test.c
├── pmm_test.c
└── integration_test.c
```

-----

## 🎓 RISORSE DI APPROFONDIMENTO

### x86_64 Paging

- Intel SDM Volume 3, Chapter 4: “Paging”
- AMD64 Architecture Programmer’s Manual Volume 2, Chapter 5

### Algoritmi

- **Buddy Allocator**: “The Art of Computer Programming” - Knuth, Vol 1
- **TLB Shootdown**: Linux kernel `mm/` source code
- **COW**: Paper “Copy-on-Write Based File Systems” (BSD)

### Best Practices

- OSDev Wiki: <https://wiki.osdev.org/Paging>
- Linux Kernel Documentation: `Documentation/vm/`
- FreeBSD Design and Implementation Book (McKusick)

-----

**Questa documentazione fornisce la roadmap completa per implementare un VMM production-ready, scalabile e moderno. Ogni fase è atomica e testabile indipendentemente.**
