# Architecture

## 1. Architectural Principle

The system is designed as a coherent set of explicit kernel subsystems rather than as an accumulation of unrelated features.

Each subsystem must have:

- a defined responsibility,
- explicit ownership,
- explicit interfaces,
- documented invariants,
- and testable behavior.

---

# 2. Current Foundational Layers

At the current stage, the architecture includes the following conceptual layers:

```text
ARM64 hardware / QEMU
        │
        ▼
CPU / exception / interrupt foundation
        │
        ▼
Physical Memory Management
        │
        ▼
Virtual Memory / MMU
        │
        ▼
Kernel Virtual Address Management
        │
        ▼
Kernel Heap / Allocation
        │
        ├── Handle Heap / Object Resources
        │
        ├── Z-Bus
        │
        ├── Input
        │
        └── Other kernel facilities
```

This diagram represents architectural relationships, not necessarily exact source-file boundaries.

---

# 3. Physical Memory Management

PMM owns the management and accounting of physical pages.

Responsibilities include:

- identifying usable physical memory,
- allocating physical pages,
- releasing physical pages,
- maintaining accounting,
- and providing physical pages to higher-level memory systems.

PMM must not silently assume that virtual-address availability implies physical-memory availability.

---

# 4. Virtual Memory

The MMU/virtual-memory subsystem maps virtual addresses to physical memory according to explicit mapping and permission rules.

The virtual-memory layer must preserve mapping correctness independently of the physical allocator's internal implementation.

---

# 5. Kernel Heap

The kernel heap provides dynamic kernel resource allocation.

The allocator must distinguish between:

- allocation size,
- physical backing,
- virtual-address reservation,
- free virtual ranges,
- and live allocation ownership.

Large-object virtual-address ranges must be reusable after release.

The allocator may use next-fit/search hints, but search-state optimization must never compromise correctness.

---

# 6. Fragmented Virtual Address Space

The allocator treats the virtual address window as a finite reusable resource.

Free ranges may be fragmented.

Correctness requires that allocator search state continues to expose usable free ranges despite fragmentation.

A free hole that is too small for one allocation remains a valid resource for a later smaller allocation.

Search wraparound must not cause such ranges to become permanently inaccessible.

---

# 7. Kernel Subsystems

Other foundational subsystems such as the handle heap, Z-Bus, input, and NEON facilities must consume the lower-level memory/resource services through defined interfaces rather than bypassing ownership rules.

---

# 8. Architectural Evolution

New subsystems must not introduce hidden dependencies that violate existing invariants.

When a design change affects an architectural assumption, update:

- `SYSTEM_SPEC.md`
- `ARCHITECTURE.md`
- `INVARIANTS.md`
- `DECISIONS.md`

as appropriate before treating the new architecture as established.