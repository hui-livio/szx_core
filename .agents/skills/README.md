# SZX Core 项目技能集合

本目录是当前仓库的精简项目级技能集合，合并了两类来源：

- `D:\work\PCB\AOI_PCB\.agents\skills`：仅提取适用于基础组件库的 AOI 仓库约束、错误契约和交付流程。
- `C:\Users\Administrator\.codex\skills`：通用 C++、CMake、测试、性能和文档技能。

技能按目录名加载，目录内的 `SKILL.md` 是入口；`references/`、`scripts/` 和 `evals/` 仅在对应技能需要时读取。

## 使用约定

1. 涉及本仓库 C++、CMake、相机/OpenCV 或单元测试时，优先使用对应项目技能。
2. 同名技能以本目录版本为准；本集合中的同名目录优先保留 AOI_PCB 项目版本，以确保与 PCB 工程流程一致。
3. 修改公共头文件或 ABI 时，同时参考 `api-stability-inline-namespace` 和 `cpp-api-documentation`。
4. 修改测试、CMake 或 OpenCV 代码时，分别使用 `cpp-unit-testing`、`cmake-cpp-build` 和 `opencv-cpp-aoi`。
5. 当前仓库暂不包含 Qt/QML 目标，但保留 Qt/QML 技能用于后续上层应用或工具接入；使用时不应强行引入 Qt 依赖。

## 技能分类

| 分类 | 技能 |
| --- | --- |
| AOI/仓库工作流 | `aoi-delivery-workflow`, `aoi-repository-development`, `aoi-module-error-contract` |
| C++ 工程 | `cpp-development`, `cpp-native-performance`, `api-stability-inline-namespace`, `cpp-api-documentation` |
| 构建与测试 | `cmake-cpp-build`, `cpp-unit-testing`, `cpp-mock-testing`, `cpp-fuzz-testing`, `cpp-benchmark-testing` |
| 图像与相机 | `opencv-cpp-aoi` |
| 性能与并发 | `performance-concurrency-advisor` |
| Qt/QML | `qt-project`, `qt-cpp-review`, `qt-qml`, `qt-qml-review`, `qt-qml-profiler` |
| 辅助流程 | `define-goal`, `doc-coauthoring`, `aoi-delivery-workflow` |

来源和合并结果见 [manifest.json](manifest.json)。
