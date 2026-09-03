# OpenSSL 预编译静态 SDK

本目录保存项目基于 OpenSSL 官方源码自行编译的 Windows x64 静态 OpenSSL SDK 制品。

- 版本：OpenSSL 4.0.2
- 架构：Windows x64
- 链接方式：静态 `libcrypto.lib` / `libssl.lib`
- Provider：当前仅保留 `default`；`legacy.dll` 仅在使用旧算法时需要
- CMake 配置：`lib/cmake/OpenSSL/OpenSSLConfig.cmake`
- 官方项目：https://github.com/openssl/openssl
- 官方许可证页面：https://www.openssl.org/source/license.html
- 许可证：OpenSSL 3.0 及以后版本适用 Apache License 2.0
- 制品来源：项目维护者使用官方源码本地构建，不是 OpenSSL 官方发布的二进制安装包
- Configure 参数：`perl Configure VC-WIN64A no-shared --release --prefix="D:\third_party\openssl\static\x64"`

当前安装前缀为 `D:\third_party\openssl\static\x64`，随后将安装产物整理到本仓库的 `third_party/OpenSSL/WIN64_Static`。该 SDK 的源码版本/提交、编译器、运行库（`/MT`/`/MD`）、Configure 参数、完整构建命令和源码校验和应在发布制品清单中维护。升级 OpenSSL 时必须同步更新版本、源码来源、校验和、许可证和兼容性测试记录。

注意：本目录中的 `.lib`、`.dll` 和头文件是预编译制品，不代表 OpenSSL 源码；分发时应同时提供 Apache License 2.0 文本和本说明中的来源信息。
