---
name: cmake-cpp-build
description: Use this skill whenever working on CMake-based C or C++ projects, especially when editing CMakeLists.txt, adding libraries or executables, wiring tests, configuring presets, linking Qt/OpenCV/GoogleTest, fixing build errors, or building and testing the AOI_PCB repository on Windows.
---

# CMake C++ Build

Use this skill for CMake build-system work in C and C++ repositories.

## Workflow

1. Inspect the existing CMake structure before editing:
   - top-level `CMakeLists.txt`
   - `cmake/` helper modules
   - `CMakePresets.json` or build scripts
   - nearby module-level `CMakeLists.txt`
2. Preserve the project's existing target style. Prefer local conventions over generic examples.
3. Model dependencies with targets, not global variables:
   - prefer `target_link_libraries`
   - prefer `target_include_directories`
   - prefer `target_compile_features`
   - avoid broad `include_directories`, `link_directories`, and global compile flags unless already established
4. Keep target names stable. Avoid renaming libraries, executables, or exported targets unless the user asked for it.
5. For third-party packages, prefer the package's imported targets:
   - Qt: `Qt6::Core`, `Qt6::Quick`, `Qt6::Widgets`, etc.
   - OpenCV: `${OpenCV_LIBS}` if that is the local convention, otherwise imported targets when available
   - GoogleTest: `GTest::gtest`, `GTest::gmock`, `GTest::gtest_main`
6. For tests, wire them through the repository's existing test helper if one exists. Otherwise use `enable_testing()`, `add_executable`, `target_link_libraries`, and `add_test`.
7. After editing, run the narrowest practical configure/build/test command available in the repo.

## AOI_PCB Build Contract

Treat `scripts/build.py`, `CMakePresets.json`, `CMakeUserPresets.example.json`,
`scripts/README.md`, and the current-build sections of `docs/开发指南.md` as the
repository sources of truth. Re-read them when changing build behavior; do not copy
commands from archived reviews or implementation plans without checking the current
files.

### Environment and presets

- Build the current repository on Windows x64 with Visual Studio 2022/MSVC x64,
  CMake 3.23 or newer, Ninja, Python 3, and Qt 6.8 or newer. Keep the MSVC, Qt,
  OpenCV, CUDA, and TensorRT ABI/configuration compatible.
- Prefer the repository entry point `py -3 scripts/build.py`. It uses Ninja
  Multi-Config, the shared `build/ninja` build tree, and `RelWithDebInfo` by default.
- Let the script select `local` when ignored `CMakeUserPresets.json` exists;
  otherwise let it select the tracked `windows-ninja` fallback. The fallback starts
  an MSVC x64 developer environment and discovers Visual Studio, Ninja, and Qt.
- Create machine-specific overrides by copying `CMakeUserPresets.example.json` to
  ignored `CMakeUserPresets.json` and retaining the `local` preset name. Never add
  local SDK paths to tracked `CMakePresets.json`.
- Use the matching build/test preset suffix for the selected configuration:
  `debug`, `release`, `relwithdebinfo`, or `minsizerel`.

### Routine commands

Run these from the repository root:

```powershell
# Incremental configure-if-needed and RelWithDebInfo build
py -3 scripts/build.py

# Narrow application build with bounded parallelism
py -3 scripts/build.py --config RelWithDebInfo --target PCBDetect -j 8

# Build and run the matching CTest preset
py -3 scripts/build.py --test

# Reconfigure from a fresh preset cache, or force a normal configure
py -3 scripts/build.py --clean
py -3 scripts/build.py --configure

# Reuse a known-valid build tree without configuring
py -3 scripts/build.py --skip-configure --target PCBDetect -j 8
```

Use `--clean` deliberately: in preset mode it passes `cmake --fresh`; it does not
delete the entire multi-config build tree. The script automatically configures on a
first build, after a preset file becomes newer than the cache, or when `--clean`,
`--configure`, `--no-tests`, or extra CMake arguments are supplied.

Pass cache overrides only after `--`:

```powershell
py -3 scripts/build.py -- -DSZX_BUILD_UT=ON
py -3 scripts/build.py --no-tests --target PCBDetect
py -3 scripts/build.py -- -DTENSORRT_ROOT=D:/sdk/TensorRT-10.4
```

Do not combine `--no-tests` with `--test`. Do not combine `--skip-configure` with
options that require configuration. Reserve `--legacy` and a custom `--build-dir`
for automation that still requires the old single-config Ninja flow.

### Direct CMake and CTest

Prefer `build.py` because it owns environment discovery. When an explicit preset
command is required and `CMakeUserPresets.json` supplies `local`, use:

```powershell
cmake --preset local
cmake --build --preset local-relwithdebinfo --target PCBDetect --parallel 8
ctest --preset local-relwithdebinfo
```

Without a local preset, run direct fallback commands only from a Visual Studio x64
developer environment where Ninja and Qt are discoverable:

```powershell
cmake --preset windows-ninja
cmake --build --preset windows-ninja-relwithdebinfo --target PCBDetect --parallel 8
ctest --preset windows-ninja-relwithdebinfo
```

Do not mix configurations in one command sequence. For multi-config builds, expect
executables under `build/out/bin/<BuildConfig>`, libraries under
`build/out/lib/<BuildConfig>`, and PDB files under
`build/out/pdb/<BuildConfig>`. Do not assume a repository-root `out/` directory.

### Verification selection

- After a local target or dependency change, build the narrowest affected target and
  run the relevant CTest preset or label.
- Keep the default `SZX_CTEST_REGISTRATION=binary` for routine test throughput. To
  select one GoogleTest case through CTest, reconfigure with
  `-- -DSZX_CTEST_REGISTRATION=case`, then use `ctest --preset <preset> -R <pattern>
  --output-on-failure`. Restore `binary` after diagnosis when the shared build tree
  is reused.
- Use `py -3 scripts/test_asset_inventory.py --run ...` for certification or release
  evidence because it treats skipped/not-run tests as failures. Do not substitute a
  historical test count for a current run.
- Treat the dual-repository install-tree commands in `docs/开发指南.md` as the target
  architecture, not the routine current single-repository build. Use them only for
  package/install-tree work after verifying that the required CMake options and
  install targets exist in the current checkout.
- Treat the Inno Setup packaging path separately: the current installer expects the
  documented single-config output layout, so do not feed the normal Ninja
  Multi-Config development build to it without following `scripts/README.md`.

## Qt Notes

- Use `qt_add_executable`, `qt_add_qml_module`, and Qt resource helpers if the project already uses Qt 6's modern CMake API.
- Keep QML module URIs, resource prefixes, and plugin targets consistent with existing code.

## OpenCV Notes

- Check whether the project uses `find_package(OpenCV REQUIRED)` globally or per-module.
- Keep image-processing modules linked only where OpenCV is actually needed.
- Avoid leaking OpenCV include paths and compile definitions to unrelated targets.

## Output

When reporting changes, include:

- which targets changed
- which dependencies were added or moved
- which configure/build/test command was run
- any remaining build assumptions
