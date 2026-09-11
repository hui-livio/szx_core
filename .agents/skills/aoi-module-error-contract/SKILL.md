---
name: aoi-module-error-contract
description: Enforce stable typed error-code contracts across AOI_PCB modules, DLLs, services, repositories, workers, callbacks, Qt signals, futures, controllers, and UIs. Use this skill whenever adding or changing APIs, DTOs, Result types, exceptions, async payloads, error mappings, or module error-code namespaces/offsets. It requires module-owned codes, one-time offset qualification at aggregation boundaries, and exception containment so raw exceptions, diagnostics, and lower-layer codes never cross forbidden boundaries.
---

# AOI Module Error Contract

Make the owning module's stable error code the only failure identity that crosses a boundary. Keep raw diagnostics at their source and translate user-facing text only at the App/UI presentation edge.

Apply this skill together with `aoi-repository-development`. For PCB DetectEngine work, also follow `pcb-detection-development` and its maintained specification.

## Required workflow

1. Draw the actual producer-to-consumer chain, including DLL, thread, callback, future, signal, controller, and QML boundaries.
2. For every boundary, identify:
   - the module that owns the public business meaning;
   - the stable public error code or typed enum;
   - the lower-level diagnostic owner;
   - retry, cancellation, timeout, and resource semantics;
   - the final UI translation owner.
3. Search the current contract and all producers/consumers before editing:

   ```text
   rg -n "ErrorCode|PcbResult|OperationError|errorCode|errorMessage|diagnostic|what\(\)" detectengine apps modules test
   rg -n "userFacingMessage|translate|qsTr|QString.*error|Result<" apps detectengine modules test
   ```

4. Choose or add a code in the owning public domain. Reuse an existing code only when its observable semantics, retryability, and scope match exactly.
5. For a composed business module, assign every child module a unique numeric offset and qualify its local code exactly once at the business boundary: `published_code = module_offset + local_code`. Preserve the module-local code inside its owning module; do not let each nested layer add the offset.
6. Convert internal/common/third-party failures once at the owning module boundary. Preserve an already-public code unchanged when no new business meaning is introduced, and do not apply an offset a second time.
7. Carry only the typed code and the normal success value across the boundary. Add structured parameters only when they are a deliberate, versioned part of the public contract and are required by the consumer; never forward arbitrary lower-layer error fields.
8. Log the diagnostic once at the source or conversion boundary with stable correlation fields. Never use the log text as a protocol.
9. Map code to `OperationErrorKind`, retryability, UI translation key/text, and control policy at the appropriate outer layer.
10. Update implementations, adapters, async payloads, fakes, tests, translations, and public-contract documentation in the same change.
11. Verify the complete producer-to-consumer path and negative cases before reporting completion.

Read [error-boundary-patterns.md](references/error-boundary-patterns.md) when designing, implementing, or reviewing a boundary.

## Mandatory contract

### Public and module boundaries

- Use a scoped enum with explicit underlying type or a stable domain error-code set. Published numeric meanings must not drift or be reused.
- For DetectEngine public/DLL APIs, return `PcbResult<T>`: success value plus stable code, without message or diagnostic fields.
- Do not expose `SZX::COMMON::Result<T>` across a boundary that forbids its diagnostic payload.
- Do not pass `message`, `errorMessage`, `failureMessage`, `reason`, `diagnostic`, `exception.what()`, translated `QString`, database/OS/SDK text, or arbitrary string maps as failure identity.
- Do not expose private library error integers directly. Map them into the owning module's public domain code before returning.
- Do not collapse cancellation, timeout, queue full, permission, disk full, not found, invalid request, and execution failure into one generic code.
- Do not invent an Engine error code for a UI-only waiting or watchdog state. The layer that owns the terminal outcome owns the error code.

### Module error-code namespaces and offsets

- Each module owns a stable local error-code range and its meanings. Local codes are valid inside that module and must not be renamed merely because an outer business module qualifies them.
- A composed business module must maintain one explicit offset registry, with one unique offset per child module. Reserve non-overlapping ranges large enough for current and planned local codes; document the owner and range together.
- At the aggregation boundary, qualify a child failure with `published_code = module_offset + local_code`. For example, with offsets A=`20000`, B=`30000`, C=`40000`, and D=`50000`, local `1801`/`1802` become A `21801`/`21802`, B `31801`/`31802`, C `41801`/`41802`, and D `51801`/`51802`.
- Apply the offset exactly once, at the boundary where the child module's identity is known. Never add offsets in leaf code, generic adapters, every hop of an async chain, or multiple nested aggregators.
- Do not offset success, cancellation markers, or codes outside the child module's declared local range. Validate range membership and integer overflow before qualification; map invalid or unknown child codes to a stable owning-module fallback instead of producing an ambiguous number.
- A qualified code must remain machine-readable as the child module plus local code. Keep a reverse lookup or equivalent typed mapping for logs, tests, retryability, and UI translation; do not recover ownership by parsing display text.
- Check offset-range collisions and double-offset behavior in tests. A child code that is already qualified must be explicitly tagged or typed so it cannot be qualified again accidentally.

### Exception containment

- Catch exceptions at every boundary that accepts code from a lower layer, SDK, callback, worker, or DLL. Convert them to the owning module's stable local code, then apply the module offset only if this is the designated aggregation boundary.
- Do not allow `std::exception`, `exception.what()`, foreign exceptions, or arbitrary exception objects to cross a module/API/thread/UI boundary. Log diagnostics locally with correlation fields; return only the typed failure code and required value/state.
- Catch `std::bad_alloc` as resource exhaustion and unknown exceptions as the owning module's stable unexpected-failure code. The outer boundary must not reinterpret an already-qualified code as a new local code.

### Async and status boundaries

- Make worker/future/callback/signal payloads typed outcomes containing value/state plus error code; never use empty/non-empty text as success/failure protocol.
- Preserve request/job/product/session/generation identity so a late result cannot update the wrong state.
- Carry typed phase/stage/status codes, percentages, and counts for progress. Localize after queued delivery on the GUI thread.
- Catch all exceptions at worker, SDK, callback, and DLL boundaries. Map `std::bad_alloc` to resource exhaustion and unknown exceptions to a stable unexpected-failure code; log `what()` locally only.
- Keep cancellation semantics domain-specific and consistent at every cancellation point, including queued, running, and completed-future paths.

### UI and control flow

- Translate error codes centrally with `QCoreApplication::translate()`/`qsTr()` at the presentation edge.
- Branch behavior, colors, retry actions, and state transitions on typed codes/enums, never localized text or diagnostic substrings.
- Keep display text separate from machine state. QML may test whether display text is empty to control visibility, but must not parse it to recover error identity.
- Use specific code mappings before generic `OperationErrorKind` fallbacks. Generic wording must remain domain-neutral.

## Adding an error code

Complete this vertical slice atomically:

1. Define a uniquely owned, stable local code and document its exact trigger and local range.
2. Return it from every producer path representing that condition.
3. Map internal/common/SDK codes at the owning boundary.
4. If exposed through a composed business module, register the child offset, prove the range is non-overlapping, and qualify it exactly once.
5. Map the local/qualified code to kind and retryability.
6. Add centralized localized UI text and update translation catalogs.
7. Preserve it through synchronous and asynchronous adapters without text conversion.
8. Test the producer, boundary result, offset qualification, double-offset rejection, async propagation, UI mapping, and absence of diagnostic leakage.

Do not add a code that no production path can emit. Do not add UI mapping without migrating the producer.

## Review checklist

- Trace each failure from its source to the final UI consumer.
- Reject free-form or translated text in public DTOs, futures, callbacks, signals, progress, and status structures.
- Reject string codes, numeric parsing, `contains()` routing, and stage-name conventions used as control logic.
- Confirm the originating domain code survives without accidental reclassification.
- Confirm every composed-module code uses a registered, collision-free offset exactly once and can be reverse-mapped to its owner/local code.
- Confirm all switch mappings cover new codes and have a safe fallback.
- Confirm exceptions are contained at each boundary and `what()` never enters a returned payload.
- Confirm diagnostics are logged once, redacted, and correlated without entering the returned payload.
- Confirm fakes and tests compile against the same contract and cover every new code.

## Completion evidence

- Report the traced boundary chain and the owner of every conversion.
- Report added or reused codes and why their semantics match.
- Report directed tests for code preservation, exception containment, cancellation/resource distinctions, UI translation, and no-text transport.
- Run the narrowest affected build/tests, formatting, and `git diff --check`; follow the higher verification level required by the owning technology/domain skill.
