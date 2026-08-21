# AOI_PCB 基础组件移植说明

## 1. 目的与依据

本次移植依据 `D:\work\PCB\AOI_PCB\docs\设计\PCB检测系统双仓拆分实施方案.md`，在 `szx_core` 仓库的 `hui` 分支建立 Repo B（`szx-components`）首批基础组件边界。

AOI_PCB 当前没有名为 `base` 的物理目录；本文中的“base 模块”按拆仓方案解释为首批通用基础组件：

| 组件 | AOI_PCB 来源 | 本仓库交付目标 |
|---|---|---|
| Common | `modules/common` | `szx::common` |
| Crypto | `modules/crypto` | `szx::crypto` |
| Logger | `modules/logger` | `szx::logger` |
| Camera | `modules/camera` | `szx::camera` |
| PLC | `modules/plc` | `szx::plc` |
| Feature Manager | `modules/feature_manager` | `szx::feature_manager` |
| Feature Manager Qt | `modules/feature_manager/qt` | `szx::feature_manager_qt`（可选） |
| UI Toolkits | `modules/UIToolkits` | `UIToolkits`（可选 Qt/QML） |

`feature_manager` 已迁入并默认构建；`feature_manager_qt` 和 `UIToolkits` 依赖 Qt 6，默认关闭，按需使用 `qt-cmake` 启用。

## 2. 移植原则

1. 依赖方向保持 `pcb-detect-app -> szx-components`，组件仓不引用 PCB 业务代码。
2. 对外发布 namespaced CMake targets：`szx::common`、`szx::crypto`、`szx::logger`、`szx::camera`、`szx::plc`、`szx::feature_manager`；Qt 适配器提供 `szx::feature_manager_qt`，QML 工具包提供 `UIToolkits`。
3. 安装消费使用 `find_package(SZXComponents CONFIG REQUIRED)`，不要求消费方 `add_subdirectory()` 组件源码。
4. 公共头文件通过 `BUILD_INTERFACE`/`INSTALL_INTERFACE` 暴露；第三方 SDK 和实现目录不进入公共接口。
5. 统一使用 `szx::` canonical targets；源码集成与安装包消费使用同一组 target 名称。
6. 不复制 PCB 配置、模型、数据库或 DetectEngine 代码；不改变 camera/PLC 的业务语义。

## 3. 目标目录与接口

本阶段新增 `components/` 作为移植落点，并提供稳定安装头路径：

```text
components/
├── common/       # AOI modules/common，header-only
├── crypto/       # AOI modules/crypto
└── logger/       # AOI modules/logger
include/szx/      # 安装时的公共头文件聚合路径
```

camera 和 plc 沿用当前仓库已完成硬件抽象的实现，改为导出规范 target；其私有 SDK 适配仍由组件内部选择性编译。

## 4. CMake 与版本契约

- 包名：`SZXComponents`，当前版本沿用仓库版本 `0.1.0`。
- 导出文件：`SZXComponentsConfig.cmake`、`SZXComponentsConfigVersion.cmake`、`SZXComponentsTargets.cmake`。
- 安装目标命名空间：`szx::`。
- C++ 标准：C++20（AOI Crypto/Common 使用 `std::span`/概念等 C++20 API）。
- `spdlog` 继续支持系统包或 FetchContent；`libmodbus`、OpenCV、IKap、Huaray SDK 保持可选，缺失时不阻塞无硬件构建。

## 5. 实施步骤

1. 将 AOI `modules/common`、`modules/crypto`、`modules/logger`、`modules/feature_manager` 和 `modules/UIToolkits` 纳入 `components/`，补齐独立 CMake 子目录和公共 include 路径。
2. 在顶层建立 `szx::` canonical targets，并将现有 camera/PLC 实现挂接到相同命名空间。
3. 增加安装、导出和 package config；安装树不依赖源码绝对路径。
4. 增加安装树 consumer smoke test，验证只通过 `find_package(SZXComponents)` 链接。
5. 在无 OpenCV/SDK 的环境执行配置、构建和测试；硬件适配仅在 SDK 可用时启用。

## 6. 验收与未完成项

本阶段验收：

- `cmake -S . -B build -DSZX_COMPONENTS_BUILD_TESTS=ON` 可配置；
- `cmake --build build --config Release` 和 CTest 通过（无可选 SDK 也可）；
- `cmake --install` 生成 SZXComponents package；
- 独立 consumer 通过 `find_package(SZXComponents CONFIG REQUIRED)` 找到并链接核心 targets；启用 Qt 适配器时 package config 自动声明 Qt6 Core 依赖；
- 连续 configure/build 不修改源码树。

后续仍需在 AOI 应用仓库完成：DetectEngine 公共 API 收敛、应用改用 package 消费，以及 Qt/QML 组件的真实消费者验证。
