---
name: cpp-native-performance
description: Profile and optimize native C++20, Qt 6, QML, OpenCV, and AOI/PCB inspection workloads. Use when investigating slow detection, low FPS or throughput, latency spikes, CPU or memory pressure, excessive image copies or allocations, thread contention, Qt event-loop blocking, or when designing reliable performance benchmarks and verifying optimizations.
---

# C++ Native Performance

Diagnose from measurements before changing code. Preserve correctness and deterministic AOI results.

## Workflow

1. Define the workload and metric:
   - AOI pipeline: images/second, board latency, p50/p95/p99 stage latency
   - UI: frame time, dropped frames, input latency, main-thread stalls
   - memory: peak working set, allocation rate, retained buffers
2. Reproduce with a fixed input set, warm-up, build type, thread count, image size, and hardware description.
3. Use a release-like build with symbols (`RelWithDebInfo`). Do not draw conclusions from Debug timings.
4. Instrument pipeline stages with `std::chrono::steady_clock`, `QElapsedTimer`, or existing tracing. Keep I/O, image decode, inference, post-processing, and rendering separate.
5. Select the narrowest tool that can confirm the suspected limit:
   - CPU sampling: Visual Studio Performance Profiler or Windows Performance Recorder/Analyzer
   - allocation/memory: Visual Studio Memory Usage, heap snapshots, or sanitizer builds where available
   - microbenchmarks: Google Benchmark for isolated pure C++ or OpenCV kernels
   - QML/rendering: use `qt-qml-profiler`
   - correctness/races: ThreadSanitizer on supported toolchains; otherwise focused synchronization review and stress tests
6. Change one bottleneck at a time and repeat the identical measurement.

## C++ and Qt Checks

- Look for accidental container copies, copy-on-write detach, temporary conversions, virtual dispatch in hot inner loops, and repeated parsing or regex construction.
- Separate CPU-bound work from blocking I/O. Never run long AOI work on the Qt GUI thread.
- Prefer bounded worker queues and explicit backpressure. Avoid unbounded `QtConcurrent` fan-out and oversubscription.
- Check mutex wait time and lock scope before changing pool sizes. Keep signal delivery and QObject thread affinity correct.
- Treat timer granularity, logging, debug visualization, and synchronous disk writes as possible measurement distortion.

## OpenCV and AOI Checks

- Confirm input type, channel count, continuity, ROI coordinates, and backend before comparing implementations.
- Find repeated `clone`, `copyTo`, color conversion, resize, split/merge, and allocation inside per-frame loops.
- Prefer safe ROI views and reusable destination buffers. Account for `cv::Mat` aliasing and lifetime before removing copies.
- Measure algorithmic reductions first: smaller ROI, earlier rejection, fewer passes, appropriate resolution, and fused operations.
- Verify whether OpenCV threading competes with application-level threading. Benchmark controlled thread counts instead of stacking parallel layers blindly.
- Keep debug overlays and image serialization outside production timing.
- After optimization, compare detection results on the same corpus, including boundary and defect cases; speedups that change tolerances or classifications are regressions unless explicitly accepted.

## Benchmark Integrity

- Report sample count, warm-up, median and tail latency; averages alone hide AOI stalls.
- Use sufficiently long runs and multiple repetitions. Record variance and outliers.
- Prevent dead-code elimination in microbenchmarks and keep setup/allocation outside the measured loop unless setup is the target.
- Compare the same compiler, flags, OpenCV/Qt build, CPU affinity/power state, and input corpus.
- Distinguish throughput gains from single-item latency gains; batching and parallelism may improve one while worsening the other.

## Output

Report:

- measured symptom and baseline
- evidence identifying the limiting stage/resource
- smallest recommended change
- correctness and concurrency risks
- exact verification command/workload and expected metric movement

Do not claim an improvement without before/after measurements from equivalent runs.
