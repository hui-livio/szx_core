# SZX Components

从 AOI_PCB `modules` 迁移的通用基础组件包，面向 `pcb-detect-app` 等应用仓库提供稳定的 CMake package。

## 组件 targets

| Target | 内容 |
|---|---|
| `szx::common` | C++20 header-only 基础类型、容器和序列化辅助 |
| `szx::crypto` | Base64、Hash/HMAC、Fernet、AES-GCM、RSA、PBKDF2 |
| `szx::crypto_service` | 面向业务的统一加密服务门面 |
| `szx::logger` | spdlog 日志封装 |
| `szx::network` | HTTP/1.1 服务端：路由、中间件、请求体限制和静态目录；libhv 为私有实现，见 [接入说明](components/network/README.md) |
| `szx::camera` | 相机生命周期、采集管道和可选 OpenCV/IKap 适配 |
| `szx::plc` | 可选 libmodbus PLC 控制 |
| `szx::feature_manager` | Feature 配置读取、持久化、组合 Provider 与订阅 |
| `szx::feature_manager_qt` | 可选 Qt 6 Feature/QML 适配器 |
| `szx::compression` | Core ZIP reader/writer facade; the minizip-ng/zlib-ng/Zstandard implementation is private and selectable with `SZX_COMPRESSION_BACKEND` |
| `UIToolkits` | 可选 Qt 6/QML 控件、图表和字体资源 |

## 构建、测试和安装

在已加载 MSVC 开发者环境的终端中（当前项目不使用 CMake 预设文件）：

```powershell
cmake -S . -B build/msvc -G "Ninja Multi-Config"
cmake --build build/msvc --config Release
ctest --test-dir build/msvc -C Release --output-on-failure
cmake --install build/msvc --config Release
```

RBAC admin 后端通过 `szx::network` 提供 HTTP 服务。独立构建默认启用
`SZX_COMPONENTS_BUILD_RBAC` 和 `SZX_COMPONENTS_BUILD_NETWORK`；作为子项目时两者默认关闭，
需要 admin 时同时设为 ON。仅消费基础库时可保持 RBAC 关闭，从而不引入服务和 MySQL 依赖。
admin 的 request ID、JSON 错误与 SPA 策略通过网络库回调注入，见
[RBAC 基础能力计划](docs/RBAC/11-基础能力开发计划.md)。

许可证请求、签发和验证的端到端测试（同时覆盖错误产品、篡改文件、损坏请求和非法参数）：

```powershell
py -3 scripts/test_license_flow.py --build-dir build/msvc
```

脚本默认查找 `build/bin` 或多配置构建的 `build/bin/RelWithDebInfo`、
`build/bin/Release`（以及对应的 `out/bin` 路径）。也可以使用 `--user-demo` 和
`--issuer` 显式指定两个可执行文件。

启用 Qt 组件时使用 Qt 6.8 或更高版本的 `qt-cmake`，并增加：

```powershell
-DSZX_COMPONENTS_BUILD_FEATURE_MANAGER_QT=ON
-DSZX_COMPONENTS_BUILD_UI_TOOLKITS=ON
-DSZX_COMPONENTS_BUILD_UI_TOOLKITS_APP=ON
```

`UIToolkitsApp` 是放在 `tools/UIToolkitsApp/` 下的 Qt/QML 演示工具，不属于可复用库。启用后，Windows 多配置构建产物位于 `build/<目录>/bin/Release/UIToolkitsApp.exe`；`.qmlls.ini` 等 Qt 工具文件由构建生成并被 Git 忽略。

安装后，消费方只需：

```cmake
find_package(SZXComponents 0.1 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE szx::common szx::crypto
    szx::crypto_service szx::logger szx::feature_manager
    szx::camera szx::plc szx::compression)
```

`szx::compression` is the exported entity target.  Product-source integration
also provides the compatibility target `SZX::compression` for existing PCB
targets; neither target exposes minizip-ng, zlib-ng, or zstd to application
code.  Set `SZX_COMPRESSION_BACKEND=none` to build the facade without a ZIP
backend; its operations then return `UnsupportedBackend`.

Qt/QML 应用按需链接 `szx::feature_manager_qt`，并通过 `add_subdirectory()` 源码集成或安装后的 `SZXComponents` package 使用。

OpenCV、IKap 和 libmodbus 均按本机 SDK/库是否存在选择性启用；无硬件环境仍可构建 common、crypto、logger 和 camera 核心。

仓库文档入口见 [docs/文档索引.md](docs/文档索引.md)，统一构建、基础组件和工具使用见 [工具与基础组件构建使用指南](docs/使用/工具与基础组件构建使用指南.md)，第三方源码依赖及其分发边界见 [docs/规范/第三方库清单.md](docs/规范/第三方库清单.md)。
