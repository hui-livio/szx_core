# Admin 部署与使用指南

本文对应 `tools/szx_admin` C++ 服务和 `tools/szx_admin/web/admin` Vue 3 管理界面，说明当前实现（2026-09-22）。Admin 已初步完成服务、认证、用户管理、审计/行为事件接口和管理页面；当前仍需完成业务 Client 接入、真实 HTTPS 联调及生产运维验收。API 字段和错误码以 [接口文档](05-RESTful-API接口文档.md) 为准。

## 1. 地址与目录

| 用途 | 默认地址或目录 |
|---|---|
| Admin 页面入口 | `http://localhost:7364/admin`，跳转到 `/admin/` |
| Swagger UI | `http://localhost:7364/api/v1`，跳转到 `/api/v1/` |
| OpenAPI 文件 | `http://localhost:7364/api/v1/openapi.json` |
| REST API | `http://localhost:7364/api/v1` |
| 健康检查 | `GET /api/v1/health` |
| 前端开发服务 | `http://localhost:5173`，代理 API 到 `127.0.0.1:7364` |
| 前端构建产物 | `tools/szx_admin/web/admin/dist` |
| 默认部署位置 | `build/bin/web/admin` |
| 默认服务程序 | `build/bin/szx_admin_service.exe` |
| 默认运行配置 | `build/bin/config/config.yaml` |

以上产物路径对应当前仓库的 `build` 构建树。自定义构建目录或配置时，以实际可执行文件目录为准。`server.staticRoot: web` 是相对**进程工作目录**的路径，并非相对配置文件位置。JWT 密钥与日志的相对路径也以工作目录为准。

生产通过 HTTPS 同源提供 `/admin/` 和 `/api/v1`，代理保留这些路径。本机 HTTP 示例用于开发验证；普通 HTTP 的局域网 IP 访问可能无法使用 Web Crypto 和 Secure refresh Cookie，导致登录或会话刷新失败。

## 2. 打包与部署

使用 Node.js 22.12+ 或 24 LTS，在仓库根目录运行：

```powershell
Set-Location tools/szx_admin/web/admin
npm ci
Set-Location ../../../..
cmake --build build --target szx_admin_service --parallel 6
```

CMake 配置时定位 npm。每次构建 `szx_admin_service` 都会自动执行 `npm run build`：先进行类型检查，再生成以 `/admin/` 为 base 的最新 `dist`，随后将产物复制到可执行文件目录下的 `web/admin`；该流程不依赖后端是否需要重新链接。构建前退出占用目标 exe 的旧服务。

自定义部署目录时，在仓库根目录运行：

```powershell
./scripts/deploy_admin_web.ps1 -StaticRoot 'D:/services/szx_admin/web'
```

这会将已构建的 `dist` 写入 `D:/services/szx_admin/web/admin`；服务应配置 `staticRoot: D:/services/szx_admin/web`。不要把 staticRoot 设置为其 admin 子目录，否则挂载位置会多出一层。

## 3. 配置与启动

配置模板为 [config.yaml](../../tools/szx_admin/config/config.yaml)。仓库内 `config/keys` 的 PEM 仅供本地开发调试；生产部署前必须替换全部 JWT/TLS 密钥，并设置数据库连接和实际允许的 Origin。首次启动执行迁移及幂等 bootstrap。生产密钥需由受控来源预先提供，生成方式见 API 文档“部署与凭据说明”。

静态托管相关配置：

```yaml
server:
  host: 0.0.0.0
  port: 7364
  apiPrefix: /api/v1
  staticRoot: web
  staticHomePage: index.html
  spaFallback: true
```

这是配置片段，保留完整配置中的 logging、auth、database 等部分。`staticHomePage` 用于根静态站点，`/admin/` 挂载固定读取 admin 子目录的 `index.html`。

`auth.allowedOrigin` 默认是 `"*"`，允许 HTTP/HTTPS 来源跨域调用；服务回显具体 Origin 并返回 `Access-Control-Allow-Credentials: true`，支持 OPTIONS 预检。也可配置单一来源，例如 `https://admin.example.com`，此时精确匹配协议、主机、端口，不含路径或末尾 `/`。拒绝 `Origin: null` 和非法来源。refresh Cookie 为 `HttpOnly; Secure; SameSite=None`，跨站调用必须使用 HTTPS，并在前端设置 `credentials: 'include'`（Axios 为 `withCredentials: true`）；refresh/logout 同时携带最新 `X-CSRF-Token`。浏览器的第三方 Cookie 限制仍可能影响跨站刷新。

`auth.refreshIdleTimeoutSeconds` 配置 refresh token 空闲期限，默认 1800 秒，取值为 900 秒到 `auth.refreshLifetimeSeconds`；`auth.refreshLifetimeSeconds` 默认 604800 秒，是会话族从首次登录开始计算的绝对期限。修改配置后需重启服务。

服务已统一提供 CSP、禁止 iframe 嵌入和 MIME 嗅探等安全头，无需 CDN。反向代理需保留这些响应头；CSP 仅允许页面连接同源 API。详细保护范围与尚需处理的凭据风险见 [安全与运维基线](07-安全与运维基线.md#6-web-攻击防护与当前边界2026-09-20)。

从仓库根目录启动：

```powershell
Set-Location build/bin
./szx_admin_service.exe --config config/config.yaml
```

打开 `http://localhost:7364/admin`。`7364` 当前是明文 HTTP 端口，不能直接使用 `https://主机:7364/admin`；`0.0.0.0` 是监听地址。远端完整使用登录和刷新需部署下述 HTTPS 入口。首次创建静态目录后需要重启服务，已有目录内替换构建文件无需为静态文件单独重启；升级后端 exe 仍需重启。

初始超管用户名为 `admin`，初始密码为 `superadmin`，首次登录必须修改为至少 8 个字符且含字母、数字的新密码。初始化密码是 bootstrap 特例。重复启动不会恢复默认密码，也不会覆盖现有密码。

### 远端 HTTPS 入口

当前 admin 没有接入底层网络库的 TLS 选项，因此更改 URL 为 `https://`、开放端口或允许 CORS 都不会开启 HTTPS。Firefox 的“SSL 接收到一个超出最大准许长度的记录”（`SSL_ERROR_RX_RECORD_TOO_LONG`）以及 curl 的 TLS 握手错误，常见原因是把 HTTPS 请求发到了 HTTP 端口。

连接方式：浏览器 `https://admin.example.com/admin/` → Nginx 的 `443` TLS 端口 → `http://127.0.0.1:7364`。页面、静态资源、Swagger 和 API 都通过同一个 HTTPS 入口。

1. 使用与访问域名匹配且客户端信任的证书；直接通过 IP 访问时，证书 SAN 需包含该 IP。内网 CA 的根证书需安装到访问端的信任库。
2. 使用 [Nginx 配置示例](deployment/nginx-admin.conf.example)，替换域名和证书路径，将其包含在 Nginx 的 `http {}` 配置中。示例假设代理和 admin 同机；上游保持 `http://`，不要改成 `https://127.0.0.1:7364`。
3. 运行 `nginx -t` 检查配置，再启动 Nginx 或对已有实例运行 `nginx -s reload`。放通代理的 `443` 端口。同机代理部署时，admin 可监听 `127.0.0.1:7364`。
4. 访问 `https://实际域名/admin/`。保留 `allowedOrigin: "*"` 可跨域访问；如配置单一 Origin，应使用浏览器访问的 HTTPS 来源。

同机 IPv4 上游须明确写为 `proxy_pass http://127.0.0.1:7364;`。Windows 中 `localhost` 若同时解析为 `::1` 和 `127.0.0.1`，Nginx 可能尝试 IPv6 上游，而服务监听 `0.0.0.0` 或 WSL 的 IPv4 转发并不提供该上游。以 Nginx 错误日志中的实际 upstream 地址为准。

可用 `curl.exe -I http://127.0.0.1:7364/admin/` 验证上游，再用 `curl.exe -I https://实际域名/admin/` 验证证书和代理。普通远端 HTTP 可以用于确认端口连通，但其 Web Crypto 与 Secure Cookie 限制使它不能替代完整 HTTPS 部署。浏览器强制 HTTPS 模式也可能把手工输入的 HTTP 地址升级成 HTTPS。

Nginx 示例不修改 CORS、CSP 或 Cookie 响应头。当前 admin 使用连接对端地址进行 IP 限流，不信任 `X-Forwarded-For`；代理后的客户端会共享代理 IP 的限流预算，应按部署规模配置 `auth.attemptsPerMinute`。

### 在线接口文档

构建服务会自动复制 `tools/szx_admin/web/api-docs` 到 `build/bin/web/api-docs`，不依赖 Vue 构建或外网 CDN。自定义 staticRoot 时复制到 `<staticRoot>/api-docs`；首次创建目录后重启服务。

访问 `/api/v1`，展开接口查看参数、示例与响应，点击 Try it out → Execute 发起真实请求。登录提交 SHA-256 摘要，随后将 access_token 填入 Authorize 的 bearerAuth（不用添加 Bearer 前缀）；刷新/登出所需 Cookie 由浏览器自动携带，手动填写最新 X-CSRF-Token。授权信息仅保留在当前页面，不会自动刷新。可下载 OpenAPI 文件导入接口工具。

## 4. 日常操作

| 功能 | 当前行为 |
|---|---|
| 用户登录 | 使用用户名或 UID；展示名不作为登录标识 |
| 创建用户 | 用户名与 UID 二选一；可指定并确认初始密码，留空随机生成 |
| 重置密码 | 可指定并确认新密码，留空随机生成；旧密码和已有会话失效 |
| 密码规则 | 操作员/技术员（engineer，页面显示工程师）至少 6 位，可纯数字或纯字母；管理员/超管至少 8 位且含字母和数字，特殊字符不强制 |
| 临时凭据 | 随机生成的 20 字符密码仅展示一次，默认 24 小时有效；指定密码不回显，也没有临时有效期 |
| 首次改密 | 超管强制改密；其他角色提示改密并允许跳过，不延长临时凭据有效期 |
| 管理范围 | 管理员只管理操作员/工程师；超管管理四类角色。任何用户都不能给自己分配角色或禁用自己，不能操作高于自身或超出管理范围的用户 |
| 并发修改 | 写请求携带 If-Match；版本冲突后重新加载并确认，不自动覆盖 |
| 审计查询 | 事件支持中文名称和事件代码关键词；用户搜索匹配展示名、用户名、UID，不提供内部 UUID 搜索框 |

密码强度由前端在提交摘要前检查，服务端不能从 SHA-256 摘要还原并校验原始长度和字符类别。自助改密不能留空；随机生成只适用于创建和重置。

审计页面展示中文事件名称及原始代码、操作者用户名（UID 账号显示 UID）。例如事件输入 `登录`、`login` 或 `auth.login`，用户输入展示名的一部分；条件在服务端取交集，分页和 total 使用相同条件。无关联操作者显示“未知用户”。重置筛选会清空条件并回到第一页。

access token 有效期 15 分钟；发请求前剩余有效期不超过 5 秒或收到可刷新的 access 鉴权错误时自动刷新，同一页面并发请求合并刷新，原请求最多重试一次。refresh token 默认空闲期限为 30 分钟，会话族绝对期限为 7 天。每次成功刷新都会轮换 token 并重置空闲窗口，但不会延长绝对到期时间。页面闲置时没有定时刷新；连续空闲超过 30 分钟后恢复操作必须重新登录。刷新失效/撤销后重新登录，429/5xx 保留会话供手动重试。access/CSRF 仅在内存，刷新浏览器页面后需要重新登录。

## 5. 验证与排障

在另一终端检查实际服务：

```powershell
curl.exe -I http://localhost:7364/admin
curl.exe -I http://localhost:7364/admin/login
curl.exe http://localhost:7364/api/v1/health
```

预期 `/admin` 返回 308，Location 为 `/admin/`；`/admin/login` 返回 200 HTML，健康检查返回 200 JSON。页面中的 `/admin/assets/...` 返回 JS/CSS；缺失的 JS/CSS 和未知 API 应返回 404，路径穿越应拒绝。

| 现象 | 排查方法 |
|---|---|
| CMake 提示不存在 szx_admin_service 目标 | 使用已修正的 CMakeLists：add_dependencies 必须位于 add_executable 之后；重新构建触发配置 |
| HTTP startup failed (2217)，伴随 bind 错误 | 先检查端口占用、监听地址；该提示本身不能确定是文件缺失 |
| SSL 接收到超出最大准许长度的记录 | 检查是否对明文 7364 端口使用了 HTTPS；HTTPS 应访问有证书的代理端口，代理上游使用 HTTP |
| `/admin` 返回 404 | 确认运行新版后端，实际 staticRoot/admin 下存在 index.html，首次部署后已重启 |
| 页面空白或 JS/CSS 404 | 检查 index.html 引用 `/admin/assets/`；重新 build、deploy，确认代理保留前缀 |
| `/admin/users` 刷新 404 | 确认 spaFallback 为 true，并使用支持 admin 挂载的新版后端 |
| 页面能打开但登录/刷新失败 | 检查 API 地址、Origin、HTTPS、Cookie 和 Web Crypto；以请求状态码及 request_id 定位 |
| 查询接口偶发约 2 秒、最终 200，Server-Timing 很小 | 检查 Nginx error.log 是否同期出现 `[::1]:7364` / `connect() failed (10061)`；IPv4 上游改为 `127.0.0.1:7364`，通过 `nginx -t` 后 `nginx -s reload`，复测 HTTPS 入口 |
| 找不到配置或密钥 | 检查启动工作目录和配置内路径；--config 不会改变进程工作目录 |

Windows 检查默认端口占用：

```powershell
Get-NetTCPConnection -LocalPort 7364 -State Listen |
  Select-Object LocalAddress, LocalPort, OwningProcess
Get-Process -Id <上一步的OwningProcess>
```

确认进程身份后退出旧实例，再启动服务；不要同时运行两个监听同一地址和端口的实例。修改端口后同步更新开发代理和访问地址。

自动化验证命令见 [前端 README](../../tools/szx_admin/web/admin/README.md)。后端静态挂载测试位于 `tests/rbac/HttpTransport_test.cc`，可构建 `test_rbac_infra` 后运行 `ctest --test-dir build -R '^test_rbac_infra$' --output-on-failure`。前端浏览器测试使用模拟 API，不能替代真实数据库、HTTPS Cookie 和生产环境验收。


## 查询接口偶发等待排查

用户列表、审计日志与权限查询共用 HTTP 传输、同步访问日志和数据库连接池。浏览器“等待服务器响应”包括服务端处理前的等待及响应传输；现有访问日志的 `duration` 在写入该条日志之前计算，因此不能用它单独排除日志输出阻塞。

新版服务为 API 响应补充 `Server-Timing`，可在浏览器 Network → Timing/Headers 查看：

| 指标 | 覆盖范围 |
|---|---|
| `app` | HTTP 中间件开始到业务处理、响应准备完成，包含鉴权和数据库操作 |
| `access_log` | 同步访问日志输出耗时，包含控制台和文件 sink 的写入/刷新 |
| `server` | 上面两段合计；不包含进入中间件之前的排队和返回响应之后的套接字发送 |

记录慢请求的 `X-Request-ID`，对照访问日志和 `pool_acquire_slow`、`database_callback_slow`、`orm_operation_slow`。若 `app` 很慢，继续沿鉴权/数据库路径定位；若 `access_log` 很慢，检查控制台及日志文件写入；若 `server` 很快但浏览器等待很久，检查 I/O 线程排队、响应发送、代理/端口转发与 TCP。三段统计不能代替客户端和服务器的同一次请求关联。

默认服务配置使用 `logging.console: false`，诊断继续写入轮转文件，避免终端或输出管道背压阻塞 HTTP。该开关在 Release/RelWithDebInfo 生效；Debug 构建的 logger 始终输出控制台。关闭控制台不排除文件 sink 卡顿，仍需对照 `access_log`。

`workerThreads: 0` 当前实际使用一个 HTTP I/O 线程，业务处理和日志输出均同步执行。其他请求或日志 sink 卡顿可能阻塞后续请求，此时后续请求的业务耗时仍可能很短。仅调大线程数不能证明或解决根因。

可用仓库采样工具对比客户端、新连接与长连接：

```powershell
py -3 scripts/profile_admin_http.py http://127.0.0.1:7364/api/v1/health --count 100
py -3 scripts/profile_admin_http.py http://127.0.0.1:7364/api/v1/health --count 100 --new-connection
py -3 scripts/profile_admin_http.py "http://127.0.0.1:7364/api/v1/admin/users?page=1&page_size=20&sort=-created_at" --token-env SZX_ADMIN_PROFILE_TOKEN --count 100
```

`SZX_ADMIN_PROFILE_TOKEN` 应为现有会话的 access token。工具只发 GET，不执行登录、刷新或业务写入；不输出 token、响应正文或 URL 查询参数，慢样本输出请求 ID、响应头计时、首字节和总耗时。401/403 等响应不代表成功查询，工具会报告状态并返回非零退出码。它直连目标地址，不读取系统 HTTP 代理，因此与浏览器差异本身也是排查线索。

在 Windows 浏览器访问 WSL 服务的场景，分别在 Windows 与 WSL 运行同样的采样；再对比 Windows 的 `localhost`/`127.0.0.1` 转发地址和 WSL 实际 IPv4 地址。以 `Get-NetTCPConnection` 的拥有进程确认是否经过 `wslrelay` 或其他代理。服务需重新启动新版可执行文件，响应才会出现新增计时头；替换磁盘文件不会让正在运行的旧进程自动更新。


### IPv6 上游回退的已复现实例（2026-10-13）

Windows Nginx 配置为 `proxy_pass http://localhost:7364;`、WSL 服务经 IPv4 转发时，两个用户查询慢请求分别在 10:06:28 和 10:12:38（北京时间）的代理错误日志出现连接 `[::1]:7364` 被拒绝。10:12:38 的请求 ID 为 `c2baf73d-899a-4a89-88ad-6a2fc6c983ef`，后端计时 `app=3 ms`、`access_log=1 ms`，浏览器等待约 2.05 秒。

通过 Windows curl 对 `https://localhost/api/v1/health` 使用相同请求方式、新进程每次请求、间隔 250 ms 进行采样（本地自签证书采样使用 `-k`）：修复前 40 次中 2 次超过 1 秒，最大 2275.8 ms，慢请求同期均有 IPv6 上游连接失败。将 upstream 明确改为 `127.0.0.1:7364` 并平滑重载后，100 次采样全部返回 200，最大 244.6 ms，超过 1 秒为 0 次，新增 IPv6 上游错误为 0。该样本证明代理路径上的失败连接/回退，不代表数据库查询性能基准；业务接口仍应按请求 ID 和 Server-Timing 验证。

### MySQL / MariaDB 兼容边界

数据库连接参数仍使用原有 `database` 配置，无需新增 engine 选项。连接时只读检测 `SELECT VERSION()`：MySQL 8.0.16+（含 8.4）和 MariaDB 10.11+；不支持的旧版/未知引擎拒绝启动，不通过关闭 CHECK 等方式降低数据约束。

初始化适配在 `infra/DatabaseDialect.h` 与 ORM 私有适配层：MySQL 按原 SQL 执行；MariaDB 执行 DDL 时将 `DROP CHECK` 转为 `DROP CONSTRAINT`，`utf8mb4_0900_ai_ci` 映射到 `utf8mb4_unicode_520_ci`，`utf8mb4_0900_bin` 映射到 `utf8mb4_nopad_bin`（保留二进制、无填充比较）。映射仅应用于可信初始化 SQL，字符串常量、注释和绑定业务参数不替换。

已发布迁移 1–10 的 SQL 和 SHA-256 校验和保持不变；迁移日志记录原始目录校验和，不记录适配后 SQL。排序规则的 Unicode 权重版本不同，跨引擎的自然语言排序不承诺逐项一致；不要直接复制 MySQL 的内部数据文件给 MariaDB，也不要据此宣称任意旧 MariaDB 库可原地升级。身份与权限标识仍按业务规范校验并使用二进制比较。

数据库由服务自动创建时也按检测结果选择排序规则。已有库不做全库排序规则重写。备份/恢复及生产版本升级仍需独立验收。

机器指标验证脚本：

```sh
python scripts/test_admin_metrics.py --service build/bin/szx_admin_service
```

该脚本启动临时、仅监听 loopback 的 MySQL/MariaDB 数据目录，验证全新迁移到 12、真实指标快照/历史、角色权限、非法查询参数及已部署的 OpenAPI；`--rbac` 另验证自定义角色/权限生命周期、范围、引用保护和并发。退出后清理，绝不连接现有应用库。MySQL 需要 mysql/mysqld，MariaDB 另需 mariadb-install-db；Python 需 cryptography。具体已验证版本见本次交付说明，不以版本检测范围替代跨版本实机测试。

迁移 12 保留迁移 1–11 的原始校验值，移除 users.role 的旧固定角色 CHECK、扩为二进制比较的 VARCHAR(64)，并修复遗留 role_id 不一致。升级前备份数据库并停止旧版本写流量；不能把旧版服务回滚后直接连接已升级库作为安全回滚方案。实际部署环境的备份恢复和升级验收需独立执行。

迁移 12 支持 DDL 已提交但数据修复或日志记录尚未完成时的重启恢复：初始化适配器按当前 schema 判断是否省略已完成的旧 CHECK 删除，随后继续列修改和数据修复，不改迁移 SQL/checksum，不忽略其他数据库错误。恢复测试仅在临时库注入失败并重启验证；现场仍应先备份，不手动删除迁移日志来绕过检查。
