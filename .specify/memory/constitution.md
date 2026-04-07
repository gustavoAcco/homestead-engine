<!--
## Sync Impact Report

**Version change**: (unversioned template) → 1.0.0
**Bump rationale**: Initial adoption — all eight principles authored from scratch.
Equivalent to a MAJOR bump from an unversioned baseline.

### Principles added (all new)
- I. Modern C++23 Idioms
- II. Separation of Concerns
- III. Testing (NON-NEGOTIABLE)
- IV. Documentation
- V. Code Style
- VI. Build System
- VII. Continuous Integration
- VIII. Performance

### Sections added
- Core Principles (8 principles)
- Build System & CI Reference
- Development Workflow
- Governance

### Templates reviewed
- `.specify/templates/plan-template.md` — Constitution Check gate is a per-feature
  placeholder; no structural change required ✅
- `.specify/templates/spec-template.md` — No constitution-specific references;
  generic structure unchanged ✅
- `.specify/templates/tasks-template.md` — Language-agnostic template; no update
  required ✅
- `.specify/templates/agent-file-template.md` — Generic guidance file; no update
  required ✅

### Deferred TODOs
- None — all placeholders resolved.
-->

# homestead-engine Constitution

## Core Principles

### I. Modern C++23 Idioms

All production code MUST target C++23. Value semantics are preferred over pointer or
reference semantics wherever ownership is unambiguous. `std::expected<T, E>` MUST be
used for fallible operations on hot paths; exceptions are forbidden in the solver and
graph traversal code. `std::optional<T>` is used for absent-but-valid values.

Templates MUST be constrained with concepts — unconstrained `template<typename T>` is
not acceptable on public API surfaces. Range adaptors and views (`std::ranges`,
`std::views`) SHOULD replace hand-written loops where they improve clarity without
sacrificing performance. Raw `new`/`delete` are forbidden; prefer stack allocation,
`std::unique_ptr`, or standard containers.

**Rationale**: C++23 features exist to eliminate entire classes of bugs and to express
intent clearly. Enforcing them from the start prevents the accumulation of legacy
idioms that resist refactoring.

### II. Separation of Concerns

The domain model (`homestead::core` — `Resource`, `Entity`, `Registry`, quantities,
lifecycle) MUST have zero dependencies beyond the C++ standard library. It MUST NOT
contain any I/O, JSON parsing, logging calls, or solver logic.

The solver (`homestead::solver`), serialization layer (`homestead::serialization`),
and CLI harness (`homestead::cli`) are separate targets with one-way dependency arrows.
No circular dependencies between library targets are permitted.

**Rationale**: A pure domain model can be compiled, tested, and reasoned about in
isolation. Coupling it to I/O or algorithms makes the model impossible to reuse and
difficult to test without side effects.

### III. Testing (NON-NEGOTIABLE)

Every public API surface MUST have unit tests (Catch2 v3). A function or type is not
considered implemented until its tests exist and pass. Property-based tests MUST cover
solver invariants (e.g., total internal production ≥ total internal consumption for any
valid entity configuration). Integration tests for the solver MUST use hand-calculated
expected results — mocking the graph or registry is not acceptable in integration tests.

Tests live in `tests/<module>/` and mirror `include/homestead/<module>/`. A single test
file can be run with `ctest --preset debug -R <test_name>`.

**Rationale**: The solver handles circular resource dependencies via iterative
fixed-point convergence. Without property-based tests asserting convergence invariants,
regressions are silent and hard to trace.

### IV. Documentation

Every public header in `include/homestead/` MUST carry Doxygen-compatible doc comments
(`///` or `/** */`) on all exported types, functions, and constants. A top-level
`README.md` MUST exist with: project description, build prerequisites, build
instructions for all three presets, and a minimal usage example demonstrating the
solver API.

Internal implementation files under `src/` do not require Doxygen comments but MUST
have inline comments wherever the logic is non-obvious.

**Rationale**: As a library, the public headers are the primary interface. Without doc
comments, downstream users cannot use the library without reading implementation source.

### V. Code Style

All code MUST conform to the C++ Core Guidelines. clang-format (project `.clang-format`)
and clang-tidy (project `.clang-tidy`) MUST pass with zero warnings before any commit.

Naming conventions are non-negotiable:

- `snake_case` — functions and variables
- `PascalCase` — types, concepts, and template parameters
- `UPPER_SNAKE_CASE` — macros (minimise; prefer `constexpr` or concepts instead)

**Rationale**: Consistent style removes superficial friction in code review and makes the
codebase accessible to contributors already familiar with the C++ Core Guidelines.

### VI. Build System

The build system MUST be CMake 3.28+ using a target-based model. Global variable
mutation (`include_directories`, `add_definitions`, `link_libraries`) is forbidden; use
`target_*` commands with explicit `PUBLIC`/`PRIVATE`/`INTERFACE` visibility on every
target.

All dependencies (nlohmann/json, Catch2, spdlog) MUST be acquired via `FetchContent`
or `find_package` — no vendored source copies, no system-level install assumptions.
`CMakePresets.json` MUST define at least three presets: `debug`, `release`, `sanitize`.

**Rationale**: Target-based CMake prevents transitive dependency leakage and makes the
library trivially consumable via `add_subdirectory` or `FetchContent` by downstream
projects.

### VII. Continuous Integration

A GitHub Actions workflow MUST run on every pull request with a matrix covering:
GCC 13+, Clang 17+, and MSVC latest (Windows runner). Each matrix entry MUST build and
run the full test suite. The `sanitize` preset (ASan + UBSan) MUST run on at least one
Linux job per PR.

No PR MAY be merged if any CI job fails.

**Rationale**: Multi-compiler CI catches non-portable assumptions early. Sanitizer runs
catch memory errors and undefined behaviour that unit tests alone cannot detect.

### VIII. Performance

The solver MUST handle graphs with 10,000+ nodes in under 1 second on a reference
developer machine for typical homestead plans. This target MUST be validated by a
dedicated benchmark (not just unit tests) before a performance claim is made.

Optimisation MUST be preceded by profiling. Speculative optimisation without a measured
hotspot is a constitution violation. The solver MUST support circular resource
dependencies (e.g., chicken manure → compost → corn → feed → chicken) via iterative
fixed-point convergence, never assuming the graph is a DAG.

**Rationale**: Profile-first discipline prevents premature complexity. The 10 k-node
target is a concrete, testable bound derived from realistic homestead model sizes.

## Build System & CI Reference

All domain quantities use a `(min, expected, max)` triple — never a single scalar.
JSON schemas produced by `homestead::serialization` MUST include a `"version"` field;
breaking schema changes require a version bump. The `homestead::cli` target is off by
default (`HOMESTEAD_BUILD_CLI=OFF`) and exists solely as a minimal test harness.

Preset summary:

| Preset | Flags | Purpose |
|---|---|---|
| `debug` | `-O0 -g` | Day-to-day development |
| `release` | `-O3 -DNDEBUG` | Benchmarking and distribution |
| `sanitize` | ASan + UBSan | Memory and undefined-behaviour validation |

## Development Workflow

1. Every feature or bug fix MUST have an associated spec (`specs/<id>/spec.md`) before
   implementation begins.
2. A `plan.md` MUST pass the Constitution Check gate (all eight principles verified)
   before Phase 0 research.
3. All `NEEDS CLARIFICATION` markers in specs MUST be resolved before implementation
   tasks are written.
4. Complexity violations (e.g., adding a fifth CMake target, bypassing `std::expected`
   on a hot path) MUST be documented in the plan's Complexity Tracking table with
   explicit justification.
5. clang-format and clang-tidy are run as part of the pre-commit check; CI will reject
   diffs that fail either tool.

## Governance

This constitution supersedes all other development practices for homestead-engine.
Amendments require: (a) a written proposal describing the change and rationale,
(b) a version bump per the semantic versioning policy below, and (c) propagation of
the change to all dependent templates and guidance files.

**Versioning policy**:

- MAJOR — backward-incompatible governance change: removal or redefinition of a
  principle.
- MINOR — new principle or section added, or materially expanded guidance.
- PATCH — clarification, wording, or typo fix with no semantic change.

All PRs MUST include a "Constitution Check" in the plan confirming compliance with the
eight principles. Complexity justifications MUST be reviewed and accepted before merge.
For runtime development guidance, see the per-feature `plan.md` files and
`.specify/templates/agent-file-template.md`.

**Version**: 1.0.0 | **Ratified**: 2026-04-07 | **Last Amended**: 2026-04-07
