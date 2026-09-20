# Admin 部署与使用指南

本文对应 `RBAC/szx_admin` C++ 服务和 `RBAC/szx_admin_web` Vue 3 管理界面，说明当前实现（2026-09-20）。API 字段和错误码以 [接口文档](05-RESTful-API接口文档.md) 为准。

## 1. 地址与目录

| 用途 | 默认地址或目录 |
|---|---|
| Admin 页面入口 | `http://localhost:7364/admin`，跳转到 `/admin/` |
| Swagger UI | `http://localhost:7364/api/v1`，跳转到 `/api/v1/` |
| OpenAPI 文件 | `http://localhost:7364/api/v1/openapi.json` |
| REST API | `http://localhost:7364/api/v1` |
| 健康检查 | `GET /api/v1/health` |
| 前端开发服务 | `http://localhost:5173`，代理 API 到 `127.0.0.1:7364` |
| 前端构建产物 | `RBAC/szx_admin_web/dist` |
| 默认部署位置 | `build/bin/web/admin` |
| 默认服务程序 | `build/bin/szx_admin_service.exe` |
| 默认运行配置 | `build/bin/config/config.yaml` |

以上产物路径对应当前仓库的 `build` 构建树。自定义构建目录或配置时，以实际可执行文件目录为准。`server.staticRoot: web` 是相对**进程工作目录**的路径，并非相对配置文件位置。JWT 密钥与日志的相对路径也以工作目录为准。

生产通过 HTTPS 同源提供 `/admin/` 和 `/api/v1`，代理保留这些路径。本机 HTTP 示例用于开发验证；普通 HTTP 的局域网 IP 访问可能无法使用 Web Crypto 和 Secure refresh Cookie，导致登录或会话刷新失败。

## 2. 打包与部署

使用 Node.js 22.12+ 或 24 LTS，在仓库根目录运行：

```powershell
Set-Location RBAC/szx_admin_web
npm ci
npm run build
npm run deploy
Set-Location ../..
```

`build` 先执行类型检查，再生成以 `/admin/` 为 base 的产物；`deploy` 只复制已有 dist，不负责构建。脚本默认写入 `build/bin/web/admin`，先复制资源再复制 index，并保留旧的哈希资源文件，避免已打开的页面加载旧资源失败。

在已配置的 Visual Studio 2022 x64 开发终端中，构建服务：

```powershell
cmake --build build --target szx_admin_service --parallel 6
```

CMake 在配置时发现 `dist/index.html` 后添加 `szx_admin_web_assets`，每次构建服务都会复制已有产物。若首次配置时没有 dist，可在生成 dist 后运行 `cmake -S . -B build`，或直接用 `npm run deploy`。CMake 不运行 npm，也不会自动更新过期的前端产物。构建前退出占用目标 exe 的旧服务。

自定义部署目录时，在仓库根目录运行：

```powershell
./scripts/deploy_admin_web.ps1 -StaticRoot 'D:/services/szx_admin/web'
```

这会写入 `D:/services/szx_admin/web/admin`；服务应配置 `staticRoot: D:/services/szx_admin/web`。不要把 staticRoot 设置为其 admin 子目录，否则挂载位置会多出一层。

## 3. 配置与启动

配置模板为 [config.yaml](../../RBAC/szx_admin/config/config.yaml)。部署前设置数据库连接、JWT 密钥和实际允许的 Origin；首次启动执行迁移及幂等 bootstrap。密钥需预先提供，生成方式见 API 文档“部署与凭据说明”。

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

从仓库根目录启动：

```powershell
Set-Location build/bin
./szx_admin_service.exe --config config/config.yaml
```

打开 `http://localhost:7364/admin`。其他机器使用服务所在机器的主机名或 IP；`0.0.0.0` 是监听地址。首次创建静态目录后需要重启服务，已有目录内替换构建文件无需为静态文件单独重启；升级后端 exe 仍需重启。

初始超管用户名为 `admin`，初始密码为 `superadmin`，首次登录必须修改为至少 8 个字符且含字母、数字的新密码。初始化密码是 bootstrap 特例。重复启动不会恢复默认密码，也不会覆盖现有密码。

### 在线接口文档

构建服务会自动复制 `RBAC/szx_admin/web/api-docs` 到 `build/bin/web/api-docs`，不依赖 Vue 构建或外网 CDN。自定义 staticRoot 时复制到 `<staticRoot>/api-docs`；首次创建目录后重启服务。

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

access token 有效期 15 分钟；发请求前剩余有效期不超过 5 秒或收到可刷新的 access 鉴权错误时自动刷新，同一页面并发请求合并刷新，原请求最多重试一次。refresh token 默认有效期 7 天，每次轮换但不延长会话族原到期时间。闲置时没有定时刷新。刷新失效/撤销后重新登录，429/5xx 保留会话供手动重试。access/CSRF 仅在内存，刷新浏览器页面后需要重新登录。

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
| `/admin` 返回 404 | 确认运行新版后端，实际 staticRoot/admin 下存在 index.html，首次部署后已重启 |
| 页面空白或 JS/CSS 404 | 检查 index.html 引用 `/admin/assets/`；重新 build、deploy，确认代理保留前缀 |
| `/admin/users` 刷新 404 | 确认 spaFallback 为 true，并使用支持 admin 挂载的新版后端 |
| 页面能打开但登录/刷新失败 | 检查 API 地址、Origin、HTTPS、Cookie 和 Web Crypto；以请求状态码及 request_id 定位 |
| 找不到配置或密钥 | 检查启动工作目录和配置内路径；--config 不会改变进程工作目录 |

Windows 检查默认端口占用：

```powershell
Get-NetTCPConnection -LocalPort 7364 -State Listen |
  Select-Object LocalAddress, LocalPort, OwningProcess
Get-Process -Id <上一步的OwningProcess>
```

确认进程身份后退出旧实例，再启动服务；不要同时运行两个监听同一地址和端口的实例。修改端口后同步更新开发代理和访问地址。

自动化验证命令见 [前端 README](../../RBAC/szx_admin_web/README.md)。后端静态挂载测试位于 `tests/rbac/HttpTransport_test.cc`，可构建 `test_rbac_infra` 后运行 `ctest --test-dir build -R '^test_rbac_infra$' --output-on-failure`。前端浏览器测试使用模拟 API，不能替代真实数据库、HTTPS Cookie 和生产环境验收。
