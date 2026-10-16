\# System Specification



\## 1. System Identity



This project is a novel ARM64 operating-systemkernel architecture.



It is being developed from first principles rather than being defined as a conventional LinuxUnix-compatible kernel.



The system's architecture, abstractions, resource model, and execution model are defined by this repository's specification documents.



\---



\# 2. Specification Hierarchy



The following documents have distinct authority



1\. `SYSTEM\_SPEC.md`

&#x20;  - Defines what the system fundamentally is.



2\. `ARCHITECTURE.md`

&#x20;  - Defines how the system is structurally organized.



3\. `INVARIANTS.md`

&#x20;  - Defines properties that must always remain true.



4\. `DESIGN\_RULES.md`

&#x20;  - Defines constraints on implementation and architectural evolution.



5\. `DECISIONS.md`

&#x20;  - Records important decisions and their rationale.



6\. `CURRENT\_STATE.md`

&#x20;  - Records what is actually implemented and verified at the present time.



7\. `KNOWN\_FAILURES.md`

&#x20;  - Records known failures, uncertainties, and unresolved limitations.



8\. `VERIFICATION.md`

&#x20;  - Records how architectural claims are verified.



`Achieved Features.md` records implemented and successfully demonstrated capabilities but does not override the specification or verification documents.



\---



\# 3. Fundamental System Requirements



The kernel must provide a coherent foundational execution environment for the eventual operating system.



The foundational system must establish



\- ARM64 execution,

\- physical memory management,

\- virtual memory management,

\- kernel heapresource allocation,

\- kernel objectresource ownership,

\- devicebus infrastructure,

\- input infrastructure,

\- and the architectural primitives required by later system layers.



\---



\# 4. Memory Model



The kernel must explicitly distinguish



\- physical memory,

\- virtual address space,

\- kernel-owned memory,

\- live allocations,

\- freereusable regions,

\- and externally controlled resources.



Virtual-address allocation must not be treated as equivalent to physical-memory allocation.



A virtual range becoming free must make that range eligible for future reuse according to allocator policy.



\---



\# 5. Resource Lifecycle



Every kernel-managed resource must have an identifiable lifecycle



```text

creation

&#x20;   ↓

ownership

&#x20;   ↓

use

&#x20;   ↓

release

&#x20;   ↓

reclamationreuse

```



No resource may silently transition into an unreachable state.



\---



\# 6. Verification Requirement



Architectural claims are provisional until verified.



The system must prefer executable verification over assertions in documentation.



Historical test results may be retained as evidence of previous states but must not be represented as verification of a changed implementation.

