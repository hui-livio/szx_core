# Common Capability Index

Read the actual header and tests before use; this index routes discovery and records major caveats, not a frozen API specification.

| Concern | Header / primary types | Typical use | Important boundary notes |
|---|---|---|---|
| Wall-clock time | `TimeHelper.hpp`: `TimeHelper` | Millisecond epoch timestamps, UTC/local formatting, strict timestamp and ISO-8601 parsing | Use UTC for persistence/interchange. Do not measure elapsed work with system time. |
| Elapsed time | `TimeHelper.hpp`: `Stopwatch`, `ScopeTimer` | Performance and stage `elapsed_ms`, resettable or scope-based timing | Uses a monotonic clock. Keep logging callbacks non-throwing in effect. |
| Files | `FileHelper.hpp`: `FileHelper` | Existence, size, extension checks, text/byte reads and writes, append, copy, remove one file | Basic helpers return `bool`/`optional`; they do not classify permission/disk-full failures or guarantee atomic publication. |
| Directories and paths | `FolderHelper.hpp`: `FolderHelper` | Create directories/parents, list files/directories, absolute and weak-canonical paths | Enumeration skips permission-denied entries and can return partial/empty data. No recursive-delete or confinement contract. |
| Strings | `StringHelper.hpp`: `StringHelper` | ASCII case conversion, integer parsing, trim, split, join | Check encoding and locale needs; this is not a UI translation layer. |
| JSON | `JsonHelper.hpp`: `JsonHelper`; `Serializer.hpp`: `Json` | Safe parse, required objects, deep merge, strict/coerced booleans, finite numbers | Keep unstructured JSON out of stable interfaces when a typed DTO is known. |
| Identity | `UUID.hpp`: `Uuid` | UUID v4/v7 generation, parse, canonical/compact formatting | Prefer stable IDs over display names, array indices, or paths. |
| Results | `Result.hpp`: `Result<T>` | Internal/foundation success, code, value, diagnostic handling | Contains free-form `message`; do not expose it through code-only public boundaries or UI. |
| Cleanup | `Defer.hpp`: `Defer` | Scope rollback/cleanup when no more specific owner exists | Prefer a dedicated RAII type for handles, locks, transactions, and long-lived registrations. Cleanup must not throw. |
| Blocking queue | `BlockQueue.hpp`: `BlockingQueue<T>` | Bounded producer/consumer flow, timed pop, close, high-water mark | Pass an explicit nonzero capacity for production queues unless an unbounded design is explicitly justified. |
| Thread pool | `ThreadPool.hpp`: `ThreadPool`, `ThreadPoolOptions` | Named workers and task submission | Define queue capacity, shutdown, and exception containment at the task boundary. |
| Managed work queue | `WorkQueueService.hpp`: `WorkQueueService`, `WorkQueueOptions`, state/error DTOs | Submit/status/cancel/result/retention lifecycle | Map common queue errors into the owning domain; configure maximum queued and retained work explicitly. |
| Messaging | `MessageBus.hpp`: `MessageBus`, topic/subscription types | In-process typed publish/subscribe | Define topic ownership, payload bounds, subscription lifetime, callback thread, and full policy. |
| Geometry | `Point.hpp`, `Geometry.hpp` | Qt-free points, sizes, rectangles, segments, circles, saturating area/distance operations | State coordinate system, units, inclusivity, and conversion at boundaries. |
| Enum support | `MagicEnum.hpp` | Enum reflection/string conversion where already established | Do not turn enum names into persisted or UI control protocols without an explicit stable mapping. |
| CLI | `Command.hpp`: `Command` | Repository CLI composition | Keep CLI errors translated at the command boundary. |
| Startup checks | `StartupSelfCheck.hpp` | Structured startup validation | Keep user presentation separate from internal diagnostics. |
| Memory diagnostics | `MemoryTrace.hpp` | Repository memory tracing | Diagnostic-only; do not make tracing change production ownership or timing semantics. |

## Discovery examples

Before adding elapsed timing:

```text
rg -n "class Stopwatch|class ScopeTimer" modules/common/TimeHelper.hpp
rg -n "Stopwatch|ScopeTimer" apps modules detectengine test
```

Before adding filesystem code:

```text
rg -n "class FileHelper|class FolderHelper" modules/common
rg -n "FileHelper::|FolderHelper::" apps modules detectengine test
```

Before adding a queue or worker:

```text
rg -n "BlockingQueue|ThreadPool|WorkQueueService|MessageBus" modules/common test/modules/common
rg -n "BlockingQueue|ThreadPool|WorkQueueService|MessageBus" apps modules detectengine
```
