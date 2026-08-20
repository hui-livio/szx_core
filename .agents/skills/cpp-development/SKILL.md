---
name: cpp-development
description: Apply the repository's C++20 production-development rules when creating, changing, refactoring, or reviewing C++ code. Use this skill for any C++ implementation or public API work, especially code involving exceptions, Result<T>, error codes, RAII, locks, handles, transactions, temporary files, service boundaries, callbacks, worker threads, futures, Qt adapters, C APIs, plugins, common/foundation utilities, or third-party libraries. Standard-library-style common/foundation code may throw documented runtime exceptions; all acquired resources must be protected by RAII before throwing work begins; business, service, thread, callback, ABI, and other stable external boundaries must contain exceptions and communicate failures through stable module error codes and SZX::COMMON::Result<T> or an equivalent explicit result contract.
---

# C++ Development

Write maintainable C++20 code with explicit ownership and failure contracts. Preserve the repository's existing target, namespace, formatting, and test conventions.

## Core error rule

Use two explicit error zones:

- Standard-library-style `common`/foundation utilities may throw documented runtime exceptions when they cannot complete. This keeps generic infrastructure independent of PCB business error enums.
- Algorithms and private implementation code may also use exceptions internally.
- Business modules must catch exceptions from `common`, the standard library, and third-party libraries at their stable external boundary.
- No exception may escape a business/service boundary, thread or task entry, callback, Qt/C/plugin/SDK adapter, CLI command handler, persistence transaction façade, or exported ABI.
- Convert failures at those boundaries to a stable owning-module error code carried by `SZX::COMMON::Result<T>`, `Result<void>`, or the boundary's existing explicit result type.
- Do not communicate failure across a boundary with only `bool`, an empty value, a log line, `errno`, or exception text.
- Do not expose third-party exception types in public headers or error contracts.
- Exception handling never replaces cleanup. Put every owned resource, lock, transaction, temporary output, callback registration, and state rollback action under RAII before any later operation can throw.

Therefore, do not mechanically replace every throwing `common` API with `Result<T>`. The consumer that owns the business operation is responsible for translating the exception into its public error domain.

## Error-policy layers

| Layer | May throw? | Outgoing failure contract |
|---|---|---|
| `common`/foundation utility | Yes, documented runtime exceptions | Exception to the immediate C++ consumer; no PCB/business error enum |
| Private algorithm/module implementation | Yes | Contained before the owning module's stable external boundary |
| Business/service public boundary | No | Stable module error code + `Result<T>`/explicit status |
| Worker/task/timer/callback entry | No | Failed result/event/status; never terminate the worker |
| C ABI/plugin/SDK/driver/Qt adapter | No | Numeric/status error contract safe for that boundary |

Foundation exceptions should describe programming/runtime failure using appropriate standard types such as `std::invalid_argument`, `std::out_of_range`, `std::runtime_error`, or `std::system_error`. Avoid inventing a business-specific exception hierarchy in `common`.

## Identify boundaries before coding

Consider all of these stable external boundaries:

- exported/public methods called by another CMake target or business layer;
- DLL/shared-library, plugin, C ABI, SDK, device-driver, database, model-runtime, OpenCV, or Qt adapter entry points;
- thread, worker, task-queue, timer, event-loop, callback, signal/slot, future, or promise entry functions;
- function objects supplied by another module or invoked asynchronously;
- serialization, IPC, CLI, GUI-controller, and persistence edges.

Private calls within one owned implementation and calls into standard-library-style `common` utilities are not automatically error-code boundaries. Define a stable boundary from business ownership, asynchronous execution, ABI, and deployment—not only from namespaces, folders, or CMake targets.

## Workflow

1. Classify the code as foundation utility, private implementation, or stable external boundary.
2. Inspect the local API, its callers, existing module error enum, and nearby `Result<T>` usage.
3. State which functions are stable boundaries and which standard/common/third-party operations inside them may throw.
4. Define or reuse stable, semantic error codes before adding boundary `try/catch`.
5. Inventory every resource acquired before potentially throwing work: memory, mutex, OS/device handle, file, transaction, temporary output, thread, registration, and mutable state.
6. Wrap each resource in an RAII owner immediately after acquisition; do not leave an ownership gap between acquisition and guard construction.
7. Keep exception-throwing work in a narrow internal or foundation function where practical.
8. Catch at the nearest owning stable boundary, translate once, and return an explicit failure.
9. Propagate an existing failed `Result<T>` without throwing it or replacing a specific error with a generic one.
10. Add unit tests for foundation exception behavior, RAII cleanup, rollback, boundary mapping, and non-escape guarantees.
11. Review every new `throw`, `try`, `catch`, `noexcept`, callback, asynchronous entry point, manual acquire/release pair, and lock operation before completion.

## Boundary implementation pattern

Prefer a small boundary wrapper around an internal operation:

```cpp
enum class ModelError
{
    InvalidRequest = 1,
    ModelUnavailable,
    DecodeFailed,
    UnexpectedFailure,
};

namespace
{
ModelData loadModelImpl(const ModelRequest &request) // May throw internally.
{
    auto file = openModelFile(request.path); // Returns an RAII file owner.
    auto parser = makeParser();              // unique_ptr or another RAII owner.
    return parser->parse(file);              // All earlier resources unwind safely.
}
} // namespace

SZX::COMMON::Result<ModelData> ModelService::loadModel(const ModelRequest &request)
{
    try
    {
        return SZX::COMMON::Result<ModelData>::Success(loadModelImpl(request));
    }
    catch (const InvalidModelRequest &error)
    {
        return SZX::COMMON::Result<ModelData>::Failure(
            static_cast<int>(ModelError::InvalidRequest), sanitize(error.what()));
    }
    catch (const ThirdPartyModelError &error)
    {
        logInternalFailure(error); // Internal diagnostic; do not expose vendor type.
        return SZX::COMMON::Result<ModelData>::Failure(
            static_cast<int>(ModelError::DecodeFailed), "Failed to decode model data.");
    }
    catch (const std::exception &error)
    {
        logInternalFailure(error);
        return SZX::COMMON::Result<ModelData>::Failure(
            static_cast<int>(ModelError::UnexpectedFailure), "Unexpected model failure.");
    }
    catch (...)
    {
        logUnknownInternalFailure();
        return SZX::COMMON::Result<ModelData>::Failure(
            static_cast<int>(ModelError::UnexpectedFailure), "Unexpected model failure.");
    }
}
```

Adapt names to the owning module. Do not copy one module's error enum into another module merely to reuse numeric values.

## RAII is mandatory with exceptions

An exception can bypass every statement between the throw point and the matching handler. Manual cleanup placed after a throwing call, or duplicated inside selected catch blocks, is therefore incomplete.

Use the narrowest standard or repository RAII type:

| Resource | Preferred ownership |
|---|---|
| Heap object | value semantics, `std::unique_ptr`, or `std::shared_ptr` only for genuine shared ownership |
| Mutex | `std::lock_guard`, `std::unique_lock`, or `std::scoped_lock` |
| File/stream | stream object or a dedicated file owner whose destructor closes it |
| Native/SDK handle | `std::unique_ptr` with custom deleter or a small move-only handle class |
| Thread | `std::jthread`, or a move-only owner that joins/stops in a non-throwing destructor |
| Transaction | transaction guard that rolls back unless explicitly committed |
| Temporary output | output transaction that removes/abandons the temporary artifact unless committed |
| Registration/callback | connection/subscription token that unregisters on destruction |
| Arbitrary reversible action | repository scope guard such as `Defer`, with a non-throwing cleanup action |

Rules:

- Establish ownership immediately. Avoid `T *p = acquire();` followed by operations before constructing the guard.
- Acquire a mutex through a lock object, never a bare `lock()` followed later by `unlock()` across throwing code.
- Do not manually `delete`, `free`, `close`, `unlock`, `rollback`, or remove temporary files in multiple catch branches.
- Destructors and deleters used during stack unwinding must be `noexcept` in effect. They must contain cleanup failures and use only safe diagnostic reporting.
- Keep lock scope as small as correctness allows. Do not perform slow I/O, callbacks, logging hooks, or third-party work while holding a lock unless the invariant requires it.
- Destruction order is reverse construction order. Construct guards in an order that makes dependent resources release safely.
- RAII protects resource lifetime, not business atomicity by itself. Use commit/rollback guards so an exception cannot expose partially updated state.
- A catch block may translate or recover after unwinding; it should not be the primary cleanup mechanism.

Bad:

```cpp
mutex.lock();
Handle handle = sdkOpen();
auto value = sdkRead(handle); // Throws: lock and handle leak.
sdkClose(handle);
mutex.unlock();
```

Good:

```cpp
std::unique_lock lock(mutex);
UniqueSdkHandle handle{sdkOpen(), SdkHandleCloser{}};
auto value = sdkRead(handle.get());
return value; // Normal return and exception unwinding release both resources.
```

## Error-code design

- Use an `enum class` owned by the module; assign stable values when the code crosses a persisted, ABI, CLI, or IPC boundary.
- Describe business meaning such as `InvalidRequest`, `NotFound`, `Unavailable`, `Cancelled`, `Timeout`, `PermissionDenied`, `ResourceExhausted`, or `ExecutionFailed`.
- Reserve a distinct `UnexpectedFailure`/`InternalFailure` for unmapped exceptions. Do not map every exception to `InvalidRequest`.
- Preserve a downstream error code when the current layer adds no new meaning. Map it only when crossing into a different public error domain.
- Keep the public message bounded, sanitized, and useful. It is diagnostic text, not a stable identifier.
- Never put passwords, keys, connection strings, personal data, image payloads, model bytes, or sensitive filesystem details in returned messages or logs.
- Record detailed exception type/context only in the owning module's internal log when safe.

## Catching rules

- Catch only where the code can translate, recover, or terminate an asynchronous operation safely. Routine cleanup belongs to RAII, not the handler.
- Catch specific domain/third-party exceptions before `std::exception`; add `catch (...)` at a true boundary.
- Avoid `catch (...)` inside ordinary algorithms if it only hides defects or returns success.
- Never swallow an exception and continue with partially initialized state.
- Do not catch merely to log and rethrow at every layer; this creates duplicate logs. Log once at the owning boundary.
- Use RAII so cleanup, unlock, close, unregister, rollback, and temporary-file abandonment do not depend on a catch block.
- Destructors and cleanup callbacks must not emit exceptions. Catch internally and report through an allowed diagnostic channel.
- A `noexcept` function must not call potentially throwing work unless it contains every exception path. Apply `noexcept` deliberately, not cosmetically.

## Threads, callbacks, and async work

Thread and callback exceptions otherwise lead to termination or lost work, so treat their entry functions as mandatory boundaries:

```cpp
auto task = [request]() -> SZX::COMMON::Result<Output>
{
    try
    {
        return runTaskImpl(request);
    }
    catch (const std::exception &error)
    {
        return makeTaskFailure(TaskError::ExecutionFailed, error);
    }
    catch (...)
    {
        return makeTaskFailure(TaskError::UnexpectedFailure);
    }
};
```

- Fulfil promises/futures with an explicit failed result; do not leave a broken promise as the business error contract.
- Convert callback exceptions before returning to Qt, C, SDK, driver, or plugin code.
- Do not allow a worker exception to terminate the worker thread or process.
- Preserve cancellation and timeout as their own error codes rather than generic exceptions.

## Public API rules

- Public headers should expose value types, stable error enums, and result types, not exception classes required by callers.
- Do not require callers to wrap every module call in `try/catch`.
- C ABI functions must never allow C++ exceptions to escape and should return a numeric/status result with documented output ownership.
- ABI callbacks must validate pointers and lengths before entering implementation code.
- If a legacy public API currently throws, do not silently change compatibility. Add a non-throwing façade or plan a versioned migration, then update all callers and tests.

## Testing requirements

For foundation utilities, test the documented exception type and trigger condition. For each stable external boundary, test:

- successful value/result;
- each intentionally mapped domain or third-party exception;
- `std::exception` fallback;
- non-standard exception through `catch (...)`;
- returned error code and sanitized message;
- no exception escapes (`EXPECT_NO_THROW` around the boundary when useful);
- no partial output/state commit after failure;
- asynchronous completion receives a failed `Result<T>` rather than process termination or broken state;
- existing failed results retain their code when propagated.
- every acquired fake resource is released exactly once after success, documented exceptions, `std::exception`, and unknown exceptions;
- mutexes are available after failure and no deadlock remains;
- transactions roll back and temporary outputs are not committed after failure;
- cleanup/destructor paths do not throw while another exception is active.

Use injected fakes that throw deterministically. Do not force real hardware, database, GPU, or filesystem corruption just to exercise translation.

## Review checklist

- [ ] Every added throwing operation has an owning boundary.
- [ ] Public, thread, callback, C ABI, plugin, and adapter boundaries catch all exception categories.
- [ ] Failures leave the module as `Result<T>`/status plus a stable module error code.
- [ ] No public signature exposes a third-party exception contract.
- [ ] Error codes are semantic, stable, and not all collapsed into one generic value.
- [ ] Messages and logs contain no sensitive data.
- [ ] Every resource is placed under RAII immediately after acquisition, before later throwing operations.
- [ ] Mutexes use scoped lock owners; no manual lock/unlock pair crosses throwing code.
- [ ] Handles, threads, transactions, registrations, and temporary files have move-safe, non-throwing cleanup owners.
- [ ] Catch blocks translate/recover but are not the primary cleanup mechanism.
- [ ] RAII protects cleanup and transaction guards prevent partial state from being committed.
- [ ] Tests prove exception-to-error mapping and non-escape behavior.
- [ ] Failure-injection tests prove release, unlock, rollback, and cleanup occur exactly once.
- [ ] Existing result failures are propagated without losing specificity.

## Coordination with other skills

- Use `cmake-cpp-build` for target wiring and build verification.
- Use `cpp-unit-testing` or `cpp-mock-testing` for GoogleTest/GMock implementation details.
- Use Qt/OpenCV/AOI skills for their domain-specific lifecycle and correctness rules.
- This skill remains authoritative for exception containment and error propagation across all those domains.
