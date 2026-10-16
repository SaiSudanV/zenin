# Kernel Invariants

These properties must remain true regardless of workload.

An implementation that violates one of these invariants is incorrect even if ordinary tests pass.

## Memory

1. A physical page may not simultaneously be owned by incompatible allocations.

2. PMM accounting must equal the actual state represented by the allocator.

3. A live allocation must retain its backing resources until release.

4. Releasing a resource must not corrupt unrelated live allocations.

## Virtual Memory

5. A live virtual mapping must remain valid for its lifetime.

6. Virtual ranges released by the allocator must become eligible for reuse.

7. Virtual-address exhaustion must represent genuine inability to satisfy the request, not permanent loss caused by allocator bookkeeping.

8. Fragmentation must not cause usable free ranges to become permanently invisible.

9. A free range that is too small for one request remains available for later smaller requests.

10. Search cursor/hint state must never cause an otherwise valid free range to be permanently skipped.

11. Wraparound searches must consider the entire eligible address space.

## Allocator Integrity

12. Allocation metadata must correspond to actual allocated regions.

13. Freeing the same resource more than once must not silently corrupt allocator state.

14. Allocator bookkeeping must not overlap live allocations.

15. Live allocations must retain their contents across unrelated allocation/free operations.

16. Repeated allocation/free cycles must not monotonically consume reusable address space.

## Interfaces

17. Higher-level subsystems must not bypass ownership rules of lower-level resource managers.

18. A subsystem may not assume successful allocation without handling failure.

## Verification

19. A historical successful test does not prove correctness after the implementation changes.

20. Documentation must never claim a stronger property than the tests and evidence establish.

21. A test must fail when the corresponding invariant is violated; tests that cannot detect the targeted failure are insufficient evidence.

## Architectural Integrity

22. Implementation convenience must not silently change the architectural contract.

23. New behavior that changes an existing invariant requires an explicit architectural decision.

24. Unverified behavior must be labeled unverified rather than treated as established.