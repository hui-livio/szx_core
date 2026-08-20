---
name: aoi-repository-development
description: Enforce repository-first capability and UI-component reuse plus repository-owned formatting for any implementation, fix, refactor, code review, or pre-commit work in AOI_PCB. Use whenever changing source code, especially for time and elapsed measurement, files/folders/paths, strings/JSON/UUID, results and cleanup, queues/thread pools/message buses, geometry, Qt/QML UI and UT/UIToolkits components, or whenever a local helper, raw standard-library/Qt control, new dependency, or ad hoc formatting command might duplicate repository capabilities or conventions.
---

# AOI Repository Development

Prefer proven repository capabilities over local reinvention. Apply this skill before technology-specific skills; continue to use C++, Qt, QML, OpenCV, build, testing, and PCB-domain skills for their own constraints.

## Capability priority

For general-purpose mechanics, use this order:

1. Inspect `modules/common` for an existing capability.
2. Inspect existing repository call sites and tests to confirm intended semantics.
3. Reuse another established repository abstraction when `common` does not own the concern.
4. Use a standard-library, Qt, or third-party API directly only when the repository has no suitable abstraction or the required semantics differ materially.
5. Add or extend `common` only for a genuinely cross-module capability with a stable, business-neutral contract and tests.

Do not create a local wrapper merely to rename an existing common API. Do not copy a common implementation into another module.

Domain abstractions still own business semantics. `common` provides mechanics; it does not replace typed domain errors, atomic publication protocols, access policy, cancellation rules, or UI localization.

For UI mechanics and visual primitives, apply the same repository-first rule to `modules/UIToolkits`: prefer the established `UT*` components and `UTTheme` before raw Qt Quick Controls or app-local duplicates.

## Required workflow

1. Classify the need: time, file/folder/path, text/JSON, identity, result/cleanup, concurrency/messaging, geometry, serialization, UI component/theme, or another utility.
2. Search before coding:

   ```text
   rg --files modules/common
   rg -n "<candidate type or operation>" modules/common test/modules/common
   rg -n "<candidate API>" apps modules detectengine test
   rg --files modules/UIToolkits/qml test/modules/uitoolkits
   rg -n "<candidate UT component or interaction>" modules/UIToolkits apps test
   ```

3. Read the complete candidate header plus relevant tests and representative callers. Never infer behavior from its filename alone.
4. Compare units, clock type, UTC/local behavior, encoding, path normalization, overwrite/atomicity, resource bounds, thread safety, ownership, exceptions, and failure representation.
5. Reuse the capability when its contract fits. Link `szx::common` through the owning CMake target instead of adding manual include paths.
6. At business, thread, callback, DLL, Qt, device, database, or file-publication boundaries, translate common failures into the owning module's stable error code. Keep diagnostics local.
7. If no capability fits, record why before introducing another implementation. Prefer extending common only when at least two independent modules can use the same business-neutral behavior.
8. Verify with the narrowest build and tests. Before committing, discover and run the repository-owned formatter for every changed code type, then run its check mode when available and `git diff --check`. When common changes, run its unit tests and every affected consumer lane.

Read [common-capabilities.md](references/common-capabilities.md) whenever the task touches a listed area.

## Mandatory choices

### Time

- Use `TimeHelper::NowTimestamp`, ISO-8601 conversion, and parsing for wall-clock timestamps.
- Use `Stopwatch` or `ScopeTimer` for elapsed time and performance logging.
- Use `std::chrono` directly for typed durations, deadlines, waits, or APIs not covered by common; do not build another elapsed-time helper around `steady_clock`.
- Never use wall-clock timestamps to measure elapsed duration.

### Files, folders, and paths

- Use `FileHelper` for basic existence, size, extension, text/byte read-write, copy, and single-file removal.
- Use `FolderHelper` for directory creation, parent creation, enumeration, absolute paths, and weak canonicalization.
- Treat their `bool`, empty collection, and `optional` results as foundation outcomes. Map failures at the owning business boundary; do not expose them as ambiguous UI success/failure.
- Do not assume these helpers provide atomic publication, locking, safe recursive deletion, permission classification, disk-full classification, symlink confinement, or crash recovery. Use or design the domain protocol when those properties are required.

### Text, data, identity, and cleanup

- Prefer `StringHelper`, `JsonHelper`, `Uuid`, common geometry types, and repository serialization adapters to ad hoc parsing or duplicated value types.
- Use `Defer` or a more specific RAII owner for reversible cleanup.
- Use `Result<T>` only inside boundaries that permit its diagnostic payload. Public PCB DetectEngine boundaries must use their code-only result contract.

### Concurrency and messaging

- Inspect `BlockingQueue`, `ThreadPool`, `WorkQueueService`, and `MessageBus` before creating local worker/queue/pub-sub infrastructure.
- Configure explicit capacity, shutdown, cancellation, retention, and backpressure. Never accept an unbounded default accidentally.
- Keep exceptions contained at worker and callback boundaries and preserve stable domain cancellation/resource error codes.

### UI components and theme

- Before creating a QML control, dialog, loading mask, toast, input, list/tree, chart, image viewer, title bar, or common visual primitive, inspect `modules/UIToolkits/qml`, its call sites, and `test/modules/uitoolkits/qml`.
- Prefer `import UIToolkits`, an existing `UT*` component, and `UTTheme` tokens when the component contract fits. Do not reproduce its behavior or styling with raw Qt Quick Controls, local `Rectangle`/`MouseArea` compositions, copied QML, or hard-coded colors and dimensions.
- Compose application-specific workflows in the application module. Only extend or add a UIToolkits component when it remains independently reusable outside that application and its properties, signals, focus, keyboard, accessibility, theme, lifecycle, and tests form a stable generic contract.
- If a UT component does not fit, record the concrete semantic or interaction mismatch. Do not bypass it merely to obtain a slightly different appearance.
- When changing UIToolkits, update its QML module/CMake registration when needed and add or update the matching Qt Quick test. Verify representative application consumers as well as the component test.

### Pre-commit formatting

- Detect repository formatting ownership before committing: inspect formatter scripts, `.clang-format` or equivalent configuration, CI checks, and maintained development instructions. If the repository defines a formatter for a changed code type, running it is mandatory.
- For this repository's tracked C/C++ sources, use `scripts/format_cpp.ps1` with the repository `.clang-format`; do not substitute an IDE profile or an ad hoc style. Use `-IncludeTests` whenever test C/C++ files are part of the intended change.
- Inspect `git status --short` and formatter scope before formatting so unrelated user changes are not silently rewritten. Prefer repository-supported roots/options to constrain the authorized scope; review the resulting diff immediately.
- Run format mode before the final build/test evidence, then run check mode with the same scope before commit. A normal production-plus-test command pair is:

  ```powershell
  .\scripts\format_cpp.ps1 -Mode Format -IncludeTests
  .\scripts\format_cpp.ps1 -Mode Check -IncludeTests
  ```

- For QML, scripts, documents, or other languages, use the repository-owned formatter only when one exists. Do not invent a broad mechanical rewrite in the absence of an established format contract.
- Treat a missing required formatter as an unexecuted gate: report it explicitly instead of claiming the change is commit-ready.

## Adding to common

Only add a common capability when all are true:

- It is business-neutral and useful across modules.
- Its ownership, threading, units, bounds, and failure behavior are explicit.
- It does not depend on PCB, QML, database rows, device SDKs, or another business layer.
- Existing common APIs cannot be extended coherently.
- Unit tests cover success, boundary values, failure, cleanup, and concurrency where applicable.
- `modules/common/CMakeLists.txt` and affected target links are updated together.

Avoid turning `common` into a catch-all. A module-specific helper belongs with its owning module when reuse is speculative.

## Completion check

- Confirm no new local time/file/string/queue helper duplicates common.
- Confirm new UI work reused a suitable `UT*`/`UTTheme` capability, or report the verified contract mismatch and why application-local UI is appropriate.
- Confirm direct standard-library or Qt utility use has a concrete semantic reason.
- Confirm common failure details do not cross a stable business/UI boundary.
- Confirm file and queue operations meet atomicity, safety, and capacity requirements beyond basic helper behavior.
- Confirm every repository-defined formatter applicable to changed files ran before commit, its check mode passed when available, and formatting did not pull unrelated files into the change.
- Report which common capability was reused or why none fit.
