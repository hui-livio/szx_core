# szx::network（HTTP / TCP / WebSocket）

发布 HTTP/1.1 服务端 `HttpServer`、同步/异步客户端 `HttpClient`，以及 hv 事件驱动的 TCP 薄封装 `TcpServer` / `TcpClient`。同时提供 `WebSocketServer` / `WebSocketClient`；不发布 UDP 或共享 `EventLoopGroup`。公共 API 位于 `SZX::NETWORK::inline v1`；默认写 `SZX::NETWORK::HttpServer`，也可显式写 `SZX::NETWORK::v1::HttpServer`。使用方需要与 DLL 保持相同编译器、STL 和运行时配置。

组件 1.1.0 在 v1 中兼容新增行为注入、连接限制与独立文件 handler 接口，已有公共结构体布局和函数签名保持不变，SOVERSION 仍为 1。

组件 1.2.0 在 v1 中兼容新增 `useBeforeRouting()`：调用方可注入路由匹配前的请求改写或短路逻辑。已有 `use()` 继续在路由选定之后执行，仍可读取 `pathParams`。网络库默认保留原始路径；是否合并连续斜杠由应用决定。

组件 1.3.0 在 v1 中兼容新增 `HttpClient`、`HttpClientResponse`、请求选项及 `http` 快捷函数；SOVERSION 仍为 1。已有服务端签名和 DTO 布局不变，旧错误码不重编号，追加 ResourceExhausted=19、HttpClientUnavailable=20。

## 构建与链接

独立构建默认开启；作为子项目默认关闭。使用 `-DSZX_COMPONENTS_BUILD_NETWORK=ON` 启用，链接 `szx::network`。组件是共享库，PUBLIC 依赖只有 `szx::common`，PRIVATE 链接已有 bundled `hv_static`，公共头文件与安装导出不依赖 libhv。`SZX_NETWORK_ENABLE_HTTP_CLIENT` 默认 ON；旧构建目录保留缓存值，需要显式开启。关闭时 HTTP 客户端调用返回 HttpClientUnavailable；WebSocket 仍可使用。

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

## WebSocket 服务端与客户端

组件 1.5.0 在 `inline v1` 中新增 WebSocket 接口，SOVERSION 保持 1，不改变已发布的 HTTP/TCP 接口。使用 hv TCP 模板管理连接与事件循环，使用 `WebSocketChannel` 发送分帧。私有 `WebSocketProtocol` 接入 hv 自带的 C HTTP/WebSocket 解析器，在帧头阶段限制消息大小，并分别保存控制帧与数据分片；不修改 hv 源码，不新增解析器依赖或任务线程池。每端点一个 hv IO 线程。

| 类型 | 职责 |
| --- | --- |
| `WebSocketServer` | start/stop、port/running、getChannelById、connectionNum、foreachChannel、broadcast |
| `WebSocketHandle` | id/isConnected、peeraddr/path、send/close、setContext/context/getContextPtr<T> |
| `WebSocketClient` | start/stop、isConnected、send/close、setContext/context；不暴露连接句柄 |
| `WebSocketServerCallbacks` | 纯虚 onConnection(handle)、onMessage(handle, bytes, type)、onError(handle, error) |
| `WebSocketClientCallbacks` | 独立纯虚 onConnection(bool)、onMessage(bytes, type)、onError(error) |
| `WebSocketChannelVisitor` | 纯虚 onChannel(handle)，同步遍历连接快照 |

最小 echo 示例：

```cpp
#include <SzxNetwork/WebSocketServer.h>
using namespace SZX::NETWORK;

class Echo final : public WebSocketServerCallbacks {
public:
    void onConnection(const WebSocketHandle &connection) override {
        // connection.isConnected() 区分升级成功和连接断开。
    }
    void onMessage(const WebSocketHandle &connection, std::string_view bytes,
                   WebSocketMessageType type) override {
        auto sent = connection.send(std::string(bytes), type);
        if (!sent.ok) (void)connection.close();
    }
    void onError(const WebSocketHandle &, NetworkError error) override {
        // 在业务日志中记录错误码。
    }
};
auto created = WebSocketServer::Create({.listen={"127.0.0.1",9001}}, std::make_shared<Echo>());
if (!created.ok) return;
auto server = std::move(created.value);
auto started = server->start();
if (!started.ok) return;
// 保留 server 至业务结束，在回调外 stop()。
```

客户端回调直接使用客户端对象：

```cpp
#include <SzxNetwork/WebSocketClient.h>
class ClientEvents final : public WebSocketClientCallbacks {
public:
    WebSocketClient *client = nullptr;
    void onConnection(bool ready) override {
        if (ready) {
            auto sent = client->send("hello", WebSocketMessageType::Text);
            if (!sent.ok) (void)client->close();
        }
    }
    void onMessage(std::string_view bytes, WebSocketMessageType type) override {
        // 消息是完整消息，view 仅在当前回调期间有效。
    }
    void onError(NetworkError error) override {}
};
auto events = std::make_shared<ClientEvents>();
auto created = WebSocketClient::Create({.url="ws://127.0.0.1:9001/echo"}, events);
if (!created.ok) return;
auto client = std::move(created.value);
events->client = client.get(); // start 前赋值，非拥有指针。
auto started = client->start();
if (!started.ok) return;
// start 成功仅表示启动连接；onConnection(true) 表示 WebSocket 升级成功。
```

- `send` 默认为 Text，也可选 Binary，支持空消息和内含 NUL 的二进制。hv 负责大消息发送分片，接收适配器通过 hv C 解析回调组装完整消息；类型由回调单独给出。文本内容应是有效 UTF-8，封装不转码。
- `maxMessageBytes` 限制完整消息，默认 1 MiB，配置上限 16 MiB。接收时在帧头回调中检查声明长度与此前分片的累计长度，超限立即报告 MessageTooLarge 并断开，无须等待载荷或按声明长度预分配。控制帧另设最多 125 字节缓冲，分片中穿插 ping/pong 不影响消息组装。所有解析回调均拦截 C++ 异常。本封装不宣称完整 RFC 6455 一致性。
- `maxPendingBytes` 分别限制应用发送任务队列与 hv 写缓冲（默认各 4 MiB，含保守帧头预算）。提交时满返回 SendQueueFull，IO 写缓冲超限经 onError 报告并关闭连接；两阶段总量可能接近两倍配置值。成功表示接受发送，不保证远端已收到。
- `close` 排队发送正常关闭帧后关闭传输，不等待完整双向关闭握手，不暴露自定义关闭码；停止/异常关闭不保证交付关闭帧。封装自动以相同载荷回复 pong，默认无主动心跳。当前不提供自动重连、压缩、TLS/wss、自定义子协议策略、HTTP 路由或认证。`wss://` 明确返回 TlsUnavailable。普通 HTTP 请求返回 404，不提供静态文件服务。
- 服务端校验 GET、HTTP/1.1、Host、Upgrade/Connection、版本 13 和解码为 16 字节的密钥，拒绝重复关键字段及非空握手报文体；无效升级返回 400，且不触发连接成功事件。所有路径均可升级，`handle.path()` 返回握手路径。客户端校验 101、Upgrade/Connection 和 Sec-WebSocket-Accept，拒绝未请求的扩展或子协议。客户端可带普通自定义 headers，握手及消息定界字段由库管理，不允许覆盖。客户端不跟随重定向；连接超时只覆盖 TCP 建连，DNS 和 HTTP 升级不包含在内。如需访问控制，应在受控入口部署。
- 两端持有各自 `shared_ptr` 回调对象至析构。事件在 IO 线程调用，异常转 Internal 并关闭连接，onError 异常被拦截。回调可 send/close/context，不能阻塞、start/stop 或析构端点。生命周期由调用方串行控制；析构不能与公共调用并发。
- 上下文使用共享所有权，在断开通知后释放连接持有的引用；业务仍持有 shared_ptr 时继续存活。句柄可复制，不延长服务端生命周期；旧句柄不会指向重启后的新连接。`foreachChannel` 在调用线程锁外访问快照，访问器异常返回失败 Result。broadcast 委托各连接发送，首次失败后不再提交，已提交的发送不回滚。

## TCP 服务端与客户端

组件 1.4.0 在 v1 中兼容新增 `TcpServer`、`TcpClient`、`TcpFraming` 和事件回调，SOVERSION 保持 1，现有 HTTP API/布局/错误码不变。TCP 不受 `SZX_NETWORK_ENABLE_HTTP_CLIENT` 开关影响。

这是 hv 的薄封装：每个端点只使用 hv 的一个 IO 线程，不另建线程池、不复制拆包器。`start()` / `stop()` 管理生命周期；TCP 连接和收包由回调驱动，不使用 HTTP 请求式 future。

| 接口 | 行为 |
| --- | --- |
| `TcpServer::Create(options, callbacks)` | 创建监听器配置，默认回环地址，端口 0 支持动态分配 |
| `TcpServer::start/stop/running/port` | 启停、查询状态和实际监听端口；stop 后可重启 |
| `TcpServer::getChannelById(id)` / `connectionNum()` | 查找连接对象、查询连接数；旧连接 ID 不会用于重启后的新连接 |
| `TcpServer::broadcast(data)` / `forEach(visitor)` | 向当前连接广播，或遍历连接句柄快照；visitor 可调用 send/disconnect |
| `TcpHandle::send/write`、`disconnect/close`、`setContext/context` | 单连接发送、关闭、上下文操作，由连接对象负责 |
| `TcpClient::Create(options, callbacks)` | 指定 remote、连接超时和分帧方式 |
| `TcpClient::start/stop/connected` | start 发起异步连接；连接成功通过 onConnection 通知 |
| `TcpClient::send(data)` / `disconnect()` | 向当前连接发送或断开；不自动重连 |
| `TcpClient::setContext/context` | 为当前连接绑定/读取 `shared_ptr<void>` 上下文 |
| `TcpServerCallbacks::onConnection(handle)` | 服务端连接建立/断开，通过 handle.isConnected() 区分 |
| `TcpClientCallbacks::onConnection(connected)` | 客户端连接建立/断开，无连接句柄 |
| `onMessage(handle, bytes)` / `onMessage(bytes)` | 服务端/客户端收包，Raw 为任意字节块 |
| `onError(handle, error)` / `onError(error)` | 服务端/客户端错误，使用 NetworkError |

分帧直接映射到 hv 的 `setUnpack`：Raw、定长、1..8 字节分隔符、长度字段。长度字段支持 1..3 字节大/小端整数，帧长度为 `bodyOffset + 字段值`，接收保留帧头/分隔符。当前 hv 的 4 字节/varint 长度计算存在溢出边界，封装不开放这些配置，不修改 hv 源码。发送不自动添加帧头或分隔符。

`maxFrameBytes` 限制接收帧；`maxPendingBytes` 限制每连接尚未写入系统的发送字节，包含排队任务和 hv 写缓冲。超额发送返回 SendQueueFull。send 成功仅表示任务被接收，不保证远端已收到；关闭可能丢弃未发送数据。

回调中的 `string_view` 仅在本次调用期间有效。可以在回调内 send/disconnect，但不可阻塞、start/stop 或析构端点。回调异常会报告 Internal 并关闭连接，onError 的异常被拦截。生命周期操作须串行，send/disconnect 可并发；stop 返回后不会再调用业务回调，主动停止不保证逐连接通知。TCP 当前不提供 TLS、自动重连或半关闭。

TCP 回调全部采用纯虚接口：服务端 `TcpServerCallbacks`、客户端 `TcpClientCallbacks`、连接遍历 `TcpChannelVisitor`。业务必须实现所有事件方法，不关心的事件可显式实现为空。`Create` 接收 `shared_ptr` 并持有回调对象至端点销毁；省略回调表示不接收事件。回调对象不应强引用端点，避免循环所有权。两个端点使用各自独立的回调接口和对象。

服务端连接回调接收 `TcpHandle`；客户端只有一条连接，回调无需句柄：`onConnection(bool)`、`onMessage(string_view)`、`onError(NetworkError)`。发送、断开和上下文操作直接通过 `TcpClient` 完成。

```cpp
#include <SzxNetwork/TcpServer.h>
#include <SzxNetwork/TcpClient.h>
using namespace SZX::NETWORK;

struct Session { std::string name; };
class EchoEvents final : public TcpServerCallbacks {
public:
    void onConnection(const TcpHandle &connection) override {
        if (connection.isConnected()) {
            auto result = connection.newContextPtr<Session>(Session{"guest"});
            if (!result.ok) (void)connection.close();
        }
    }
    void onMessage(const TcpHandle &connection, std::string_view frame) override {
        auto session = connection.getContextPtr<Session>();
        if (!session.ok || !session.value) return;
        auto sent = connection.write(std::string(frame));
        if (!sent.ok) (void)connection.close();
    }
    void onError(const TcpHandle &, NetworkError error) override {
        // 在业务日志中记录 error。
    }
};

// 在应用函数中创建并持有 server：
auto created = TcpServer::Create(
    {.listen = {"127.0.0.1", 9000}, .framing = {.mode = TcpFrameMode::Delimiter}},
    std::make_shared<EchoEvents>());
if (!created.ok) return;
auto server = std::move(created.value);
auto started = server->start();
if (!started.ok) return;
// 应用运行期间保留 server；退出时在回调外调用 stop()。
```

客户端示例：回调对象中的非拥有指针在 `start()` 前赋值，端点析构会先停止并等待回调退出。

```cpp
class ClientEvents final : public TcpClientCallbacks {
public:
    TcpClient *client = nullptr; // 不拥有端点。
    void onConnection(bool connected) override {
        if (connected) {
            auto sent = client->send("hello\n");
            if (!sent.ok) (void)client->disconnect();
        }
    }
    void onMessage(std::string_view frame) override {
        // 处理响应；需要保存时复制 frame。
    }
    void onError(NetworkError error) override {
        // 在业务日志中记录 error。
    }
};
auto events = std::make_shared<ClientEvents>();
auto created = TcpClient::Create(
    {.remote = {"127.0.0.1", 9000}, .framing = {.mode = TcpFrameMode::Delimiter}}, events);
if (!created.ok) return;
auto client = std::move(created.value);
events->client = client.get();
auto started = client->start();
if (!started.ok) return;
// 应用运行期间保留 client；退出时在回调外调用 stop()。
```

按 ID 操作连接，先查找再交给连接对象：

```cpp
auto found = server->getChannelById(id);
if (!found.ok) return;
auto &connection = found.value;
auto sent = connection.write("hello\n");
if (!sent.ok) (void)connection.close();
```

遍历与广播：

```cpp
class PingVisitor final : public TcpChannelVisitor {
public:
    void onChannel(const TcpHandle &connection) override {
        auto sent = connection.write("ping\n");
        if (!sent.ok) (void)connection.close();
    }
};
PingVisitor visitor;
auto visited = server->foreachChannel(visitor);
auto sent = server->broadcast("notice\n");
// 检查 visited / sent 的 Result。
```

| libhv 常用接口 | 本封装 |
| --- | --- |
| `channel->isConnected()` | `handle.isConnected()`，连接状态快照 |
| `channel->write(data)` / `close()` | `handle.write(data)` / `close()`，返回 Result；发送为排队，关闭为异步关闭 |
| `channel->peeraddr()` | `handle.peeraddr()`，返回 `Result<Endpoint>`（host/port 分离） |
| `newContextPtr<T>()` / `getContextPtr<T>()` | 句柄同名模板，返回 `Result<shared_ptr<T>>`；new 支持构造参数 |
| `setContextPtr/contextPtr/deleteContextPtr` | 句柄同名共享上下文接口，返回 Result |
| `server.getChannelById(id)` / `connectionNum()` | 同名接口，返回 Result；连接数排除关闭中的连接，停止时为 0 |
| `server.foreachChannel(fn)` / `broadcast(data)` | `foreachChannel(TcpChannelVisitor&)` / `broadcast(data)`；遍历回调在锁外执行 |
| `client.send(data)` / `isConnected()` | 直接操作 `TcpClient`；客户端不公开连接句柄 |

`TcpServer` 管理监听与连接集合，不提供按 ID 发送、关闭或操作上下文的接口。`TcpHandle` 负责单连接操作，保留 `send/disconnect/setContext/context` 及 hv 风格名称。`broadcast` 是连接集合上的批量操作，通过遍历并委托每个 `TcpHandle::send` 完成发送。上下文采用共享所有权，不提供 hv 的裸指针 `newContext/deleteContext`。句柄不延长服务端生命周期，断开通知期间仍可读取上下文，通知结束后或端点停止/销毁后操作返回 Closed。客户端上下文通过 `client.setContext/context` 操作，仅在有当前连接时有效。

`forEach/foreachChannel` 同步在调用线程执行，访问器仅借用至调用返回，遍历时不持有连接表锁，允许 send/disconnect/context 操作；不允许在 visitor 内启停或析构端点。遍历使用快照，新连接不加入本轮，已断开的句柄操作返回 Closed。visitor 异常转换成失败 Result 并停止遍历。`broadcast` 对调用时未关闭的连接取快照，遍历期间连接仍可能断开；首次提交失败即返回错误，先前已接受的发送不回滚；空连接集合返回成功。

关闭请求生效后，尚未开始分发的消息（包括本次读取中的剩余帧）不再进入业务回调；已经执行中的回调不会被中断。异常关闭也遵循此规则。

上下文默认为空，通过 `shared_ptr<void>` 共享所有权；设置空指针清除上下文。断开通知结束后释放连接持有的上下文引用，无需等待客户端 stop；外部仍持有 shared_ptr 时对象继续存活。上下文槽位读写有锁保护，对象内容的并发读写由业务同步，析构器不得抛异常。类型转换需与设置时的类型一致。

创建与配置通过 `Create(options, callbacks)`，分帧由 options 映射到 hv `setUnpack`。不开放运行中直接修改回调、底层 fd/event loop 或 hv 对象。当前未封装自动重连、TLS、写完成回调及多 IO 线程配置。

## HTTP 客户端

接口接近 Python requests：`http::get/post/put/patch/del/head/options` 和 `HttpClient` 对应成员方法。DELETE 使用 C++ 合法名称 `del`。通用入口为 `request(method, url, options)`，方法名使用大写。JSON 类型复用 `SZX::COMMON::Json`。

```cpp
#include "SzxNetwork/HttpClient.h"
using namespace SZX::NETWORK;

auto reply = http::get("http://127.0.0.1:8080/users",
                      {.params = {{"page", "1"}, {"search", "Alice Smith"}}});
if (!reply.ok) return reply.error;          // DNS、连接、超时等传输失败
if (!reply.value.ok()) return 1;            // HTTP 4xx/5xx
auto document = reply.value.json();         // Result<Json>，显式 JSON 解析

auto posted = http::post("http://127.0.0.1:8080/users",
                        {.json = SZX::COMMON::Json{{"name", "Alice"}}});
auto form = http::post("http://127.0.0.1:8080/form",
                      {.form = HttpParameters{{"name", "Alice Smith"}}});
auto binary = http::put("http://127.0.0.1:8080/file", {.data = std::string{"a\0b", 3}});
```

异步使用 future，不在 IO 线程回调用户函数，也不使用 `std::async` 的每请求线程。复用 common 线程池，以 workerCount 限制并发，以 maxPendingRequests 限制运行与排队任务总数。外层 Result 报告提交失败，future 内层 Result 报告传输结果，避免队列已满时创建伪任务。

```cpp
auto created = HttpClient::Create({.workerCount = 4, .maxPendingRequests = 64});
if (!created.ok) return created.error;
auto client = std::move(created.value);
auto submitted = client->getAsync("http://127.0.0.1:8080/users");
if (!submitted.ok) return submitted.error; // 例如 SendQueueFull

// 此处可执行其他工作；UI 线程不要直接阻塞 get()。
auto result = submitted.value.get();       // HttpClientResult；请求异常已转换为错误码
if (!result.ok) return result.error;
auto status = result.value.statusCode;
auto text = result.value.text();
```

- `requestAsync/getAsync/postAsync/putAsync/patchAsync/delAsync/headAsync/optionsAsync` 均返回 `Result<HttpClientFuture>`；不同请求可以并发提交。future 析构不等待，客户端析构等待已接受任务执行完毕；析构不能与提交或同步调用并发。当前不提供取消、完成回调或 coroutine awaiter。
- URL 必须为绝对 `http://` 或 `https://`，使用 ASCII/百分号编码，不接受 URL 内凭据。params/form 按 UTF-8 字节编码，保留重复键和顺序；params 追加到现有查询串。fragment 不发送。data/json/form 最多指定一个。
- 响应保留 `statusCode/body/headers/url/setCookies`。HTTP 4xx/5xx 不转换为传输错误；`response.ok()` 为 200..399。`text()` 返回原始字节，不自动转码；`json()` 解析失败返回 ProtocolError。头名大小写不敏感；Set-Cookie 单独保存后端规范化后的多值，不自动管理 Cookie jar。
- 默认连接期限 5 秒、发送/接收期限 30 秒；均非零。后台排队及系统 DNS 解析不计入这些期限；连接期限受 libhv 上限约束。超时不自动重试。
- 默认响应体上限 16 MiB，超限返回 MessageTooLarge；以流式回调限制正文累积。libhv 解析器仍可能额外预留最多 16 MiB 接收缓冲。原始正文不自动 gzip/br 解压。
- 不自动跳转、不自动重试、不复用连接、不读取代理环境变量。3xx 的 Location 由调用方决定如何处理；认证等敏感头不会自动转发至其他地址。Host/Connection/Content-Length/Transfer-Encoding/Expect/Upgrade 由封装管理，禁止注入。
- `SZX_NETWORK_ENABLE_TLS=ON` 时支持 HTTPS，默认同时验证证书链和目标主机名/IP。可用 caFile 指定 PEM 信任库；否则使用 OpenSSL 默认信任库（不等同于 Windows 证书库）。verifyPeer=false 显式关闭验证。TLS OFF 返回 TlsUnavailable。
- 传输及解析失败使用固定 NetworkError，诊断字符串为空；不返回第三方异常文本。异步提交资源不足返回 ResourceExhausted，运行与排队总数到达上限返回 SendQueueFull。

TLS 回归通过临时 CA、证书和回环 HTTPS 服务验证，要求 Python cryptography：

```powershell
cmake -S . -B build -DSZX_NETWORK_ENABLE_HTTP_CLIENT=ON -DSZX_NETWORK_ENABLE_TLS=ON
cmake --build build --target test_modules_network
python -B scripts/test_network_tls.py --test build/test/RelWithDebInfo/test_modules_network.exe
```

## 服务端最小示例

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
- `onRequestError()` 可注入上述异常兜底以及早期解析/请求体超限的响应内容。回调获得可能不完整的请求、HTTP 状态和 `NetworkError`，由调用方决定内容格式；网络库保留原始状态和连接定界。回调抛异常或输出非法头时退回空错误响应，不递归调用。该接口不内置 JSON、request ID 或应用错误码，且不替代 `onError()` 观察器。
- `setMaxConnections()` 设置非零的全服务连接上限，包含空闲 keepalive 连接；多 IO 线程共用同一个计数。默认保持 `ioThreadCount * 1024`，超限连接在处理请求前关闭。
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

需要自行决定目录首页或回退条件时，使用 `HttpServer::CreateStaticHandler(directory, homePage)` 创建可复用的文件 handler，再注入 `onNotFound()` 或路由。它复用上述路径安全和大小检查，缺失文件返回 404，不自动执行 SPA 回退。业务层可依据请求和结果决定后续行为。

RBAC admin 使用这些扩展点：`useBeforeRouting()` 注入路径归一化，`use()` 注入 request ID 与访问日志，`onRequestError()` 注入 JSON 错误，`onNotFound()` 决定 API 路径排除和无扩展名路径的 SPA 回退。所有 RBAC 策略位于 `tools/szx_admin`；网络库不依赖 admin、JSON 或 logger。

## TLS 与后续范围

默认 `SZX_NETWORK_ENABLE_TLS=OFF`，此时 `tls.enabled=true` 在 Create 返回 `TlsUnavailable`。打开该构建选项时复用 `szx_add_libhv()` 的 OpenSSL 配置，透传 PEM 证书、私钥、CA 与 peer 校验选项，不管理证书轮换。HTTP 服务端每个实例拥有独立 IO 线程；后续增加其他协议时再设计共享循环与异步响应，不提前发布占位接口。

## 初版验证记录

在 Windows x64 / MSVC 19.44 / RelWithDebInfo / TLS OFF 下，21 个网络用例全部通过，覆盖真实回环连接、路由参数隔离、二进制 body、中间件顺序和短路、路由前改写、提前 413、超大 chunk 长度与数值溢出拒绝、请求行/头/体分片、大小写混合的 100-continue、HEAD/GET 流水线、报文定界与尾部字段隔离、目标路径解析、异常隔离、错误响应注入、连接上限、重启、端口冲突、并发、停止等待与静态目录约束。已有 `test_rbac_auth`、`test_rbac_infra`、`test_rbac_database` 3 组测试通过，其中 infra 包含 6 个 admin 真实 TCP 集成用例；`szx_admin_service` 构建通过。

安装树独立消费测试位于 `tests/network/package`，验证 `find_package(... COMPONENTS Network)`、默认/显式 v1 类型、启动/停止以及同步/异步客户端。导出依赖只有 `szx::common`；安装目录没有 `hv/` 头文件，DLL 不依赖 libhv DLL，导出符号为网络 v1 API。非 Windows 平台尚未验证。

1.3.0 新增客户端回环测试覆盖查询参数、JSON/表单/二进制正文、七种方法、HTTP 错误与重定向、非法输入、大小上限、超时、异步并发、队列满和析构等待。`scripts/test_network_http.py` 用独立原始 TCP 服务覆盖 chunked、EOF 定界、截断、非法报文和多个 Cookie；`scripts/test_network_tls.py` 使用临时证书覆盖不可信 CA、主机名/IP 不匹配、可信 CA 和异步 HTTPS。直接运行 CTest 时两个依赖外部 fixture 的用例会跳过，需单独执行上述脚本。

```powershell
cmake --install build --config RelWithDebInfo --prefix "$PWD/build/network-install"
cmake -S tests/network/package -B build/network-consumer -G Ninja `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$PWD/build/network-install"
cmake --build build/network-consumer
ctest --test-dir build/network-consumer --output-on-failure
```
