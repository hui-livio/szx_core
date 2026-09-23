# OpenSSL 3.5.8 Windows x64 SDK

本目录保存项目使用的 OpenSSL 3.5.8 LTS Windows x64 预编译制品，不包含
OpenSSL 源码。制品于 2026-09-21 从维护者本机安装目录
`C:\Program Files\OpenSSL-Win64` 提取；OpenSSL 项目本身不发布 Windows
二进制安装包，因此发布审计应以 `manifest.json` 中的文件校验和为准。

目录只保留项目构建和运行需要的文件：

```text
third_party/OpenSSL/
  bin/
    libcrypto-3-x64.dll
    libssl-3-x64.dll
  include/openssl/
  lib/VC/x64/MD/
    libcrypto.lib
    libcrypto_static.lib
    libssl.lib
    libssl_static.lib
  lib/VC/x64/MT/
    libcrypto.lib
    libcrypto_static.lib
    libssl.lib
    libssl_static.lib
  LICENSE.txt
  manifest.json
```

未保留 `MDd`、`MTd` 调试库、测试库、示例、命令行工具和非核心 provider
DLL。所有 CMake 配置都使用 Release OpenSSL 库，包括项目 Debug 配置。

## CMake 选项

- `SZX_OPENSSL_ROOT`：SDK 根目录，默认指向本目录。
- `SZX_OPENSSL_LINKAGE=STATIC|SHARED`：默认 `SHARED`。动态模式使用同一运行库
  目录下的导入库，并从 `bin` 部署两个核心 DLL；可显式指定 `STATIC` 使用静态库。
- `SZX_OPENSSL_MSVC_RUNTIME=MD|MT`：默认 `MD`。选择 `MT` 时必须同时通过
  `CMAKE_MSVC_RUNTIME_LIBRARY` 让项目使用静态 MSVC 运行库。

`cmake/szx_find_openssl.cmake` 只接受 Windows MSVC x64，并校验版本不低于
3.5.8、头文件和所选链接模式的制品。默认动态模式会将 `bin` 下两个 DLL
复制到目标输出目录，安装运行时也会包含这两个 DLL；静态模式不部署 DLL。

OpenSSL 3.0 及以后版本采用 Apache License 2.0，许可证正文见
`LICENSE.txt`。
