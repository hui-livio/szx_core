# 单项目 RBAC RESTful API 接口文档

## 1. 通用约定

Base URL：`/api/v1`。认证头：`Authorization: Bearer <access_token>`。全站使用 HTTPS。成功使用 200/201/202/204；分页响应 `{items,total,page,page_size}`，`page_size` 服务端限制最大值；错误响应 `{code,message,details,request_id}`。服务端不得向客户端返回 `password_hash`、token 原文或私密密钥。

并发约定固定使用 `If-Match: <version>`；所有 PATCH/PUT 和会改变用户状态/密码的 POST 都必须携带该请求头。请求体不再接受可选的 version 字段。版本不一致统一返回 HTTP 409 和 `CONFLICT_VERSION`。

使用 refresh Cookie 的 POST 请求必须同时携带 `X-CSRF-Token`；CSRF token 不等同于 access/refresh token，不能写入日志。logout 未携带 refresh Cookie 时仅清 Cookie 并返回 204，不操作服务端会话，因此不要求 CSRF。

### 当前实现范围

已实现 REST 协议基础层、健康/就绪检查及四个认证接口：`POST /auth/login`、`POST /auth/change-password`、`POST /auth/refresh`、`POST /auth/logout`。用户管理、权限和日志查询接口仍为待实现的业务契约。

- 服务路由在 `RBAC/szx_admin/src/Routes.cc` 集中注册；`main.cc` 只调用注册入口。`HttpTransport::registerApiRoute()` 接受以 `/` 开头的相对路径，自动拼接配置中的 `apiPrefix`（默认 `/api/v1`）。支持 GET、HEAD、POST、PUT、PATCH、DELETE、OPTIONS；OPTIONS 的响应由注册的处理器定义，不自动启用 CORS。
- `HttpRequest` 提供解码后的 `pathParams`、`query`，以及忽略大小写的 `header()` 查询。参数和请求头沿用网络层单值映射语义，不提供重复值列表。
- 控制器通过 `RestApi::jsonObject()` 要求 `Content-Type: application/json`（允许 UTF-8 charset）和对象类型 JSON；`allowedFields()` 校验字段白名单，必填字段和业务类型由控制器继续校验。校验失败返回 `2206` / HTTP 422。
- `pagination()` 默认 `page=1`、`page_size=20`，要求正十进制整数；`page` 上限为 4294967295，`page_size` 上限为 100，偏移量使用 64 位整数。
- `ifMatch()` 接受 `If-Match: 42` 或 `If-Match: "42"`，版本范围 1–2147483647。缺失、弱 ETag、通配符或多个版本均返回 `2206` / HTTP 422；实际版本比较和事务更新由业务层实现。
- `bearerToken()` 仅提取凭据，限制 Authorization 长度为 8192 字节，缺失或格式错误返回 `2200` / HTTP 401；改密接口进一步校验 JWT 签名、issuer/audience/有效期、用户状态和当前 authz_version，通用业务权限路由尚未实现。
- `RestApi` 统一产生成功 `{data,request_id}`、错误 `{code,message,details,request_id}` 和空 204 响应，附带 `X-Request-ID`、`Cache-Control: no-store`。错误使用稳定数字码和安全默认消息，不回传内部诊断。

`GET ${apiPrefix}/health` 返回 `{"data":{"status":"ok"},"request_id":"…"}`。`GET ${apiPrefix}/ready` 检查数据库连接，成功返回 `{"data":{"status":"ready"},"request_id":"…"}`，不可用返回 `2220` / HTTP 503。`/ready` 已从原有响应改为统一 envelope，调用方应读取 `data.status`。

路由访问默认记录 INFO 日志：`request_id=... method=GET path="/api/v1/health" status=200`。`server.accessLog` 默认及随包配置均为 true；包含未命中路由和异常响应。路径截断至 512 字节并转义，不记录 query、Authorization 或请求体。

## 2. 认证

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/auth/login` | 登录，已实现 |
| POST | `/auth/refresh` | 刷新令牌及轮换 Cookie，已实现 |
| POST | `/auth/logout` | 撤销当前 refresh 会话族，已实现 |
| POST | `/auth/change-password` | 首次及自助修改密码，已实现 |

登录请求使用统一的 `identifier`，可匹配用户的 `uid` 或 `username`。客户端对密码原始 UTF-8 字节计算 SHA-256，将 64 位小写十六进制值放入 `password_hash`，不传输明文 `password`：

```json
{"identifier":"U10086","password_hash":"<64 lowercase hexadecimal characters>"}
```

登录成功返回当前用户、15 分钟 access token、access 过期时间和当前 `authz_version`；refresh token 通过 HttpOnly、Secure、SameSite Cookie 设置，不在 JSON 中返回。失败统一返回 `AUTH_INVALID_CREDENTIALS`，不区分用户不存在、禁用、锁定或密码错误。

浏览器计算示例（密码不 trim、不转换大小写、不做 Unicode 规范化）：

```javascript
const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(password));
const password_hash = Array.from(new Uint8Array(digest), b => b.toString(16).padStart(2, "0")).join("");
await fetch("/api/v1/auth/login", {
  method: "POST",
  credentials: "include",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ identifier, password_hash })
});
```

服务端不保存或直接比较传输的 SHA-256 值，而是使用随机盐 Argon2id 再哈希，存储格式为 `sha256-v1:<Argon2id PHC>`。传输 hash 仍是登录凭据，使用 HTTPS 且不得写入日志。既有 `Argon2id(原密码)` 无法直接转换：仍使用默认密码且保留首次改密标记的初始超管在成功登录事务中自动升级；其他旧凭据保持原值，需要通过明确的密码重置迁移，不接受明文回退。该变化不修改既有建表 SQL 或 migration checksum。

当前登录响应 `data` 包含 `user`、`access_token`、`token_type: "Bearer"`、`expires_in: 900`、UTC ISO 8601 `expires_at`、`authz_version` 和 `csrf_token`。用户信息沿用用户对象字段白名单，不包含密码哈希或临时凭据过期时间。首次改密按角色区分：

| 用户状态 | user.force_password_change | user.password_change_recommended | 业务端流程 |
|---|---|---|---|
| 超管待改密 | true | false | 必须改密，不提供跳过入口 |
| 非超管待改密 | false | true | 提示改密，允许跳过进入业务 |
| 已完成改密 | false | false | 正常进入业务 |

该状态来自数据库待改密标记与当前角色，不依赖 last_login_at 是否为空。跳过是业务客户端的会话内操作，不调用改密接口、不清除数据库标记；下次登录仍可提示。当前仓库提供服务端接口和业务端接入契约，尚未实现前端页面或通用业务接口的权限守卫。

Cookie 名为 `szx_refresh`，固定 `HttpOnly; Secure`，`allowedOrigin="*"` 时使用 `SameSite=None`，指定单一 Origin 时使用 `SameSite=Strict`。Path 为 `${apiPrefix}/auth`（根前缀时为 `/auth`），默认有效期 7 天。原文由 CSPRNG 生成 32 字节，只在 Cookie 中返回；数据库保存 SHA-256 哈希和新 family ID。`csrf_token` 为 `SHA256("csrf:" + refresh_token)` 的小写十六进制，refresh/logout 以常量时间比较校验；每次 refresh 后必须使用响应中的新 CSRF token。

请求仅接受 `identifier`、`password_hash` 两个字符串字段；password_hash 必须为 64 位小写十六进制，旧 password 字段、明文、大小写不符或长度不符均返回 `2206` / 422，报文体不超过 8192 字节。identifier 去除首尾 ASCII 空白并做 Unicode NFC；username 使用不依赖 locale 的小写，UID 保持大小写，规范化结果不超过 64 个 Unicode 字符。UID/username 同时匹配不同账号时统一拒绝。未知账号也执行 Argon2id 校验以减少时序差异。临时凭据同样先由客户端计算 SHA-256；过期返回同样的 401。超管临时凭据成功登录时在事务中消费；非超管登录不消费临时凭据，允许跳过改密后在原有效期内再次登录，跳过不会延长有效期。

`auth.allowedOrigin` 默认配置为 `"*"`，允许任意 Origin 访问认证接口；也可指定单一 `http://` 或 `https://` Origin，此时不匹配的请求返回 403。无 Origin 的原生客户端允许访问。四个接口均支持 OPTIONS 预检，允许 POST 及 Content-Type、Authorization、If-Match、X-Request-ID、X-CSRF-Token 请求头。允许的来源在成功和错误响应中均回传具体 `Access-Control-Allow-Origin`、`Access-Control-Allow-Credentials: true` 和 `Vary: Origin`，不对携带凭据的请求返回通配符响应头。浏览器请求使用 `credentials: "include"`，通过 HTTPS 使用 Secure Cookie。

默认同一 socket 对端 IP 每分钟最多 30 次认证尝试（四接口共享计数桶，改密在 JWT 校验后计数，refresh/logout 在 Cookie 格式和 CSRF 校验后计数），超出返回 `2211` / 429；无 Cookie 的 logout 不计数。忽略客户端提供的 X-Forwarded-For，代理部署需在入口配置独立限流。单进程最多保留 4096 个未到期 IP 计数桶，多实例计数不共享。连续 5 次登录错误密码将 active 用户置为 locked，同时递增 version/authz_version；locked_until 默认写入当前时间加 900 秒，按账号契约到期不自动解锁。成功登录清零失败计数。阈值均可通过 auth 配置调整。

账号查询使用行锁。失败计数、锁定、最后登录时间、refresh 哈希和安全审计在同一事务中提交，写入失败不返回令牌或 Cookie。事件为 `auth.login.success`、`auth.login.failure`、`auth.account.locked` 和 `auth.login.rate_limited`，审计不包含 identifier、密码或 token。审计失败返回 `2212` / 500。

服务启动前须配置 `auth.privateKeyPath` 和 `auth.publicKeyPath`（支持 PEM/DER，路径相对进程工作目录），缺失或密钥不匹配时启动失败；不自动生成临时密钥。可用 OpenSSL 生成专用 RSA 3072 位密钥：

```sh
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3072 -out jwt-private.pem
openssl pkey -in jwt-private.pem -pubout -out jwt-public.pem
```

将文件放在静态目录之外并配置对应路径。`auth.allowedOrigin: "*"` 允许任意来源，需要限制来源时填写具体 Origin。也可复用仓库 `szx_license_issuer keygen --private-key <path> --public-key <path>` 命令生成独立密钥对，不与许可证签发共用密钥。PS256 issuer/audience 默认 `szx-admin` / `szx-client`；重启继续加载同一密钥，不使未过期 access token 因换钥失效。

`POST /auth/change-password` 仅接受 `{old_password_hash,new_password_hash}`（与登录相同的 SHA-256 编码），必须携带 `Authorization: Bearer <access_token>` 和当前用户 `If-Match`，不接受指定目标用户。仅使用 Bearer 鉴权，不读取 refresh Cookie，因此本接口不要求 X-CSRF-Token。超管首次改密和各角色自助改密均可调用。客户端校验原始密码长度/复杂度；服务端校验摘要格式、新旧不同以及旧密码匹配，不从摘要推断原始密码强度。

成功返回 200：`{"data":{"version":2,"authz_version":2,"force_password_change":false,"password_change_recommended":false,"reauthentication_required":true},"request_id":"…"}`。服务端在同一事务中更新密码、清除待改密标记及临时有效期、设置 password_changed_at、递增 version/authz_version、撤销该用户全部 refresh token 并记录 `auth.password.changed`；清除 refresh Cookie，业务端清除旧 access token 并重新登录。改密接口会拒绝旧 authz_version 的 access token；后续所有受保护接口也必须校验该版本。超管可使用刚登录时已消费的临时密码 hash 完成改密，当前 access 会话仍须有效。

无效/过期 JWT、用户禁用/锁定/不存在、旧授权版本返回 `2200` / 401；旧密码错误返回 `2203` / 401 并记录 `auth.password.change_failed`；缺失/错误 If-Match 或摘要字段返回 `2206` / 422；版本冲突返回 `2209` / 409；审计失败返回 `2212` / 500。密码、会话撤销、审计任一写入失败均回滚；不会返回成功 Cookie。

### 刷新与登出

两个接口均接受空请求体或 `Content-Type: application/json` 的空对象 `{}`；拒绝其他字段，不从请求体、query 或 Authorization 读取 refresh token，不要求 access token 或 If-Match。凭据只从 `Cookie: szx_refresh=<token>` 读取，必须为规范的 32 字节 Base64URL 无填充编码；重复同名 Cookie 返回 `2206` / 422。

`POST /auth/refresh`：携带 Cookie 和当前 `X-CSRF-Token`。成功返回 200，`data` 字段与登录一致，包含新的 access_token、csrf_token、当前用户和改密标记；新 refresh token 只通过 Set-Cookie 返回。数据库在同一事务内锁定用户及 token、检查 active 状态和 token 有效期、创建同 family 的新 token、将旧 token 标记为 rotated 并写入 replaced_by/last_used_at/revoked_at、记录 `auth.refresh.success`。刷新不延长 family 原到期时间，Cookie Max-Age 为剩余秒数。access JWT 仍为 15 分钟，使用当前用户 authz_version；首次改密标记不会因刷新清除。

缺失/格式错误/未知/已撤销 Cookie 或禁用/锁定用户返回 `2200` / 401，过期返回 `2201` / 401，缺失或错误 CSRF 返回 `2204` / 403。重放已被替换的 token，在事务中撤销该 family 的所有未撤销 token，写入 `auth.refresh.replayed`，提交后返回 `2202` / 409；不影响其他登录 family。并发刷新只会有一次成功，另一请求按重放处理，因此客户端必须合并并发刷新。其他已通过协议校验的刷新失败记录 `auth.refresh.failure`。

`POST /auth/logout`：携带 Cookie 时同样必须有匹配的 CSRF，可使用当前 token 或已轮换的祖先 token 撤销整个 family。过期、禁用和锁定状态不阻止退出。成功返回空 204，清除相同 Path/SameSite 的 Cookie，并记录 `auth.logout`。重复退出或格式有效但未知的 token 也返回 204；无 Cookie 时直接返回 204 并清 Cookie。其他 family 保持有效，不修改用户 version/authz_version。

登出与重放只撤销 refresh family，不即时吊销已签发的 access JWT；客户端须清除 access/CSRF，本轮 access 最迟按 15 分钟有效期失效。改密则递增全局 authz_version 并撤销用户全部 refresh。后续角色/状态变更接口必须在同一事务内撤销受影响 refresh，会话表不保存授权版本快照，不能仅更新 authz_version 后留下旧 refresh。

刷新、重放撤销和登出均将状态写入与安全审计原子提交。任一写入失败返回 500 并完整回滚，不发送 Set-Cookie；CSRF 或 Origin 拒绝也不撤销会话。调用方在失败时不得假定服务端已登出。

```javascript
const response = await fetch("/api/v1/auth/refresh", {
  method: "POST",
  credentials: "include",
  headers: { "X-CSRF-Token": csrfToken }
});
// Success: replace the in-memory access and CSRF tokens with response.data values.
// Logout uses the same headers and credentials with /api/v1/auth/logout.
```

## 3. 用户管理

| 方法 | 路径 | 权限 | 说明 |
|---|---|---|---|
| GET | `/admin/users` | admin/super_admin | 用户列表；admin 默认只能查询 operator/engineer，super_admin 可查询全部 |
| POST | `/admin/users` | admin/super_admin | 创建用户；admin 只能创建 operator/engineer |
| GET | `/admin/users/{id}` | admin/super_admin | 用户详情；admin 不能读取 admin/super_admin 的敏感字段 |
| PATCH | `/admin/users/{id}` | admin/super_admin | 编辑用户；不允许修改 uid/username/role |
| PATCH | `/admin/users/{id}/status` | admin/super_admin | 启用/禁用；admin 不得操作 admin/super_admin |
| PATCH | `/admin/users/{id}/unlock` | admin/super_admin | 解锁；admin 仅能解锁 operator/engineer |
| PUT | `/admin/users/{id}/role` | super_admin | 分配四类固定角色 |
| POST | `/admin/users/{id}/reset-password` | admin/super_admin | 重置密码并设置 force_password_change；admin 仅能操作 operator/engineer |
| GET | `/admin/user-logs` | admin/super_admin | 分页查询审计日志 |

用户列表支持 `page`、`page_size`、`search`、`status`、`role` 和固定白名单 `sort` 参数；审计日志支持 `page`、`page_size`、`from`、`to`、`event_name`、`result` 和 `actor_user_id` 过滤。所有分页参数必须有服务端上限。

审计日志条目返回 `id、actor_user_id、target_user_id、event_name、result、request_id、created_at、details`；`details` 已脱敏，不返回密码、token、私钥、原始机器标识或内部堆栈。

创建用户请求必须且只能提供 `uid` 或 `username`；创建后身份标识不可修改：

```json
{"uid":"U10086","display_name":"Alice","role":"operator"}
```

服务端生成临时凭据并只在创建/重置响应中展示一次，明文不落库；默认 24 小时有效。超管首次登录必须改密，其临时凭据只能成功登录一次；非超管首次登录仅提示，业务端允许跳过，可在原有效期内继续登录。若部署采用外部邀请通道，可用一次性 activation token 替代临时密码。

创建/重置成功响应只在本次返回临时凭据：

```json
{"data":{"user":{...},"temporary_credential":"<show-once>","temporary_credential_expires_at":"2026-09-11T00:00:00Z"}}
```

客户端不得缓存或再次展示 `temporary_credential`。

创建和重置密码先在事务内校验并锁定当前调用者，再生成临时凭据及 Argon2id 哈希。它们与登录、改密共享按实际对端 IP 计数的 `auth.attemptsPerMinute` 预算；超限返回 `429 / RATE_LIMITED`，不写入账号或凭据变更。

用户对象统一返回 `id、uid、username、display_name、role、status、force_password_change、password_change_recommended、version、authz_version、last_login_at、created_at、updated_at`；临时凭据过期时间不返回给普通用户；缺失的 uid 或 username 返回 `null`，不返回密码哈希。force_password_change 与 password_change_recommended 按角色派生，不直接透传数据库同名字段。

同时提供或同时缺失返回 `USER_IDENTITY_ONE_OF_REQUIRED`；canonical 标识重复返回 `USER_IDENTITY_CONFLICT`。管理员提交管理员/超管角色返回 `FORBIDDEN`。

`PATCH /admin/users/{id}/status` 请求体为 `{"status":"active|disabled"}`；`PATCH /admin/users/{id}/unlock` 无业务字段，只能将 locked 转为 active；`PUT /admin/users/{id}/role` 请求体为 `{"role":"operator|engineer|admin|super_admin"}`。以上接口均遵循 `If-Match`，服务端不得覆盖并发更新。

## 4. Client 当前用户权限与行为日志

### 4.1 当前用户权限

`GET /client/me/permissions` 返回客户端业务判断所需的固定角色和能力码：

```json
{"data":{"user_id":"...","role":"engineer","status":"active","force_password_change":false,"authz_version":3,"capabilities":["business.read","business.operate","engineering.configure"]}}
```

客户端据此控制菜单、按钮和流程分支；实际业务 API 仍必须在服务端按角色校验。

业务服务端自动记录 Client 行为；如需前端埋点，使用非可信采集接口：

```http
POST /client/behavior-events
Authorization: Bearer <access_token>
Content-Type: application/json

{"event_name":"report.view","result":"success","details":{"source":"dashboard"}}
```

返回 `202 Accepted` 和 `event_id`。该接口是非可信 telemetry，不得用于记录登录、角色变更、禁用等安全审计；Admin 查询：`GET /admin/user-logs`，需要 `admin` 或 `super_admin`。

## 5. 错误码

错误响应的 `code` 是 JSON 整数，数值由 `SzxAdmin/Infra/ErrorCode.h` 中的 `ErrorCode` 固定定义。客户端按数值判断错误；`message` 仅用于说明，`request_id` 用于关联请求。例如资源不存在时，HTTP 状态为 404，响应为：

```json
{"code":2205,"message":"The requested resource was not found.","details":{},"request_id":"..."}
```

本次修正将初版响应中的字符串名称改为整数。调用方须将字符串比较（如 `code === "RESOURCE_NOT_FOUND"`）改为数字比较（`code === 2205`），不得把数字转为字符串传输。下表名称用于文档和诊断，不作为响应 `code` 的值。

| code | 名称 | 默认 HTTP 状态 |
|---|---|---|
| 2200 | AUTH_REQUIRED | 401 |
| 2201 | TOKEN_EXPIRED | 401 |
| 2202 | TOKEN_REPLAYED | 409 |
| 2203 | AUTH_INVALID_CREDENTIALS | 401 |
| 2204 | FORBIDDEN | 403 |
| 2205 | RESOURCE_NOT_FOUND | 404 |
| 2206 | VALIDATION_ERROR | 422 |
| 2207 | USER_IDENTITY_ONE_OF_REQUIRED | 422 |
| 2208 | USER_IDENTITY_CONFLICT | 409 |
| 2209 | CONFLICT_VERSION | 409 |
| 2210 | LAST_SUPER_ADMIN_PROTECTED | 409 |
| 2211 | RATE_LIMITED | 429 |
| 2212 | AUDIT_WRITE_FAILED | 500 |
| 2213 | REQUEST_ID_INVALID | 422 |
| 2214 | ROUTE_CONFLICT | 409 |
| 2215 | PAYLOAD_TOO_LARGE | 413 |
| 2216 | SERVICE_UNAVAILABLE | 503 |
| 2217 | TRANSPORT_FAILURE | 500 |
| 2218 | INVALID_CONFIGURATION | 422 |
| 2219 | METHOD_NOT_ALLOWED | 405 |
| 2220 | DATABASE_UNAVAILABLE | 503 |
| 2221 | DATABASE_QUERY_FAILED | 500 |
| 2222 | DATABASE_TRANSACTION_FAILED | 500 |
| 2223 | MIGRATION_FAILED | 500 |
| 2224 | MIGRATION_CONFLICT | 500 |
| 2299 | INTERNAL_FAILURE | 500 |

登录时 disabled/locked 统一映射为 `2203`（AUTH_INVALID_CREDENTIALS），不单独暴露账户状态。协议解析错误保留网络层 HTTP 状态，例如 400、417、431 对应业务错误码 `2206`，不会统一改成 422。

HTTP 映射固定为：401（未认证、过期或 refresh 失败）、403（权限/目标角色限制）、404（资源不存在）、409（版本冲突、最后超管保护或 refresh 重放）、422（参数校验失败）、429（限流）。错误 `details` 不得包含密码、token、密钥或内部堆栈。
