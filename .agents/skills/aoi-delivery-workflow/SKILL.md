---
name: aoi-delivery-workflow
description: Drive an AOI feature or change through the complete repository delivery workflow from intake and requirement clarification to design, implementation, review, testing, performance validation, documentation, release acceptance, and archival. Use when starting a feature, fixing a production-relevant defect, preparing a release, defining a definition of ready or done, checking whether work is truly complete, or coordinating handoffs among product, project, C++/Qt/QML, algorithm, test, and field roles.
---

# AOI Delivery Workflow

Use the smallest workflow that matches risk, but never skip evidence needed for production safety or traceability.

## Lifecycle

### 1. Intake and readiness

- Identify the user problem, business value, scope, priority, owner, dependencies, and target release.
- Define observable acceptance criteria and exclusions.
- Resolve decisions that would materially change architecture, data, hardware behavior, or release promises.

Definition of Ready: scope is bounded; status and priority are explicit; dependencies and sample/test data are available or planned; acceptance evidence is named.

### 2. Impact and design

- Trace affected UI/QML, Controller/Adapter, PCB VM, DetectEngine, algorithm nodes, configuration, database, device integration, CMake, tests, and documentation.
- Preserve layer boundaries: QML does not access DB/devices/OpenCV; DetectEngine does not depend on Qt; cross-repository access uses versioned packages.
- Record alternatives and tradeoffs for consequential decisions.
- Review concurrency, cancellation, error semantics, traceability, and rollback.

### 3. Implementation

- Deliver a thin vertical slice before broad refactoring.
- Keep commits focused and follow `docs/规范/提交信息规范.md`.
- Update configuration mapping, validation, examples, and tests together when adding fields.
- Keep debug visualization and diagnostic logging separate from production behavior and timing.

### 4. Verification

- Run the narrowest relevant unit and integration tests, then broader CTest/build checks proportional to impact.
- Add fixed-corpus regression checks for algorithm behavior.
- Validate GUI-thread safety, worker lifetime, cancellation, stale-result prevention, and model contracts for Qt/QML changes.
- Measure baseline and after-state for performance claims using equivalent release-like runs.
- Verify failure, empty input, resource error, product switch, archive timing, and recovery paths.

### 5. Review and acceptance

- Use the relevant C++/Qt/QML/OpenCV/performance review skills.
- Map each acceptance criterion to concrete evidence.
- Check visible UI actions end to end; disable, hide, or formally remove incomplete actions from release scope.
- Decide release level using `docs/产品/发布验收清单.md`; do not equate merged code with release approval.

### 6. Documentation and closure

- Update product, design, operation, test, performance, and planning documents whose facts changed.
- Update `docs/文档索引.md` when adding a maintained document.
- Move completed temporary plans/reviews into the appropriate `docs/归档/` location.
- Record remaining risks, deferred scope, follow-up owner, and verification gaps.

Definition of Done: accepted behavior exists end to end; relevant tests pass; performance/correctness evidence exists where required; documents match implementation; release gates and deferred risks have explicit decisions.

## Handoff Output

For every phase transition provide:

- completed artifacts and evidence
- unresolved decisions and risks
- next owner and expected output
- commands, datasets, environment, and assumptions needed to reproduce verification
