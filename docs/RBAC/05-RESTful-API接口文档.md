# RBAC REST API 接口参考

更新日期：2026-09-22。本文按当前服务实现描述接口，采用“参数表 → 请求示例 → 响应模型 → 错误码”的参考格式。

在线 Swagger UI：启动新版服务后访问 `/api/v1`（跳转 `/api/v1/`）。支持查看全部接口、搜索、Bearer 授权和 Try it out 调试；OpenAPI 下载地址为 `/api/v1/openapi.json`。修改 `server.apiPrefix` 后文档入口和调试地址随之变化。

## 1. 接口目录

**当前已实现：18 / 18 个接口操作。** 下表“已实现”表示后端已注册路由并实现处理逻辑，不代表对应前端页面已完成；OPTIONS 预检不单独计数。

默认 Base URL 为 `/api/v1`，以下路径均相对于该前缀。部署修改 `server.apiPrefix` 时，客户端同步替换前缀。

| 分组 | 方法 | 路径 | 功能 | 鉴权 | 成功状态 | 实现状态 |
|---|---|---|---|---|---|---|
| 服务 | GET | [/health](#health) | 健康检查 | 无 | 200 | 已实现 |
| 服务 | GET | [/ready](#ready) | 数据库就绪检查 | 无 | 200 | 已实现 |
| 认证 | POST | [/auth/login](#auth-login) | 登录 | 登录凭据 | 200 | 已实现 |
| 认证 | POST | [/auth/refresh](#auth-refresh) | 刷新会话 | Cookie + CSRF | 200 | 已实现 |
| 认证 | POST | [/auth/logout](#auth-logout) | 退出会话 | 有 Cookie 时校验 CSRF | 204 | 已实现 |
| 认证 | POST | [/auth/change-password](#auth-change-password) | 修改自己的密码 | Bearer + If-Match | 200 | 已实现 |
| 用户 | GET | [/admin/users](#users-list) | 用户列表 | Bearer，admin/super_admin | 200 | 已实现 |
| 用户 | POST | [/admin/users](#users-create) | 创建用户 | Bearer，admin/super_admin | 201 | 已实现 |
| 用户 | GET | [/admin/users/{id}](#users-get) | 用户详情 | Bearer，admin/super_admin | 200 | 已实现 |
| 用户 | PATCH | [/admin/users/{id}](#users-update) | 修改展示名 | Bearer + If-Match，admin/super_admin | 200 | 已实现 |
| 用户 | PATCH | [/admin/users/{id}/status](#users-status) | 启用／禁用 | Bearer + If-Match，admin/super_admin | 200 | 已实现 |
| 用户 | PATCH | [/admin/users/{id}/unlock](#users-unlock) | 解锁 | Bearer + If-Match，admin/super_admin | 200 | 已实现 |
| 用户 | PUT | [/admin/users/{id}/role](#users-role) | 修改角色 | Bearer + If-Match，super_admin | 200 | 已实现 |
| 用户 | POST | [/admin/users/{id}/reset-password](#users-reset-password) | 重置密码 | Bearer + If-Match，admin/super_admin | 200 | 已实现 |
| 审计 | GET | [/admin/audit-logs](#audit-logs) | 安全审计列表 | Bearer，admin/super_admin | 200 | 已实现 |
| 行为 | GET | [/admin/behavior-events](#behavior-events) | 用户行为列表 | Bearer，admin/super_admin | 200 | 已实现 |
| 客户端 | GET | [/client/me/permissions](#client-permissions) | 当前用户权限 | Bearer，任意有效角色 | 200 | 已实现 |
| 客户端 | POST | [/client/behavior-events](#client-behavior) | 上报行为事件 | Bearer，任意有效角色 | 202 | 已实现 |

认证、用户管理和客户端接口支持 `OPTIONS` 浏览器预检。当前没有删除用户接口。

## 2. 通用协议

### 2.1 请求头

| 名称 | 类型 | 必填条件 | 说明 |
|---|---|---|---|
| `Content-Type` | string | 发送 JSON 请求体时 | `application/json`，允许 UTF-8 charset |
| `Authorization` | string | Bearer 接口 | `Bearer <access_token>`；请求头值最多 8192 字节 |
| `If-Match` | string | 改密、用户 PATCH/PUT、重置密码 | 当前目标用户的 version，例如 `1` 或 `"1"`；范围 1–2147483647 |
| `Cookie` | string | refresh；logout 可选 | `szx_refresh=<refresh_token>`；浏览器自动携带 |
| `X-CSRF-Token` | string | 携带 refresh Cookie 的 refresh/logout | 最近一次登录或刷新返回的 csrf_token |
| `X-Request-ID` | string | 否 | 1–128 个字母、数字、`-`、`_`、`.`；未提供或非法时生成新 ID |
| `Origin` | string | 浏览器自动设置 | 受 auth.allowedOrigin 控制 |

请求体必须为 JSON 对象；未知字段、错误类型、非法枚举通常返回 `422 / 2206`。认证、用户写接口和行为事件的请求体上限为 8192 字节，超过返回 `413 / 2215`；客户端事件的 details 另有 4096 字节限制。

If-Match 不接受弱 ETag、通配符或多个版本，也不能用 body 中的 version 字段替代。创建用户、登录、刷新、登出、行为上报不要求 If-Match。

### 2.2 响应包装

成功响应统一使用 `data` 与 `request_id`；分页数据位于 data 内：

```json
{"data":{"items":[],"total":0,"page":1,"page_size":20},"request_id":"users-list-001"}
```

错误响应不包含 data：

```json
{"code":2209,"message":"The resource was changed by another request.","details":{},"request_id":"users-update-001"}
```

业务响应携带 `X-Request-ID` 和 `Cache-Control: no-store`。204 没有响应体，客户端不要调用 JSON 解析。按整数 code 判断错误，不依赖英文 message 文本；完整映射见第 7 节。

### 2.3 时间与空值

| 数据 | JSON 类型／单位 | 示例 |
|---|---|---|
| 响应 created_at、updated_at、last_login_at | integer，UTC Unix 毫秒 | `1789636500000` |
| expires_at、temporary_credential_expires_at | integer，UTC Unix 毫秒 | `1789637400000` |
| 未发生过的 last_login_at | null | `null` |
| expires_in | integer，秒 | `900` |
| JWT 内部 iat、nbf、exp | integer，Unix 秒 | `1789636500` |
| 日志查询参数 from、to | string，UTC ISO 8601 | `2026-09-17T09:15:00Z` |

绝对时间响应不再返回日期字符串，数据库微秒小数截断到毫秒。业务端自行转为本地时间，例如 `new Date(timestamp).toLocaleString()`；处理可空时间前先判断 null。

用户编辑、状态变更、解锁、角色分配和重置密码的响应，在事务提交前读取数据库最新用户记录，返回本次写入实际生成的 updated_at。

**查询输入与响应输出的格式不同**：from／to 当前仍要求精确的 `YYYY-MM-DDTHH:mm:ssZ`，不接受毫秒时间戳、小数秒或时区偏移字符串。

uid 和 username 为二选一登录标识，用户响应只返回实际设置的字段，未设置的字段直接省略。id 是所有用户都有的稳定 UUID 主键。

### 2.4 鉴权、角色与版本

Bearer 接口验证 JWT 签名、issuer、audience、有效期，并检查数据库中的用户状态和 authz_version。只有 active 用户及当前授权版本可访问。

| 角色 | 用户管理范围 | 角色分配 | 日志查询 |
|---|---|---|---|
| operator | 无 | 无 | 无 |
| engineer | 无 | 无 | 无 |
| admin | 仅 operator/engineer | 无 | 有 |
| super_admin | 全部四种角色 | 有 | 有 |

admin 读取或修改 admin/super_admin 用户时返回 `403 / 2204`，不会返回裁剪后的用户对象。最后一个 active 超管不能被禁用或降级，返回 `409 / 2210`。

version 用于并发更新，冲突后应重新获取用户并使用新版本提交。authz_version 用于使旧 access token 失效。实际角色、状态、解锁和密码变更会递增授权版本并撤销受影响用户的 refresh 会话；仅修改展示名不使会话失效。

### 2.5 浏览器、Cookie 与限流

生产接入使用 HTTPS。szx_refresh 为 `HttpOnly; Secure; SameSite=None`，默认空闲期限为 30 分钟，当前会话族绝对期限为 7 天；Cookie `Max-Age` 取两个期限的剩余时间较小值。Path 为 `${apiPrefix}/auth`（前缀为 `/` 时为 `/auth`）。`auth.allowedOrigin` 默认 `"*"`，允许 HTTP/HTTPS 来源跨域携带凭据，响应回显具体 Origin 并设置 `Vary: Origin`。也可配置单一 Origin 精确限制来源。非法配置阻止启动；不允许的来源或 `Origin: null` 返回 `403 / 2204`。跨站前端需启用 credentials/withCredentials，刷新和登出仍须提供 CSRF token；浏览器第三方 Cookie 策略可能限制刷新。成功刷新会轮换 token 并重置空闲窗口，但不延长当前会话族原到期时间。

超管 `force_password_change=true` 时，后端拒绝用户管理、审计查询、权限查询和行为上报（`403 / 2204`）；仍允许登录、刷新、登出和修改自己的密码。成功改密并重新登录后才恢复业务访问。该规则由后端检查，直接调用 API 也无法绕过；其他角色的建议改密行为不变。

浏览器登录、刷新、登出使用 `credentials: "include"`。允许的来源会收到具体 Access-Control-Allow-Origin、Access-Control-Allow-Credentials: true 和 Vary: Origin；来源不匹配返回 `403 / 2204`。无 Origin 的原生客户端可访问。用户和客户端接口沿用同一 Origin 策略。

| 预算 | 默认值 | 配置 | 作用范围 |
|---|---|---|---|
| 认证／凭据签发 | 每 IP 每分钟 30 次 | auth.attemptsPerMinute，1–1000 | 登录、改密、刷新、登出、创建用户、重置密码共享 |
| 行为事件 | 不按分钟限流；受进程内有界接收队列约束 | telemetry.queueCapacity，默认 10000 | 队列满返回 `503 / 2225`，写入器不可用返回 `503 / 2226` |

认证预算超限返回 `429 / 2211`。按实际 socket 对端 IP 计数，不信任 X-Forwarded-For。认证计数单进程内有效，多实例不共享。无 Cookie 的 logout 不计数；改密在 JWT 验证后计数；refresh/logout 在 Cookie 格式及 CSRF 校验后计数；创建／重置在事务鉴权后、生成凭据前计数。行为事件接口完成鉴权、校验和脱敏后立即尝试非阻塞入队，短时间突发由队列吸收，后台按 `telemetry.batchSize` 和 `telemetry.flushIntervalMs` 批量延迟写入；成功响应 `202` 只表示已入当前进程内存队列，不表示已提交数据库。

## 3. 响应模型

### 3.1 User

登录、刷新、用户详情、列表元素和用户修改响应共用此模型。

| 字段 | 类型 | 返回规则 | 说明 |
|---|---|---|---|
| id | string(UUID) | 必有 | 稳定用户 ID；路径 UUID 字母不区分大小写 |
| uid | string | UID 账号才有 | UID 登录标识，区分大小写 |
| username | string | 用户名账号才有 | 展示值；匹配使用规范化后的小写值 |
| display_name | string | 必有 | 展示名 |
| role | string | 必有 | 数据库角色 code，支持内置及自定义角色 |
| status | string | 必有 | active / disabled / locked |
| force_password_change | boolean | 必有 | 仅待改密超管为 true |
| password_change_recommended | boolean | 必有 | 待改密非超管为 true，可跳过提示 |
| version | integer | 必有 | 下次变更时提交到 If-Match |
| authz_version | integer | 必有 | 当前授权版本 |
| last_login_at | integer / null | 必有 | 最近登录时间，UTC 毫秒；未登录为 null |
| created_at | integer | 必有 | 创建时间，UTC 毫秒 |
| updated_at | integer | 必有 | 更新时间，UTC 毫秒 |

不返回密码哈希、临时凭据过期时间或内部锁定信息。uid／username 只返回其中一个；客户端迁移时兼容旧响应中的显式 null。

### 3.2 Session

作为登录与刷新响应的 data。

| 字段 | 类型 | 说明 |
|---|---|---|
| user | User | 当前用户 |
| access_token | string | PS256 JWT；有效期 15 分钟 |
| token_type | string | 固定 Bearer |
| expires_in | integer | 固定 900，单位秒 |
| expires_at | integer | access 到期时间，UTC 毫秒；等于 JWT exp × 1000 |
| authz_version | integer | 当前授权版本，与 user 中一致 |
| csrf_token | string | 64 位小写十六进制；刷新／登出使用 |

refresh token 只通过 Set-Cookie 返回，不在 JSON 中返回。当前算法仍是 PS256，未切换为 ES256；客户端应把 access token 当作不透明凭据，不依赖字符长度。

### 3.3 CredentialIssue

作为创建用户和重置密码响应的 data。

| 字段 | 类型 | 说明 |
|---|---|---|
| user | User | 创建或重置后的用户 |
| temporary_credential | string | 随机 20 字符临时密码，仅本次响应可见 |
| temporary_credential_expires_at | integer | 到期时间，UTC 毫秒；默认 24 小时有效 |

临时密码登录时同样先计算 SHA-256。超管临时密码只能成功登录一次，随后必须改密；其他角色可跳过改密提示，在原有效期内重复登录。跳过不会清除待改密标记或延长有效期。

### 3.4 Page<T> 与 AuditLog

| 分页字段 | 类型 | 说明 |
|---|---|---|
| items | array<T> | 本页条目 |
| total | integer | 过滤后的总条数 |
| page | integer | 当前页，起始为 1 |
| page_size | integer | 每页数量 |

| AuditLog 字段 | 类型 | 说明 |
|---|---|---|
| id | string(UUID) | 日志 ID |
| actor_user_id | string | 操作者 ID；缺失时为空字符串 |
| actor_username | string | 操作者当前用户名；UID 账号返回 UID，无法关联操作者时为空字符串 |
| target_user_id | string | 目标用户 ID；缺失时为空字符串 |
| event_name | string | 事件名称 |
| result | string | success / failure / denied |
| request_id | string | 关联请求 ID |
| peer_address | string | 当前响应固定为空字符串，不暴露数据库 IP |
| details | object | 脱敏后的 JSON 对象，不是 JSON 字符串 |
| created_at | integer | 事件时间，UTC 毫秒 |

## 4. 服务与认证接口

<a id="health"></a>

### GET /health — 健康检查

**实现状态：已实现**

无需鉴权，无请求参数和请求体。200 响应：

```json
{"data":{"status":"ok"},"request_id":"health-001"}
```

<a id="ready"></a>

### GET /ready — 数据库就绪检查

**实现状态：已实现**

无需鉴权，无请求参数和请求体。200 响应：

```json
{"data":{"status":"ready"},"request_id":"ready-001"}
```

数据库不可用时返回 `503 / 2220`。

<a id="auth-login"></a>

### POST /auth/login — 登录

**实现状态：已实现**

**鉴权：** 无 Bearer；提交登录凭据。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| identifier | string | 是 | UID 或用户名；原始 UTF-8 最多 512 字节；规范化后 1–64 个 Unicode 字符 |
| password_hash | string | 是 | 原始密码 UTF-8 字节的 SHA-256，64 位小写十六进制 |

identifier 去首尾空白并执行 Unicode NFC，UID 保留大小写，用户名另按不依赖 locale 的规则小写匹配。若 UID 和用户名匹配到不同账号，统一拒绝登录。

请求示例（尖括号内容为占位值）：

```http
POST /api/v1/auth/login
Content-Type: application/json
X-Request-ID: login-001
```

```json
{"identifier":"admin","password_hash":"<64位小写SHA-256摘要>"}
```

**成功：** 200，data 为 Session，同时设置 refresh Cookie。

```json
{
  "data": {
    "access_token": "<access-token>",
    "token_type": "Bearer",
    "expires_in": 900,
    "expires_at": 1789637400000,
    "authz_version": 1,
    "csrf_token": "<csrf-token>",
    "user": {
      "id": "10000000-0000-4000-8000-000000000001",
      "username": "admin",
      "display_name": "System Administrator",
      "role": "super_admin",
      "status": "active",
      "force_password_change": true,
      "password_change_recommended": false,
      "version": 1,
      "authz_version": 1,
      "last_login_at": 1789636500000,
      "created_at": 1789550100510,
      "updated_at": 1789636500000
    }
  },
  "request_id": "login-001"
}
```

| HTTP / code | 条件 |
|---|---|
| 401 / 2203 | 用户不存在、密码错误、disabled/locked、临时密码已过期或已消费 |
| 422 / 2206 | 字段、摘要格式或身份标识非法；不接受明文 password 字段 |
| 429 / 2211 | 认证预算耗尽 |
| 500 / 2212 | 安全审计写入失败，登录事务回滚 |

默认连续 5 次错误密码将 active 账号置为 locked。locked_until 默认记录 900 秒后，但到期不会自动解锁，必须调用解锁接口。成功登录清零失败计数。未知用户、禁用、锁定不对外区分原因。

<a id="auth-refresh"></a>

### POST /auth/refresh — 刷新会话

**实现状态：已实现**

**鉴权：** 必须携带 szx_refresh Cookie 和当前 X-CSRF-Token；不要求 Bearer 或 If-Match。

Body 可省略，或发送空对象 `{}`，不接受其他字段。refresh token 只能来自 Cookie，不能放入 body、query 或 Authorization。

```http
POST /api/v1/auth/refresh
Cookie: szx_refresh=<refresh-token>
X-CSRF-Token: <csrf-token>
```

**成功：** 200，data 为 Session，与登录相同；Set-Cookie 替换 refresh token。客户端须替换 access token 和 CSRF token。成功轮换会重新开始默认 30 分钟的空闲计时，但会话族仍在首次登录后最多 7 天到期。

| HTTP / code | 条件 |
|---|---|
| 401 / 2200 | 缺失、非法、未知或普通已撤销 Cookie；账号不可用 |
| 401 / 2201 | refresh token 已超过绝对期限或空闲期限 |
| 403 / 2204 | CSRF 缺失、不匹配，或 Origin 不允许 |
| 409 / 2202 | 使用已轮换的旧 token，整个会话族被撤销 |
| 422 / 2206 | 重复同名 Cookie、非法请求体 |
| 429 / 2211 | 认证预算耗尽 |

客户端必须合并并发刷新：同一 token 并发刷新只有一次成功，另一请求会按重放处理，撤销同一会话族。其他登录会话族不受影响。写入或审计失败时事务回滚，不发送新的 Cookie。

<a id="auth-logout"></a>

### POST /auth/logout — 退出当前会话

**实现状态：已实现**

**鉴权：** 有 refresh Cookie 时必须携带匹配的 CSRF；无 Cookie 时直接清除浏览器 Cookie。不要求 Bearer 或 If-Match。

Body 可省略或为 `{}`。请求头与刷新接口相同，路径改为 `/auth/logout`。

**成功：** 204 No Content，清除 refresh Cookie，没有 JSON 响应体。重复退出、格式正确但未知的 token 均可返回 204。

| HTTP / code | 条件 |
|---|---|
| 401 / 2200 | refresh Cookie 编码非法 |
| 403 / 2204 | 携带 Cookie 但 CSRF 缺失／错误，或 Origin 不允许 |
| 422 / 2206 | 重复同名 Cookie、非法请求体 |
| 429 / 2211 | 认证预算耗尽；无 Cookie 的退出不计数 |
| 500 / 2212 | 审计失败，撤销操作回滚 |

退出只撤销当前 refresh 会话族，不即时吊销 access JWT；客户端应清除本地 access/CSRF，旧 access 最迟按 15 分钟有效期失效。过期、锁定或禁用不阻止合法会话退出。失败响应不能视为已退出成功。

<a id="auth-change-password"></a>

### POST /auth/change-password — 修改自己的密码

**实现状态：已实现**

**鉴权：** Bearer；If-Match 为当前用户的 version，不接受指定目标用户。不使用 refresh Cookie，也不要求 CSRF。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| old_password_hash | string | 是 | 原密码的 64 位小写 SHA-256 |
| new_password_hash | string | 是 | 新密码的 64 位小写 SHA-256；不得与旧摘要相同 |

```http
POST /api/v1/auth/change-password
Authorization: Bearer <access-token>
If-Match: 1
Content-Type: application/json
```

```json
{"old_password_hash":"<旧密码SHA-256>","new_password_hash":"<新密码SHA-256>"}
```

**成功：** 200，清除 refresh Cookie，要求重新登录。

```json
{"data":{"version":2,"authz_version":2,"force_password_change":false,"password_change_recommended":false,"reauthentication_required":true},"request_id":"password-change-001"}
```

错误：`401 / 2200`（会话无效）、`401 / 2203`（旧密码错误）、`409 / 2209`（版本冲突）、`422 / 2206`（请求非法）、`429 / 2211`（限流）、`500 / 2212`（审计失败）。

成功后清除待改密标记及临时有效期，递增 version/authz_version，撤销全部 refresh 会话，旧 access 失效。超管可用刚登录时已消费的临时密码摘要完成首次改密。客户端负责原始密码强度检查，服务端不从摘要推断密码复杂度。

## 5. 用户管理与审计接口

本节全部接口要求 Bearer。所有 `{id}` 均为必填 Path 参数，类型 string(UUID)，传用户 id 而非 uid 或 username；UUID 支持大写十六进制字母。

公共错误包括 `401 / 2200`（会话无效）、`403 / 2204`（角色或目标范围不允许）、`404 / 2205`（用户不存在）、`422 / 2206`（参数非法）、`500 / 2221`（数据库失败）。变更接口另有 `409 / 2209`（版本冲突）、`500 / 2212`（审计失败）。下文列出各接口的额外规则。

<a id="users-list"></a>

### GET /admin/users — 用户列表

**实现状态：已实现**

**权限：** admin/super_admin。admin 仅能列出 operator/engineer。

| Query 参数 | 类型 | 必填 | 默认／约束 |
|---|---|---|---|
| page | integer | 否 | 默认 1；范围 1–4294967295 |
| page_size | integer | 否 | 默认 20；范围 1–100 |
| search | string | 否 | 最多 128 个 Unicode 字符，无控制字符；搜索 uid、username、display_name |
| role | string | 否 | 调用者管理范围内的数据库角色 code，可筛选停用角色 |
| status | string | 否 | active / disabled / locked |
| sort | string | 否 | 默认 -created_at；白名单见下文 |

sort 仅接受 `-created_at`、`created_at`、`display_name`、`-display_name`、`last_login_at`、`-last_login_at`、`role`、`status`，前缀 - 表示降序。未知 Query 参数返回 `422 / 2206`；admin 显式请求 role=admin/super_admin 返回 `403 / 2204`。

```http
GET /api/v1/admin/users?page=1&page_size=20&role=operator&sort=-created_at
Authorization: Bearer <access-token>
```

**成功：** 200，data 为 `Page<User>`。无匹配结果示例：

```json
{"data":{"items":[],"total":0,"page":1,"page_size":20},"request_id":"users-list-001"}
```

<a id="users-create"></a>

### POST /admin/users — 创建用户

**实现状态：已实现**

**权限：** admin/super_admin；admin 只能创建 operator/engineer。不要求 If-Match。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| uid | string | 与 username 二选一 | 规范化后 1–64 个 Unicode 字符；保留大小写 |
| username | string | 与 uid 二选一 | 规范化后 1–64 个 Unicode 字符；按小写 canonical 判重 |
| display_name | string | 是 | 1–128 个 Unicode 字符，无控制字符，不得全为空格 |
| role | string | 是 | 当前启用且在调用者管理范围内的角色 code，支持自定义角色 |

可选 Body 字段 `password_hash` 为初始密码 UTF-8 SHA-256 摘要，必须是 64 位小写十六进制字符串；空字符串、null 和其他类型均返回 `422 / 2206`。省略时沿用自动生成临时密码的行为。前端按目标角色校验指定密码：操作员/技术员（engineer）至少 6 个 Unicode 字符，允许纯数字或纯字母；管理员/超级管理员至少 8 个 Unicode 字符且包含字母和数字。两次输入须一致；服务端仅收到摘要，校验摘要格式并加盐存储。

不接受明文密码、状态、id 或版本字段。身份字段的 null 视为未提供；UID 和用户名均去首尾空白、执行 Unicode NFC，原始输入最多 512 字节。用户名保留重音差异，UID 区分大小写；创建后身份不可修改。

UID 与 username 共用登录入口，因此也检查跨字段冲突：已有 username=admin 时，uid=admin 或 uid=ADMIN 均返回 `409 / 2208`；已有 uid=U10086 时，username=u10086 同样冲突。两个 UID 的 U10086 与 u10086 仍可分别存在。并发创建遵循相同规则，冲突请求不会创建用户或签发临时凭据。

```json
{"uid":"U10086","display_name":"Alice","role":"operator"}
```

**成功：** 201，data 为 CredentialIssue。

```json
{
  "data": {
    "user": {
      "id": "10000000-0000-4000-8000-000000000002",
      "uid": "U10086",
      "display_name": "Alice",
      "role": "operator",
      "status": "active",
      "force_password_change": false,
      "password_change_recommended": true,
      "version": 1,
      "authz_version": 1,
      "last_login_at": null,
      "created_at": 1789636500000,
      "updated_at": 1789636500000
    },
    "temporary_credential": "<本次生成的临时密码>",
    "temporary_credential_expires_at": 1789722900000
  },
  "request_id": "users-create-001"
}
```

指定 `password_hash` 时，响应中 `temporary_credential` 和 `temporary_credential_expires_at` 均为 null，不回传密码或摘要，也不设置临时密码的 24 小时有效期。首次登录改密策略保持不变：超管强制修改，其他角色建议修改。省略该字段时，响应仍返回一次性的随机临时密码及 24 小时有效期。

额外错误：`422 / 2207`（uid/username 同时提供或同时缺失）、`409 / 2208`（canonical 身份重复）、`429 / 2211`（凭据签发预算耗尽）。仅一个标识但类型错误返回 `422 / 2206`。临时密码仅本次展示，不应持久缓存。

<a id="users-get"></a>

### GET /admin/users/{id} — 用户详情

**实现状态：已实现**

**权限：** admin/super_admin，遵循目标角色范围。必填 Path id，无请求体。

```http
GET /api/v1/admin/users/10000000-0000-4000-8000-000000000002
Authorization: Bearer <access-token>
```

**成功：** 200，data 为 User，与登录的 data.user 结构相同，不额外套一层 user。错误见本节公共错误表。

<a id="users-update"></a>

### PATCH /admin/users/{id} — 修改展示名

**实现状态：已实现**

**权限：** admin/super_admin；必填 Path id 和 Header If-Match。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| display_name | string | 是 | 1–128 个 Unicode 字符，无控制字符，不得全为空格 |

```http
PATCH /api/v1/admin/users/10000000-0000-4000-8000-000000000002
Authorization: Bearer <access-token>
If-Match: 1
Content-Type: application/json
```

```json
{"display_name":"Alice Chen"}
```

**成功：** 200，data 为 User。实际名称变化才递增 version；authz_version 不变。拒绝 uid、username、role、status 等额外字段。错误见本节公共错误表。

<a id="users-status"></a>

### PATCH /admin/users/{id}/status — 启用／禁用

**实现状态：已实现**

**权限：** admin/super_admin；必填 Path id 和 Header If-Match。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| status | string | 是 | 仅 active / disabled |

```json
{"status":"disabled"}
```

**成功：** 200，data 为 User。实际状态变化时递增 version/authz_version，并撤销所有 refresh 会话；提交原状态不递增版本。

locked 不能直接改为 active，否则返回 `422 / 2206`，须调用 unlock。禁用最后一个 active 超管返回 `409 / 2210`；其他错误见本节公共错误表。

禁止禁用自己，返回 `403 / 2204`，不会修改状态、版本或会话；不能管理高于自身或超出既有管理范围的角色。自我禁用限制优先于最后一个超管保护。

<a id="users-unlock"></a>

### PATCH /admin/users/{id}/unlock — 解锁

**实现状态：已实现**

**权限：** admin/super_admin；必填 Path id 和 Header If-Match。

Body 可省略或为 `{}`，不接受业务字段。

```http
PATCH /api/v1/admin/users/10000000-0000-4000-8000-000000000002/unlock
Authorization: Bearer <access-token>
If-Match: 3
```

**成功：** 200，data 为 User；locked 变为 active，清零失败计数、清空锁定时间，递增 version/authz_version 并撤销 refresh 会话。

目标不是 locked 时返回 `422 / 2206`，不能用此接口启用 disabled 用户；其他错误见本节公共错误表。

<a id="users-role"></a>

### PUT /admin/users/{id}/role — 修改角色

**实现状态：已实现**

**权限：** 仅 super_admin；必填 Path id 和 Header If-Match。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| role | string | 是 | operator / engineer / admin / super_admin |

```json
{"role":"engineer"}
```

**成功：** 200，data 为 User。实际角色变化时递增 version/authz_version 并撤销 refresh 会话；提交原角色不递增版本。最后一个 active 超管不能降级，否则返回 `409 / 2210`；其他错误见本节公共错误表。

禁止给自己分配角色，即使提交原角色也返回 `403 / 2204`；该限制优先于最后一个超管保护。目标和新角色必须在调用者允许管理的范围内。

<a id="users-reset-password"></a>

### POST /admin/users/{id}/reset-password — 重置密码

**实现状态：已实现**

**权限：** admin/super_admin；必填 Path id 和 Header If-Match。

Body 可省略或为 `{}`，此时随机生成临时密码。也可提供 `password_hash` 指定新密码，格式与创建用户一致：64 位小写十六进制 SHA-256 摘要，不接受明文密码、null 或空摘要；非法值返回 `422 / 2206`。前端按目标用户角色使用与创建用户相同的密码规则。

```http
POST /api/v1/admin/users/10000000-0000-4000-8000-000000000002/reset-password
Authorization: Bearer <access-token>
If-Match: 4
```

**成功：** 200，data 为 CredentialIssue，结构与创建用户的 data 相同。

未指定密码时，服务端生成新的临时密码并设置 24 小时有效期；指定密码时不设置临时有效期，响应 `temporary_credential` 和 `temporary_credential_expires_at` 均为 null，不回传密码或摘要。两种方式均设置待改密标记，递增 version/authz_version，撤销全部 refresh 会话，旧密码失效。是否强制改密由目标角色决定，见 User 模型。超出签发预算返回 `429 / 2211`，不产生部分密码变更；其他错误见本节公共错误表。

<a id="audit-logs"></a>

### GET /admin/audit-logs — 安全审计列表

支持 `page`、`page_size`、`from`、`to`、`event_name`、`event_search`、`result`、`actor_user_id` 和 `actor_search`。权限为 `audit.read`，只读取 `security_audit_logs`，不返回客户端行为事件；返回 `Page<AuditLog>`。

按 `created_at DESC, id ASC` 排序；查询和分页前完成事件/操作者过滤，业务 API 不支持修改或删除日志。

<a id="behavior-events"></a>

### GET /admin/behavior-events — 用户行为列表

权限为 `behavior.read`。支持 `page`、`page_size`、`from`、`to`、`event_name`、`event_search`、`result`、`user_id`、`user_search`。`from`、`to` 必须同时提供；两者省略时默认最近 24 小时，显式跨度不得超过 31 天。返回条目包含 `id、user_id、username、event_name、result、request_id、details、created_at`，只读取 `client_behavior_events`，行为事件不会被解释为可信安全事实。

## 6. 客户端接口

<a id="client-permissions"></a>

### GET /client/me/permissions — 当前用户权限

**实现状态：已实现**

**鉴权：** Bearer，任意有效角色。无请求参数和请求体。

| data 字段 | 类型 | 说明 |
|---|---|---|
| user_id | string(UUID) | 当前用户 ID |
| role | string | 当前固定角色 |
| status | string | 当前有效用户为 active |
| force_password_change | boolean | 当前超管是否必须改密 |
| authz_version | integer | 当前授权版本 |
| capabilities | array<string> | 该角色的能力码 |

**成功：** 200。

```json
{"data":{"user_id":"10000000-0000-4000-8000-000000000002","role":"engineer","status":"active","force_password_change":false,"authz_version":3,"capabilities":["business.read","business.operate","engineering.configure"]},"request_id":"permissions-001"}
```

| 角色 | 能力 |
|---|---|
| operator | business.read、business.operate |
| engineer | operator 能力，加 engineering.configure |
| admin | engineer 能力，加 user.read、user.create、user.update、user.status、user.unlock、user.password.reset、audit.read、behavior.read |
| super_admin | admin 能力，加 user.role.assign |

错误：`401 / 2200`（会话无效）、`403 / 2204`（Origin 不允许）、`500 / 2221`（数据库失败）。菜单可依据此响应显示，实际业务请求仍须服务端鉴权。密码修改建议读取登录／刷新返回的 User；此接口不返回 password_change_recommended。

<a id="client-behavior"></a>

### POST /client/behavior-events — 上报行为事件

**实现状态：已实现**

**鉴权：** Bearer，任意有效角色；不要求 If-Match。

| Body 字段 | 类型 | 必填 | 约束 |
|---|---|---|---|
| event_name | string | 是 | 1–96 个 ASCII 字符；首字符为小写字母；只允许小写字母、数字、下划线、点；无首尾点或连续点 |
| result | string | 是 | 仅 success / failure |
| details | object | 是 | 序列化后最多 4096 字节；可以是空对象 |

```json
{"event_name":"report.view","result":"success","details":{"source":"dashboard"}}
```

**成功：** 202 Accepted。

```json
{"data":{"event_id":"20000000-0000-4000-8000-000000000001"},"request_id":"report-view-001"}
```

此接口为非可信客户端 telemetry；拒绝 auth.、user. 前缀，不能伪造登录、角色变更等服务端审计。操作者和目标用户均取当前用户，调用方不能指定。

事件成功进入有界内存队列后立即返回 202；后台消费者按最多 200 条或 200ms 组批，并在成功提交之间保持至少 200ms。接口不执行每分钟或每用户计数限制。错误：`401 / 2200`（会话无效）、`403 / 2204`（Origin 不允许）、`422 / 2206`（字段／名称／details 非法）、`503 / 2225`（队列已满）、`503 / 2226`（写入器未接入或正在排空）。队列拒绝响应携带 `Retry-After`，事件未被接收。

入库前递归处理对象和数组：字段名转 ASCII 小写并去掉非字母数字字符后，删除包含 password、token、secret、privatekey、apikey、credential、machineid、hardwareid、fingerprint、stack、traceback，或等于 authorization、cookie、setcookie 的字段。深度达到 32 的值替换为 null，其余元数据保留。此规则按字段名识别，调用方仍不得把凭据写入任意自由文本值。

## 7. 错误码参考

名称供文档和诊断使用，HTTP 响应中的 code 始终为整数。

| code | 名称 | 默认 HTTP |
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
| 2225 | BEHAVIOR_EVENT_QUEUE_FULL | 503 |
| 2226 | BEHAVIOR_EVENT_QUEUE_CLOSED | 503 |
| 2299 | INTERNAL_FAILURE | 500 |

包含初始化与基础设施错误，不代表每个接口都会产生全部错误码。底层 HTTP 解析错误保留 400、417、431 等原始 HTTP 状态，可对应业务码 2206，不统一改为 422。错误 details 不包含密码、token、密钥或内部堆栈。

## 8. 接入示例与兼容说明

### 8.1 浏览器登录、刷新与本地时间

```javascript
// 密码不 trim、不转换大小写、不做 Unicode 规范化。
const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(password));
const password_hash = Array.from(new Uint8Array(digest), b => b.toString(16).padStart(2, "0")).join("");
const response = await fetch("/api/v1/auth/login", {
  method: "POST",
  credentials: "include",
  headers: {"Content-Type": "application/json"},
  body: JSON.stringify({identifier, password_hash})
});
const body = await response.json();
if (!response.ok) throw new Error(`${body.code}: ${body.message}`);
let {access_token, csrf_token, user} = body.data;
const identity = user.uid ?? user.username;
const localLoginTime = user.last_login_at == null
  ? "未登录"
  : new Date(user.last_login_at).toLocaleString();

// 实际业务端应合并并发刷新；每次成功后替换两种 token。
const refreshed = await fetch("/api/v1/auth/refresh", {
  method: "POST",
  credentials: "include",
  headers: {"X-CSRF-Token": csrf_token}
});
const refreshBody = await refreshed.json();
if (!refreshed.ok) throw new Error(`${refreshBody.code}: ${refreshBody.message}`);
({access_token, csrf_token, user} = refreshBody.data);
```

### 8.2 响应格式迁移

| 旧用法 | 当前用法 |
|---|---|
| 将 expires_at／用户日期当字符串解析 | 读取 UTC 毫秒整数；JavaScript 不要再乘 1000 |
| 依赖 uid: null 或 username: null 必须存在 | 按可选字段读取；稳定引用使用 id |
| 对日志 details 再次 JSON.parse | details 已是对象，直接读取 |
| 使用整数毫秒拼接日志 from/to | 输入仍需 UTC ISO 8601 秒精度字符串 |

### 8.3 部署与凭据说明

Admin 前端入口为 `/admin`（308 跳转 `/admin/`），与本文 API 前缀 `/api/v1` 独立；例如页面 `/admin/users` 调用接口 `/api/v1/admin/users`。将 `tools/szx_admin/web/admin/dist` 内容部署到 `server.staticRoot/admin`，配置 `spaFallback: true`。完整构建、启动和故障排查见 [Admin 部署与使用指南](12-Admin部署与使用指南.md)。

启动前配置 auth.privateKeyPath 和 auth.publicKeyPath（PEM/DER，路径相对进程工作目录）；不自动生成临时密钥。当前使用 PS256／RSA-3072，issuer/audience 默认 szx-admin / szx-client。

```sh
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3072 -out jwt-private.pem
openssl pkey -in jwt-private.pem -pubout -out jwt-public.pem
```

密钥放在静态目录之外；重启加载相同密钥可保留未过期 token 的可验证性。也可用仓库 `szx_license_issuer keygen` 生成专用密钥对，不与许可证签发共用密钥。

服务端将客户端 SHA-256 摘要再经随机盐 Argon2id 哈希后保存，格式为 `sha256-v1:<Argon2id PHC>`。旧原密码 Argon2id 凭据不能直接转换；仅仍使用默认初始凭据且待改密的初始超管可在成功登录时自动升级，其他旧账号需明确重置密码。访问日志不记录 query、Authorization 或请求体，只保留 request ID、方法、路径和状态。

## 机器性能接口

监控对象是 **`szx_admin_service` 所在机器**，不是浏览器所在机器。两个接口均需 Bearer 认证和 `system.metrics.read` 权限；迁移 11 为 `admin` / `super_admin` 授权，更新其 `authz_version`，升级后旧会话可能需重新登录。接口不缓存（`Cache-Control: no-store`），不接受任意磁盘路径参数。

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/api/v1/admin/system/metrics` | 当前 CPU、物理内存与本地卷容量快照 |
| GET | `/api/v1/admin/system/metrics/history?window_seconds=300` | 最近 1–900 秒的 CPU/内存历史，默认 300 秒 |

沿用 `{data, request_id}` 成功信封以及原有错误信封：401 未认证、403 缺少权限、422 参数错误、503/2216 采集服务未运行。拒绝未知参数、非正整数或超过 900 的时间窗口。指标的局部采集失败不使整个快照失败。

Metrics 的成功响应采用 `SystemMetricsDto.h` 中的强类型 DTO，使用 `SZX_REFL` 注册反射，由项目 `SZX::COMMON::Serializer::to_json`（iguana）一次生成完整 `{data, request_id}` JSON。采集服务只返回 DTO；HTTP 层不手工构建指标 JSON，也不先序列化再解析/重建 JSON。`null`、空数组、路径转义和 64 位数值保持既有协议，错误响应沿用公共 RestApi 错误契约。

快照 `data` 字段：

- `instance_id`：每次启动采集器生成的 UUID；同一实例内 `sequence` 从 1 递增，用于去重和识别重启。
- `sampled_at`：UTC Unix 毫秒；`platform`：`windows` / `linux`；`logical_cpus`：逻辑处理器数量。
- `sample_interval_ms=2000`，`disk_interval_ms=30000`。
- `cpu` / `memory`：`status`、`error_code`、`usage_percent`、`total_bytes`、`used_bytes`、`available_bytes`、`free_bytes`。
- `disks[]`：最多 32 个去重后的本地卷，`id`、`path`、自己的 `sampled_at` 和 `capacity`（与指标字段一致）。磁盘采样时间不得用快照时间替代。

`status` 为 `ok`、`warming_up` 或 `unavailable`；CPU 首次需要差分采样，返回 `warming_up` / 空使用率，不伪造 0%。失败返回 `unavailable` / `error_code=2216`，相关数值保持 `null`；其余情况下 `error_code=null`。CPU 只有使用率有意义，容量字段为空。

内存按物理内存统计：`used=total-available`，Linux `MemAvailable` 包含可回收缓存，不能当成原始 `MemFree`；内存 `free_bytes=null`。磁盘 `used=total-free`，`available` 为服务运行身份/配额下可用空间，两者分别展示。暂不包含 GPU、磁盘读写/IOPS、进程排名或远程机器。

历史 `data` 为 `{instance_id, sample_interval_ms, window_seconds, samples[]}`；每个样本含 `sequence`、`sampled_at`、`cpu`、`memory`。历史仅在内存中保留最近约 15 分钟，最多 450 条，重启清空。前端遇到实例变化或序列缺口重新拉取历史，禁止跨实例拼接曲线。

### 采集配置与平台

```yaml
metrics:
  diskPaths: []
```

空列表自动发现本地卷；配置绝对路径可限定监控范围（Linux 例如 `/`、`/data`；Windows 例如 `'C:\'`、`'D:\'`）。同一卷去重，最多 32 条，不探测网络或伪文件系统。Linux 目前支持 ext2/3/4、xfs、btrfs、zfs、overlay、f2fs、vfat、exfat、ntfs3；其他类型需显式扩展本地文件系统白名单。缺失/不支持的配置路径显示不可用而非成功零容量。

Linux 使用 `/proc/stat`、`/proc/meminfo`、挂载表与 `statvfs`；Windows 使用 PDH 英文 aggregate 计数器（覆盖处理器组）、`GlobalMemoryStatusEx`、`GetDiskFreeSpaceExW`，链接 Pdh。采集线程共享缓存，不随网页暂停而停止；磁盘采集可能受底层本地文件系统延迟影响。

### 前端与复用边界

入口 `/admin/system/performance`（Vue 内部路由 `/system/performance`），侧栏“机器性能”。每个指标/磁盘卷一张 Card，响应式平铺；CPU/内存使用 ECharts 趋势，磁盘使用容量环图。可选 5/15 分钟窗口、暂停/继续、展示过期/不可用状态。离页、登出或隐藏标签页取消轮询；恢复刷新补齐历史；不重叠请求，错误时有界退避。

`src/ui/lumina/components/LuminaCard.vue` 复用 LuminaPanel，提供 header/actions/default/footer/state 插槽与 title/subtitle/radius/padding/variant/state 属性。`LuminaChart.vue` 按需注册 ECharts 模块，SVG 渲染、ResizeObserver、自身销毁清理；主题、低特效和系统 reduced-motion 自动生效。这两个组件只依赖 Vue / ECharts / 同目录 UI，不依赖业务 API、Pinia、路由或 i18n，仍放独立目录，不构建独立 npm 库。

## 第二阶段：自定义权限管理

新增前端 `/admin/permissions`（内部 `/permissions`），接通已注册的 RBAC 接口；Swagger 同步包含角色与权限全部资源路径。

| 方法 | 路径 | 能力 |
| --- | --- | --- |
| GET / POST | `/api/v1/admin/permissions` | permission.read / permission.create |
| GET / PATCH / DELETE | `/api/v1/admin/permissions/{id}` | permission.read / permission.update / permission.archive |
| PATCH | `/api/v1/admin/permissions/{id}/status` | permission.status |
| GET | `/api/v1/admin/roles`、`/api/v1/admin/roles/{id}` | role.read |
| GET / PUT | `/api/v1/admin/roles/{id}/permissions` | role.read / role.permission.assign |

列表为 `{data:{items:[]},request_id}`，可选 `include_archived=true`。权限字段含 id/code/name/description/module/enforcement/source/status/required_for_super_admin/version，成功响应使用反射 DTO 与 iguana，不手工构造权限 JSON。

POST 接收 code/name/module/可选 description，返回 201 和 version=1；自定义权限固定 client/custom/active，不能指定 source/enforcement/status，不允许保留的 auth/user/role/permission/audit/behavior/system 前缀。PATCH 只接受 name/description。所有变更使用 If-Match；版本冲突 409/2209，重复 code（含归档）409/2228，未知字段/非法数据 422/2206，内置或不可恢复资源 409/2232。DELETE 返回 204；必须先解除全部角色关联，仍有关联返回 409/2230。

自定义权限不是新的服务端权限注册：业务客户端调用 `/client/me/permissions` 获取 capabilities 决定业务功能；服务端路由仍由代码注册权限保护。角色权限 PUT 提交完整 permission_ids 集合，前端可改动活跃自定义项并移除已有停用项，保留系统及未知关联；超管 required_for_super_admin 权限不允许移除，系统权限由受控服务端代码发布。启停权限或替换角色集合更新角色 authz_version，旧 access token 下次请求被拒绝，客户端需刷新/重新认证获取新授权。审计与业务变更在同一事务中提交；审计失败回滚写入。

第二阶段新增的错误码遵循 V1.2 设计，不改变原有用户/会话错误码：

| 码 | 名称 | HTTP | 含义 |
| --- | --- | --- | --- |
| 2228 | PERMISSION_CODE_CONFLICT | 409 | 权限代码已存在，包括归档占用 |
| 2230 | PERMISSION_IN_USE | 409 | 权限仍被角色引用，不能归档 |
| 2232 | PROTECTED_PERMISSION | 409 | 内置或已归档权限不可变更 |
| 2233 | SUPER_ADMIN_SECURITY_FLOOR | 409 | 尝试移除超管必需权限 |

## 角色管理与动态角色（2026-10-14）

角色管理位于 `/roles`，完整只读关联页位于 `/roles/permissions`。服务端角色接口均在 `/api/v1/admin` 下：

| 方法 | 路径 | 能力 |
| --- | --- | --- |
| GET / POST | `/roles` | role.read / role.create |
| GET / PATCH / DELETE | `/roles/{id}` | role.read / role.update / role.archive |
| PATCH | `/roles/{id}/status` | role.status |
| GET / PUT | `/roles/{id}/permissions` | role.read / role.permission.assign |
| GET / PUT | `/roles/{id}/manage-scopes` | role.read / role.scope.assign |

创建只接受 code/name/可选 description，code 为 `^[a-z][a-z0-9_]{1,63}$`；编辑只接受 name/description。code 不可更改或复用，重复（含归档）返回 `409 / 2227`。内置角色不可归档，super_admin 不可停用，已归档不可恢复或编辑（`409 / 2231`）；有用户引用时归档返回 `409 / 2229`。归档清除权限及双向管理范围并失效受影响管理者授权；审计失败整体回滚。

角色响应包含 id/code/name/description/system_key/status/version/authz_version，以及 user_count、permission_count、manageable。目录为 items 集合，可选 include_archived，不伪称已有分页/更新时间。写操作使用 If-Match 并重新授权。

权限 PUT 提交完整 permission_ids；不可新绑定停用项，已绑定停用项可以解绑；超管必需权限不可移除。范围 PUT 提交完整 target_role_ids；super_admin 必须覆盖全部非归档角色。安全底线返回 `409 / 2233`。范围不替代 user.* 能力，也不放开自我改角色或禁用限制。

创建角色会原子扩展 super_admin 范围、递增其授权版本；变更授权/状态/范围后旧 access token 失效。登录/刷新用户快照包含 role_id/role_authz_version，客户端据此清除旧能力缓存。停用或归档角色不能登录或刷新会话。`/client/me/permissions` 增加 manageable_roles 与 assignable_roles；用户筛选使用前者，创建/改角色选择后者。更新用户角色同时写 role 与 role_id。

新错误码：2227 ROLE_CODE_CONFLICT、2229 ROLE_IN_USE、2231 PROTECTED_ROLE，均为 HTTP 409。角色审计事件仍为 rbac.role.changed，目标类型/ID 存 details_json。
