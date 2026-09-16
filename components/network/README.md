# szx::network 初版（HTTP 服务端）

本版仅发布 HTTP/1.1 服务端 `HttpServer`，不发布 HTTP 客户端、TCP/UDP、WebSocket 或共享 `EventLoopGroup`。公共 API 位于 `SZX::NETWORK::inline v1`；默认写 `SZX::NETWORK::HttpServer`，也可显式写 `SZX::NETWORK::v1::HttpServer`。这是新增组件，没有旧版 ABI 需要迁移。使用方需要与 DLL 保持相同编译器、STL 和运行时配置。

## 构建与链接

独立构建默认开启；作为子项目默认关闭。使用 `-DSZX_COMPONENTS_BUILD_NETWORK=ON` 启用，链接 `szx::network`。组件是共享库，PUBLIC 依赖只有 `szx::common`，PRIVATE 链接已有 bundled `hv_static`，公共头文件与安装导出不依赖 libhv。`SZX_NETWORK_ENABLE_HTTP_CLIENT` 仍默认为 OFF，该后端选项不代表本版提供 HttpClient 门面。

私有 `HttpTransport` 复用 libhv 的 TCP 服务、事件循环及已有 C HTTP 解析器，直接增量解析请求，并在接收报文体前检查长度。适配逻辑全部位于 `components/network`，不修改或在构建时修补 libhv 源码，也不新增解析器依赖。支持请求分片和 HEAD/GET 流水线，C 解析回调内的 C++ 异常统一拦截。

```cmake
find_package(SZXComponents 0.1 CONFIG REQUIRED COMPONENTS Network)
target_link_libraries(my_service PRIVATE szx::network)
```

在 MSVC x64 开发者终端中，使用仓库现有构建目录：

```powershell
cmake -S . -B build -DSZX_COMPONENTS_BUILD_NETWORK=ON
cmake --build build --target test_modules_network -j 8
ctest --test-dir build -C RelWithDebInfo -R '^test_modules_network$' --output-on-failure
```

## 最小示例

```cpp
#include "SzxNetwork/HttpServer.h"
#include <iostream>

int main()
{
    using namespace SZX::NETWORK;
    HttpServerOptions options;
    options.listen = {"127.0.0.1", 8080};
    auto created = HttpServer::Create(options);
    if (!created.ok) return created.error;
    auto server = std::move(created.value);

    auto registered = server->get("/hello/{name}", [](const HttpServerRequest &request)
    {
        HttpServerResponse response;
        response.body = "Hello, " + request.pathParams.at("name");
        response.headers["Content-Type"] = "text/plain; charset=utf-8";
        return response;
    });
    if (!registered.ok) return registered.error;
    auto started = server->start();
    if (!started.ok) return started.error;
    std::cin.get(); // 主线程等待退出；不要放到路由回调内。
    auto stopped = server->stop();
    return stopped.ok ? 0 : stopped.error;
}
```

## 接口约定

- 路由支持 GET/HEAD/POST/PUT/PATCH/DELETE/OPTIONS/TRACE，方法名必须大写。路径参数用完整段 `{name}`，通配符只允许末尾 `/*`。同方法同路径形状重复注册返回 `RouteConflict`，其他语法错误返回 `InvalidArgument`。
- 路由按各段的字面量、参数、通配符顺序选择；同等具体程度按注册顺序。HEAD 优先匹配 HEAD，再回退 GET，保留响应长度而不发送响应体。路径存在但方法不匹配返回 405。
- `path` 保留百分号编码；`pathParams` 解码一次，独立于大小写敏感的 `query`。`OPTIONS *` 保留星号目标并进入未命中处理，不映射为根路径。HTTP 头名按 ASCII 大小写不敏感比较；同名重复头保留最后一个值，不支持多条 Set-Cookie。chunked 尾部字段不暴露为请求头，不能覆盖认证或报文定界字段。
- `use()` 为同步洋葱中间件，先注册的在最外层。`next` 只能在当前调用中同步使用，最多调用一次，不得保存给异步任务。中间件可短路，可在 `next` 返回后加工响应；显式路由、405、静态文件和未命中响应都经过中间件。请求体超限、解析错误和异常兜底响应不保证经过完整中间件链。
- `attributes` 和 `userContext` 初始为空，仅供应用中间件与 handler 传值，不与客户端头部混用。本库不内置认证、授权、JSON 格式、请求 ID、业务健康接口或自动 SPA 回退。
- 空监听地址默认 `127.0.0.1`。端口 0 由系统分配，启动后通过 `port()` 获取。默认 4 个 IO 线程、1 MiB 请求体上限、60 秒 keepalive；线程数范围 1..64。
- `maxBodyBytes` 同时限制 Content-Length 与 chunked 数据，超过上限即发送 413 并停止读取，不等待剩余数据。超限检查覆盖未注册路径；解析器无法表示的长度或非法报文定界返回 400，头部超限返回 431。支持 `Expect: 100-continue`，其值按 ASCII 大小写不敏感匹配，其他期望值返回 417。
- handler 与 middleware 抛异常时返回空 500 并通知 `onError(Internal)`；错误观察器自身抛异常会被拦截。非法响应状态或头字段映射为 `ProtocolError`/500；Content-Length、Transfer-Encoding、Connection 由适配层管理。
- 公共错误用固定数值 `NetworkError` 和 `COMMON::Result<T>`，诊断字符串目前为空，不透传第三方异常、请求体、凭据或文件系统路径。同步 API 失败通过返回值报告。`use/onNotFound/onError` 相比设计草案改为返回 Result，以明确报告非法参数和运行中配置失败。

## 线程与生命周期

创建、注册和生命周期操作由调用者串行调用；配置只允许在停止状态修改。`running()/port()` 可并发读取。`ioThreadCount` 包含一个接收连接的线程及其余工作线程；设为 1 时由同一线程接收连接并处理请求。不同连接的回调可并发，应用捕获的共享状态需要自行同步。回调内不要执行同步网络调用、数据库查询、大文件操作或等待其他线程。初版回调同步返回，尚无异步响应接口。

`stop()` 幂等，关闭连接并等待 IO 线程退出，返回后不再调用应用回调；已进入的 handler 完成后才能退出，但不保证尚未发送的响应交付成功。停止后可重新启动，保留路由及中间件。在任何本库回调内调用 `stop()` 返回 `Internal`，避免自等待和跨服务相互等待。析构必须在回调外执行，且应用捕获对象必须活到 `stop()` 返回；违反析构前置条件会终止进程。

## 静态目录

```cpp
auto mounted = server->mountStatic("/ui", "./web", "index.html");
if (!mounted.ok) return mounted.error;
```

显式路由优先，然后选择最长静态前缀；只服务 GET/HEAD。目录请求读取 `index.html`，不列目录。可选 fallback 仅在该前缀下文件不存在时使用，`/api/...` 不受 `/ui` 的 SPA 回退影响。中间件可统一保护静态资源。

路径解码一次后拒绝 `..`、盘符/NTFS 流、反斜杠和符号链接逃逸，回退文件也必须位于挂载目录中。目录内容不能由不可信进程在服务期间替换（规范化与打开之间不提供操作系统级抗竞态保证）。文件使用 common 的限长读接口，当前最多 16 MiB，超出返回 413；初版静态文件读取是同步的，适合小型 UI 资源，未提供 Range、缓存协商或大文件流式发送。

## TLS 与后续范围

默认 `SZX_NETWORK_ENABLE_TLS=OFF`，此时 `tls.enabled=true` 在 Create 返回 `TlsUnavailable`。打开该构建选项时复用 `szx_add_libhv()` 的 OpenSSL 配置，透传 PEM 证书、私钥、CA 与 peer 校验选项，不管理证书轮换。HTTP 服务端每个实例拥有独立 IO 线程；后续增加其他协议时再设计共享循环与异步响应，不提前发布占位接口。

## 初版验证记录

在 Windows x64 / MSVC 19.44 / RelWithDebInfo / TLS OFF 下，18 个网络用例全部通过，覆盖真实回环连接、路由参数隔离、二进制 body、中间件顺序和短路、提前 413、超大 chunk 长度与数值溢出拒绝、请求行/头/体分片、大小写混合的 100-continue、HEAD/GET 流水线、报文定界与尾部字段隔离、目标路径解析、异常隔离、重启、端口冲突、并发、停止等待与静态目录约束。已有 `test_rbac_auth`、`test_rbac_infra`、`test_rbac_database` 3 组测试通过，`szx_admin_service` 构建通过。

安装树独立消费测试位于 `tests/network/package`，验证 `find_package(... COMPONENTS Network)`、默认/显式 v1 类型和启动/停止。导出依赖只有 `szx::common`；安装目录没有 `hv/` 头文件，DLL 不依赖 libhv DLL，导出符号为网络 v1 API。仓库格式化和差异空白检查通过。TLS ON、真实证书握手及非 Windows 平台尚未验证。

```powershell
cmake --install build --config RelWithDebInfo --prefix "$PWD/build/network-install"
cmake -S tests/network/package -B build/network-consumer -G Ninja `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$PWD/build/network-install"
cmake --build build/network-consumer
ctest --test-dir build/network-consumer --output-on-failure
```
