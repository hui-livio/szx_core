# RBAC REST API 接口参考

更新日期：2026-09-17。本文按当前服务实现描述接口，采用“参数表 → 请求示例 → 响应模型 → 错误码”的参考格式。

## 1. 接口目录

**当前已实现：17 / 17 个接口。** 下表“已实现”表示后端已注册路由并实现处理逻辑，不代表对应前端页面已完成；OPTIONS 预检不单独计数。

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
| 审计 | GET | [/admin/user-logs](#user-logs) | 审计日志列表 | Bearer，admin/super_admin | 200 | 已实现 |
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

生产接入使用 HTTPS。szx_refresh 为 `HttpOnly; Secure`，默认有效期 7 天；Path 为 `${apiPrefix}/auth`（前缀为 `/` 时为 `/auth`）。`allowedOrigin="*"` 时使用 `SameSite=None`，配置单一 Origin 时使用 `SameSite=Strict`。刷新轮换 token，不延长当前会话族原到期时间。

浏览器登录、刷新、登出使用 `credentials: "include"`。允许的来源会收到具体 Access-Control-Allow-Origin、Access-Control-Allow-Credentials: true 和 Vary: Origin；来源不匹配返回 `403 / 2204`。无 Origin 的原生客户端可访问。用户和客户端接口沿用同一 Origin 策略。

| 预算 | 默认值 | 配置 | 作用范围 |
|---|---|---|---|
| 认证／凭据签发 | 每 IP 每分钟 30 次 | auth.attemptsPerMinute，1–1000 | 登录、改密、刷新、登出、创建用户、重置密码共享 |
| 行为事件 | 每用户每分钟 60 次 | auth.behaviorEventsPerMinute，1–1000 | 独立于认证预算 |

超限返回 `429 / 2211`。按实际 socket 对端 IP 计数，不信任 X-Forwarded-For。两类计数分别最多保留 4096 个未到期窗口，单进程内有效，多实例不共享。无 Cookie 的 logout 不计数；改密在 JWT 验证后计数；refresh/logout 在 Cookie 格式及 CSRF 校验后计数；创建／重置在事务鉴权后、生成凭据前计数。

## 3. 响应模型

### 3.1 User

登录、刷新、用户详情、列表元素和用户修改响应共用此模型。

| 字段 | 类型 | 返回规则 | 说明 |
|---|---|---|---|
| id | string(UUID) | 必有 | 稳定用户 ID；路径 UUID 字母不区分大小写 |
| uid | string | UID 账号才有 | UID 登录标识，区分大小写 |
| username | string | 用户名账号才有 | 展示值；匹配使用规范化后的小写值 |
| display_name | string | 必有 | 展示名 |
| role | string | 必有 | operator / engineer / admin / super_admin |
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

**成功：** 200，data 为 Session，与登录相同；Set-Cookie 替换 refresh token。客户端须替换 access token 和 CSRF token。

| HTTP / code | 条件 |
|---|---|
| 401 / 2200 | 缺失、非法、未知或普通已撤销 Cookie；账号不可用 |
| 401 / 2201 | refresh token 过期 |
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
| role | string | 否 | operator / engineer / admin / super_admin |
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
| role | string | 是 | 四种角色之一，且调用者有权创建 |

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

<a id="user-logs"></a>

### GET /admin/user-logs — 审计日志列表

**实现状态：已实现**

**权限：** admin/super_admin。

| Query 参数 | 类型 | 必填 | 默认／约束 |
|---|---|---|---|
| page | integer | 否 | 默认 1；范围 1–4294967295 |
| page_size | integer | 否 | 默认 20；范围 1–100 |
| from | string | 否 | 起始 UTC ISO 8601 时间，包含下界 |
| to | string | 否 | 结束 UTC ISO 8601 时间，包含该精确时刻上界 |
| event_name | string | 否 | 精确匹配；事件名称语法见行为上报，查询允许 auth./user. 前缀 |
| event_search | string | 否 | 事件代码或已配置中文名称的字面量子串，最多 128 个 Unicode 字符，无控制字符；空表示不过滤 |
| result | string | 否 | success / failure / denied |
| actor_user_id | string(UUID) | 否 | 操作者 ID |
| actor_search | string | 否 | 操作者展示名、用户名或 UID 的字面量子串，最多 128 个 Unicode 字符，无控制字符；与其他筛选条件取交集 |

按 created_at 降序、id 升序排列；不支持 sort，未知参数返回 `422 / 2206`。to 只有秒精度，不包含该精确时刻之后的微秒记录；其他错误见本节公共错误表。

管理界面使用 `event_search`，提交前去除首尾空白。例如 `login` 或 `auth.login` 可匹配 `auth.login.success` / `auth.login.failure`，`登录` 还匹配中文名为“退出登录”的 `auth.logout`。未知事件可按原始代码查询。`%`、`_`、反斜杠作为普通字符。匹配在服务端分页前完成，total 使用相同条件；与其他筛选条件取交集。保留 `event_name` 原有精确语义。

`actor_search` 在服务端关联 users 后，对 display_name、username、uid 三个字段进行匹配，分页与 total 均基于过滤后的日志。`%`、`_`、反斜杠作为普通字符匹配，大小写规则沿用数据库展示列的排序规则。`actor_user_id` 仅为已有 API 客户端保留兼容，管理界面不提供 UUID 搜索；二者同时提供时取交集。返回的用户名是查询时的登录标识，不是历史快照；无关联操作者的日志仍会出现在未按用户筛选的结果中。

```http
GET /api/v1/admin/user-logs?page=1&page_size=20&from=2026-09-17T00%3A00%3A00Z&event_name=report.view
Authorization: Bearer <access-token>
```

**成功：** 200，data 为 `Page<AuditLog>`。

```json
{
  "data": {
    "items": [{
      "id": "20000000-0000-4000-8000-000000000001",
      "actor_user_id": "10000000-0000-4000-8000-000000000002",
      "actor_username": "alice",
      "target_user_id": "10000000-0000-4000-8000-000000000002",
      "event_name": "report.view",
      "result": "success",
      "request_id": "report-view-001",
      "peer_address": "",
      "details": {"source": "dashboard"},
      "created_at": 1789636500510
    }],
    "total": 1,
    "page": 1,
    "page_size": 20
  },
  "request_id": "logs-list-001"
}
```

历史日志读取时也执行字段脱敏，但不会修改存储中的历史记录。业务 API 不支持修改或删除日志。

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
| admin | engineer 能力，加 user.read、user.create、user.update、user.status、user.unlock、user.password.reset、audit.read |
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

错误：`401 / 2200`（会话无效）、`403 / 2204`（Origin 不允许）、`422 / 2206`（字段／名称／details 非法）、`429 / 2211`（独立用户预算耗尽）、`500 / 2212`（审计失败）。

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

Admin 前端入口为 `/admin`（308 跳转 `/admin/`），与本文 API 前缀 `/api/v1` 独立；例如页面 `/admin/users` 调用接口 `/api/v1/admin/users`。将 `RBAC/szx_admin_web/dist` 内容部署到 `server.staticRoot/admin`，配置 `spaFallback: true`。完整构建、启动和故障排查见 [Admin 部署与使用指南](12-Admin部署与使用指南.md)。

启动前配置 auth.privateKeyPath 和 auth.publicKeyPath（PEM/DER，路径相对进程工作目录）；不自动生成临时密钥。当前使用 PS256／RSA-3072，issuer/audience 默认 szx-admin / szx-client。

```sh
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3072 -out jwt-private.pem
openssl pkey -in jwt-private.pem -pubout -out jwt-public.pem
```

密钥放在静态目录之外；重启加载相同密钥可保留未过期 token 的可验证性。也可用仓库 `szx_license_issuer keygen` 生成专用密钥对，不与许可证签发共用密钥。

服务端将客户端 SHA-256 摘要再经随机盐 Argon2id 哈希后保存，格式为 `sha256-v1:<Argon2id PHC>`。旧原密码 Argon2id 凭据不能直接转换；仅仍使用默认初始凭据且待改密的初始超管可在成功登录时自动升级，其他旧账号需明确重置密码。访问日志不记录 query、Authorization 或请求体，只保留 request ID、方法、路径和状态。
