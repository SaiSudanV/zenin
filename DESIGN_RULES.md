# Design Rules

## Rule 1 — Correctness Before Optimization

Never optimize search state, caching, allocation strategy, or bookkeeping at the expense of correctness.

An optimization that can hide reusable resources or violate an invariant is unacceptable.

---

## Rule 2 — Preserve Existing Invariants

Every change must preserve all invariants in `INVARIANTS.md`.

If a change intentionally modifies an invariant, update the architectural documentation and decision record first.

---

## Rule 3 — Prefer Root-Cause Fixes

When a test fails:

- identify the violated invariant,
- identify the underlying mechanism causing the violation,
- fix that mechanism,
- add or preserve a regression test.

Do not suppress the failure or weaken the test merely to obtain a PASS.

---

## Rule 4 — Tests Must Be Adversarial

For foundational infrastructure, test:

- normal operation,
- boundaries,
- exhaustion,
- release/reuse,
- fragmentation,
- wraparound,
- repeated cycles,
- and interaction with live resources.

---

## Rule 5 — No Evidence Inflation

Never convert:

```text
implemented
```

into:

```text
verified
```

without executing the relevant verification.

Never convert:

```text
old verification
```

into:

```text
current verification
```

after the implementation has changed.

---

## Rule 6 — No Hallucinated Results

An agent must never invent:

- compiler output,
- test output,
- QEMU output,
- measurements,
- hardware results,
- or successful execution.

If execution is unavailable, state that clearly.

---

## Rule 7 — Preserve Working Architecture

Do not redesign a subsystem merely because another implementation might be theoretically cleaner.

Existing architecture should be changed only when:

- requirements demand it,
- an invariant is violated,
- testing demonstrates a real deficiency,
- or an explicit architectural decision authorizes the change.

---

## Rule 8 — Regression Tests Are Permanent

Whenever an implementation bug is discovered through a test, retain the test after fixing the implementation.

The test becomes part of the project's permanent correctness boundary.

---

## Rule 9 — Resource Reuse Is Explicit

Any finite resource that can be released must have an explicit reclamation/reuse strategy unless the architecture intentionally specifies otherwise.

---

## Rule 10 — Documentation Reflects Reality

Documentation must describe:

- what is specified,
- what is implemented,
- what is verified,
- and what remains uncertain.

These categories must not be conflated.

---

## Rule 11 — Smallest Correct Change

Prefer the smallest change that restores the violated invariant without introducing unnecessary architectural complexity.

---

## Rule 12 — Current Workspace Is Authoritative for Implementation

When documentation conflicts with the actual source tree:

1. inspect the discrepancy,
2. determine which is current,
3. update the documentation,
4. do not blindly trust historical notes.

Never silently assume the source or documentation is correct without checking.

---

## Rule 13 — Phase Gates

Do not begin relying on a later-phase subsystem as foundational until the required earlier-phase invariants have been verified.

---

## Rule 14 — No Silent Scope Expansion

Do not introduce unrelated features while completing a defined phase unless the feature is necessary to satisfy or verify the phase requirement.