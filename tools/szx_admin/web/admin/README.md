# szx_admin_web

数自星智能用户管理系统，接入 `tools/szx_admin` 的 REST API。

## 启动

使用 Node.js 22.12+（或 24 LTS）和 npm。

```powershell
cd tools/szx_admin/web/admin
npm ci
npm run dev
```

打开 `http://localhost:5173`。开发服务器默认把 `/api/*` 转发到 `http://127.0.0.1:7364`。可复制 `.env.example` 为 `.env.local` 修改 `ADMIN_API_TARGET`；后端修改 API 前缀时同步修改 `VITE_API_BASE_URL` 和 Vite 代理前缀。

需要启动并配置 `szx_admin` 后端（数据库、JWT 密钥及管理员账号）。生产应用没有演示账号、模拟数据或内置密码。

后端 `auth.allowedOrigin` 应允许前端实际来源。后端 refresh Cookie 带 `Secure`，开发使用 `localhost`；生产必须使用 HTTPS。不要通过普通 HTTP 的局域网 IP 访问：Cookie 和 Web Crypto 可能不可用。

后端默认 `allowedOrigin: "*"` 支持 HTTP/HTTPS 来源跨域携带凭据，也可配置单一 Origin 限制来源。refresh Cookie 使用 `SameSite=None; Secure; HttpOnly`；跨站接入需 HTTPS、`withCredentials: true` 和刷新/登出的 `X-CSRF-Token`。当前前端使用相对 API 地址，同源部署或 Vite 代理无需修改。

## MVVM 分层

| 层          | 目录                               | 职责                                                 |
| ----------- | ---------------------------------- | ---------------------------------------------------- |
| View        | `src/pages/`、`src/components/`    | 页面组合、语义化表单、展示与事件绑定                 |
| ViewModel   | `src/composables/use*ViewModel.ts` | 表单校验、页面状态、权限派生、提交、错误与交互流程   |
| 共享状态    | `src/stores/auth.ts`               | Pinia 当前用户、能力和改密流程                       |
| Model / API | `src/api/types.ts`、`admin.ts`     | 类型化数据与服务端资源操作                           |
| 会话传输    | `src/api/client.ts`                | Axios、Bearer、Cookie/CSRF、单次并发刷新、稳定错误码 |
| 领域规则    | `src/domain.ts`                    | 固定角色、密码校验、摘要、日志脱敏与时间转换         |

Vue 组件不直接调用 HTTP 或实现业务操作。新增功能时将业务行为加入对应 ViewModel，展示组件通过 props/model 输入和 emits 输出。DOM 生命周期（对话框）由独立 composable 处理。

## 页面和行为

- `/login`：UID/用户名登录，原始密码 UTF-8 SHA-256 摘要提交。
- `/change-password`：超管首次强制改密；其他角色可跳过建议；成功后重新登录。前端按角色校验：操作员/技术员（engineer，界面显示工程师）至少 6 个字符，可使用纯数字或纯字母；管理员/超级管理员至少 8 个字符且包含字母和数字。
- `/users`：搜索、筛选、排序、分页、详情、创建、展示名编辑、启停、解锁、重置密码、超管角色分配。
- `/logs`：按事件、结果、用户及本地时间区间查询；事件名称支持中文或事件代码关键词（例如“登录”、`login`、`auth.login`），使用后端 `event_search`，需要更新后端。“搜索用户”同时匹配展示名、用户名和 UID，不匹配内部 UUID。列表显示用户名（UID 账号显示 UID）；搜索与分页均由后端执行。请求时间转为 UTC 秒精度，详情脱敏展示。
- `/home`：当前账号与后端返回的权限能力。操作员和工程师默认进入此页，业务操作在对应业务客户端进行。

管理员只管理操作员/工程师；超管管理四种角色。前端限制用于交互，服务端仍是权限判定方。用户更新带 `If-Match`；2209 冲突后必须重新加载并确认，不自动覆盖。

任何用户都不能给自己分配角色或禁用自己；超出管理范围的用户操作按钮不可用，后端同样拒绝。

Access/CSRF token 仅保存在 API 客户端闭包内，refresh token 由 HttpOnly Cookie 管理。没有 localStorage/sessionStorage 持久化，刷新页面后需要重新登录；当前也不做跨标签页会话同步。同一页面并发刷新合并为一次。429/5xx 保留会话并提示手动重试；失败的写请求不自动重复执行，只有明确的 access 鉴权失败会刷新后重试一次。退出仅在服务端成功返回后清除内存会话。

发请求前，access token 已过期或剩余有效期不超过 5 秒时，先调用 `POST /auth/refresh`；收到 `401 / 2200` 或 `401 / 2201` 时也会刷新并最多重试一次。刷新使用 HttpOnly Cookie 和 `X-CSRF-Token`，服务端轮换 refresh token。页面闲置时不定时刷新；默认连续空闲超过 30 分钟后，恢复操作会因 refresh 空闲过期而要求重新登录。成功刷新会重置空闲窗口，但不会延长默认 7 天的会话族绝对期限。

创建/重置返回的临时密码仅在当前弹窗展示，关闭或离开页面立即清除，不写入 Pinia 或浏览器存储。不要将它复制到日志或工单中。

重置密码也可手动填写新密码并确认，按目标用户角色校验；留空则随机生成。指定密码不回显，重置自己的密码后返回登录页；随机重置时先展示临时密码，确认记录后再退出。两种方式均使旧密码和已有会话失效，需要后端支持重置请求的 `password_hash` 字段。

创建用户可填写初始密码和确认密码（操作员/技术员至少 6 个字符，可用简单密码；管理员/超级管理员至少 8 个字符，包含字母和数字），以 SHA-256 摘要提交；留空仍自动生成临时密码。指定密码不会回显，也没有临时密码的 24 小时有效期；首次登录改密策略不变。成功、取消或离开页面清空密码。此功能需要支持创建请求 `password_hash` 字段的新版后端。

## 验证

```powershell
npm run build
npm test
npx playwright install chromium
npm run test:e2e
npm run format:check
```

本机已有 Edge 时可使用 `$env:PLAYWRIGHT_CHANNEL='msedge'` 运行浏览器测试。测试开发服务使用独立的 5183 端口。浏览器测试拦截 API 并提供契约数据，不修改真实服务；不代替真实 HTTPS Cookie、数据库及完整后端联调验收。

测试覆盖并发刷新、失效和临时错误、失败登出、摘要、角色边界、改密、临时凭据清除、版本冲突、审计脱敏与手机布局。

## 发布

服务配置、首次启动、操作说明和端口占用排查见 [Admin 部署与使用指南](../../../../docs/RBAC/12-Admin部署与使用指南.md)。

`npm run build` 生成以 `/admin/` 为资源和路由前缀的 `dist/`，开发服务器仍使用 `/`。Vue Router 自动使用构建时的 BASE_URL，接口仍走同源 `/api/v1`。

在前端目录执行 `npm run deploy`，将已构建的 `dist/` 部署到 `build/bin/web/admin/`。从 `build/bin` 启动新版 `szx_admin_service.exe`，访问 `http://<host>:<port>/admin`；服务会跳转到 `/admin/`，支持 `/admin/login`、`/admin/users`、`/admin/logs` 等页面直接打开和刷新。首次创建部署目录后需重启服务。

自定义静态目录时，在仓库根目录执行 `./scripts/deploy_admin_web.ps1 -StaticRoot <server.staticRoot的实际目录>`。始终把 dist 内容放在 staticRoot 的 `admin` 子目录，保持 `spaFallback: true`；只公开构建产物。API 和不存在的 JS/CSS 文件不会回退为 HTML。

CMake 在配置时发现 dist 后，会通过 `szx_admin_web_assets` 目标在每次构建服务时复制产物；前端仍由 npm 独立构建，首次生成 dist 后可重新配置 CMake 或直接运行部署脚本。生产使用 HTTPS 同源提供页面与 API。局域网普通 HTTP 下 Web Crypto 不可用时，需要 HTTPS 才能完成密码摘要和登录。

接口依据：[RESTful API 接口文档](../../../../docs/RBAC/05-RESTful-API接口文档.md)。
