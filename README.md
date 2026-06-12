# szx_core

工业视觉 IO 通用 C++ 库：多厂商相机 SDK 统一抽象、PLC Modbus 控制、结构化日志。

## 包含什么

| 模块 | 说明 | 依赖 |
|---|---|---|
| `common_support` | 日志（spdlog 封装）、阻塞队列 | spdlog |
| `camera_core` | 相机生命周期、帧管道、SDK 适配器框架 | common_support, 可选: IKap / Huaray SDK |
| `plc_core` | PLC Modbus 读写抽象 | common_support, libmodbus |

已内置的相机厂商适配器：

- **FileImage**（假相机，回放目录里的图片，不需要任何 SDK，可用于联调）
- **IKap**（I-TEK OptoElectronics 帧采集卡）
- **Huaray-FG**（华睿科技帧采集卡，IMVFG SDK）
- **Huaray-USB**（华睿科技 USB/GigE 直连相机，IMV SDK）

新增厂商参考 [docs/camera_vendor_extension_guide.md](docs/camera_vendor_extension_guide.md)。

## 快速上手

最小可跑示例（不需要任何相机 SDK）：

```bash
git clone <repo> szx_core
cd szx_core
cmake -S . -B build
cmake --build build --config Release
build/examples/Release/camera_fileimage_demo.exe my_images_dir
```

集成到你的工程（add_subdirectory）：

```cmake
add_subdirectory(szx_core)
target_link_libraries(my_app PRIVATE szx_core::camera_core szx_core::plc_core)
```

或者 install 后 find_package：

```cmake
find_package(szx_core CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE szx_core::camera_core)
```

详细的集成、SDK 配置、API 用法见：

- **[docs/design.md](docs/design.md)** — 整体设计：分层、依赖方向、扩展点
- **[docs/usage.md](docs/usage.md)** — 集成与使用：CMake 选项、生命周期、SDK 配置、运行时
- **[docs/camera_core_class_design.md](docs/camera_core_class_design.md)** — camera_core 类设计
- **[docs/camera_core_usage.md](docs/camera_core_usage.md)** — camera_core 使用手册
- **[docs/camera_vendor_extension_guide.md](docs/camera_vendor_extension_guide.md)** — 新增厂商指南
- **[docs/plc_core_usage.md](docs/plc_core_usage.md)** — plc_core 使用手册

## 平台支持

主要在 Windows + MSVC 上验证。Linux 应该能编（核心代码是跨平台 C++17），但相机厂商 SDK 都只有 Windows 版本，所以 Linux 上只有 FileImage 假相机可用。

## 许可

待定。
