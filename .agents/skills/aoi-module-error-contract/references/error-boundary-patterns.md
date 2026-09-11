# Error boundary patterns

Use this reference to classify a boundary and select the repository pattern. Current source files remain authoritative; inspect them before changing code.

## Repository anchors

| Concern | Current anchor | Required use |
|---|---|---|
| PCB public codes | `detectengine/pcb/include/PcbDetectEngineCommonTypes.h` | Stable public meanings and numeric values |
| PCB public result | `detectengine/pcb/include/PcbEngineResult.h` | Code-only DetectEngine DLL result |
| Error classification | `detectengine/pcb/include/PcbOperationError.h` and `detectengine/pcb/src/PcbOperationError.cc` | Kind and retryability mapping |
| App presentation | `apps/PCBDetect/src/engine/OperationErrorSupport.*` | Exception containment and centralized translation |
| Internal generic result | `modules/common/Result.hpp` | Internal diagnostics only where the boundary permits them |
| Maintained rules | `docs/规范/PCB检测开发规范与设计护栏.md`, section 6 | Three-layer error and i18n contract |

Do not copy the current error-code list into a skill or another module; it will drift. Read the owning header.

## Boundary matrix

| Boundary | Allowed failure payload | Forbidden payload | Conversion owner |
|---|---|---|---|
| Internal function within one module | Internal typed failure and local diagnostic when useful | UI translation | Owning implementation |
| Common/SDK/DB/filesystem to domain module | Private failure before conversion | Private numeric codes or raw text leaving the module | Domain adapter/repository/service |
| DetectEngine public/DLL API | `PcbResult<T>` with stable code and value | `Result.message`, diagnostic, exception | Outermost Engine boundary |
| Worker to GUI future/watcher | Typed value/outcome plus stable code and identity | `QString error`, translated text, `exception.what()` | Worker extracts code; GUI presenter translates |
| Callback/Qt signal | Typed state/stage/error code and identity | Display labels, raw reason/message | Callback producer/adapter |
| Controller/model to QML | Typed code/state for logic; separately localized display text | Parsing or comparing localized text | Controller/presenter |

## Good patterns

### Public result

```cpp
auto load(...) -> PcbResult<Value>
{
    const auto loaded = repository.load(...); // internal result may own diagnostics
    if (!loaded.ok)
    {
        logFailure(loaded.error, loaded.message);
        return PcbResult<Value>::Failure(mapRepositoryError(loaded.error));
    }
    return PcbResult<Value>::Success(std::move(loaded.value));
}
```

If `loaded.error` is already the same public domain code, preserve it instead of remapping it to a generic execution failure.

### Async result

```cpp
struct TaskResult
{
    Value value{};
    int errorCode = ErrorCode::Success;
    RequestId requestId{};
};
```

The worker logs diagnostics and stores only `errorCode`. The GUI-thread finished handler validates identity, then translates the code.

### Progress/status

```cpp
struct Progress
{
    StageCode stage = StageCode::Unknown;
    int percent = 0;
    int errorCode = ErrorCode::Success;
};
```

The UI maps `stage` and `errorCode`; the producer never fills `stageLabel` or `message`.

## Rejected patterns

```cpp
Result<Value> publicApi();                 // Result carries free-form message
QString prepareHardware();                 // empty means success, text means failure
emit completed(false, exception.what());   // diagnostic crosses thread/UI boundary
if (error.contains("database")) { ... }    // text is control protocol
if (state == tr("Pending review")) { ... } // translation is machine state
status.failureCode = std::to_string(code); // stringly typed code
```

Also reject DTO fields such as `failureMessage`, `reason`, or `metrics["error"]` when they carry lower-layer failure details. A structured parameter is allowed only when it has an explicit type, validation, privacy rule, and stable consumer need independent of diagnostics.

## Mapping decisions

- Preserve: the incoming code already has the same public owner and meaning.
- Translate: a private/common/SDK code must become the domain's stable public code.
- Refine: multiple business-distinct conditions currently share a generic code and the consumer needs different policy or text.
- Generalize: implementation-specific failures have the same externally observable action and disclosure would leak internals.
- Keep local: timing out a UI spinner while work continues is presentation state, not a terminal Engine failure.

Never infer mapping from diagnostic text. Use the typed source condition at the point where it is still known.

## Composed-module error codes

When one business module depends on several child modules, keep child meanings local and qualify them only at the business aggregation boundary:

```text
published_code = child_module_offset + child_local_code
```

Example registry:

| Child module | Offset | Local `1801` | Local `1802` |
|---|---:|---:|---:|
| A | 20000 | 21801 | 21802 |
| B | 30000 | 31801 | 31802 |
| C | 40000 | 41801 | 41802 |
| D | 50000 | 51801 | 51802 |

The registry is the source of truth. Reserve non-overlapping ranges, validate that the local code belongs to the child range, and reject overflow or unknown codes. The same qualified value must not be offset again by an adapter, worker, callback, future, or nested aggregator. Use a typed qualified-code representation or explicit metadata so double qualification is detectable without parsing numbers or text.

The child module maps exceptions and private/SDK failures to its own stable local code first. The aggregator catches any boundary exception that remains, maps it to its own stable fallback, and qualifies only child failures whose ownership is known. No exception, diagnostic string, or `what()` crosses the module boundary.

## Required test slice

For each new or changed code, cover:

1. Producer emits the expected code for the exact condition.
2. Public result contains no diagnostic text and has valid success/failure invariants.
3. Adapter/future/callback/signal preserves the code and stable identity.
4. `makeOperationError` produces the expected kind and retryability.
5. UI presenter returns the intended translatable message/key.
6. Unknown exceptions and `std::bad_alloc` map to stable fallback codes without exposing `what()`.
7. Cancellation, capacity, permission, not-found, and execution failures remain distinguishable.
8. Fakes, capability tests, and ABI/compile-contract tests use the new signature.
9. Offset registry tests prove non-overlapping ranges, expected A/B/C/D examples, one-time qualification, reverse mapping, overflow rejection, and unknown-code fallback.
