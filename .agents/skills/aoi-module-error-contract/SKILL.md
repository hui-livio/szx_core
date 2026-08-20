---
name: aoi-module-error-contract
description: Enforce stable typed error-code contracts across AOI_PCB module, DLL, service, repository, worker, callback, Qt signal, future, controller, and UI boundaries. Use when adding or changing APIs, DTOs, Result types, exceptions, async task payloads, progress/status events, error mappings, retry/cancellation behavior, logs, Toasts, or translations, and when reviewing for diagnostic text, QString, exception.what(), third-party codes, string parsing, or lower-layer error fields crossing a boundary.
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
5. Convert internal/common/third-party failures once at the owning module boundary. Preserve an already-public code unchanged when no new business meaning is introduced.
6. Carry only the typed code and the normal success value across the boundary. Add structured parameters only when they are a deliberate, versioned part of the public contract and are required by the consumer; never forward arbitrary lower-layer error fields.
7. Log the diagnostic once at the source or conversion boundary with stable correlation fields. Never use the log text as a protocol.
8. Map code to `OperationErrorKind`, retryability, UI translation key/text, and control policy at the appropriate outer layer.
9. Update implementations, adapters, async payloads, fakes, tests, translations, and public-contract documentation in the same change.
10. Verify the complete producer-to-consumer path and negative cases before reporting completion.

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

1. Define a uniquely owned, stable code and document its exact trigger.
2. Return it from every producer path representing that condition.
3. Map internal/common/SDK codes at the owning boundary.
4. Map it to kind and retryability.
5. Add centralized localized UI text and update translation catalogs.
6. Preserve it through synchronous and asynchronous adapters without text conversion.
7. Test the producer, boundary result, async propagation, UI mapping, and absence of diagnostic leakage.

Do not add a code that no production path can emit. Do not add UI mapping without migrating the producer.

## Review checklist

- Trace each failure from its source to the final UI consumer.
- Reject free-form or translated text in public DTOs, futures, callbacks, signals, progress, and status structures.
- Reject string codes, numeric parsing, `contains()` routing, and stage-name conventions used as control logic.
- Confirm the originating domain code survives without accidental reclassification.
- Confirm all switch mappings cover new codes and have a safe fallback.
- Confirm diagnostics are logged once, redacted, and correlated without entering the returned payload.
- Confirm fakes and tests compile against the same contract and cover every new code.

## Completion evidence

- Report the traced boundary chain and the owner of every conversion.
- Report added or reused codes and why their semantics match.
- Report directed tests for code preservation, exception containment, cancellation/resource distinctions, UI translation, and no-text transport.
- Run the narrowest affected build/tests, formatting, and `git diff --check`; follow the higher verification level required by the owning technology/domain skill.
