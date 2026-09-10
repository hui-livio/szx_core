# 单项目 RBAC RESTful API 接口文档

## 1. 通用约定

Base URL：`/api/v1`。认证头：`Authorization: Bearer <access_token>`。全站使用 HTTPS。成功使用 200/201/202/204；分页响应 `{items,total,page,page_size}`，`page_size` 服务端限制最大值；错误响应 `{code,message,details,request_id}`。服务端不得向客户端返回 `password_hash`、token 原文或私密密钥。

并发约定固定使用 `If-Match: <version>`；所有 PATCH/PUT 和会改变用户状态/密码的 POST 都必须携带该请求头。请求体不再接受可选的 version 字段。版本不一致统一返回 HTTP 409 和 `CONFLICT_VERSION`。

使用 refresh Cookie 的 POST 请求必须同时携带 `X-CSRF-Token`；CSRF token 不等同于 access/refresh token，不能写入日志。

## 2. 认证

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/auth/login` | 登录 |
| POST | `/auth/refresh` | 刷新令牌 |
| POST | `/auth/logout` | 登出 |
| POST | `/auth/change-password` | 修改密码 |

登录请求使用统一的 `identifier`，可匹配用户的 `uid` 或 `username`：

```json
{"identifier":"U10086","password":"<password>"}
```

登录成功返回当前用户、15 分钟 access token、access 过期时间和当前 `authz_version`；refresh token 通过 HttpOnly、Secure、SameSite Cookie 设置，不在 JSON 中返回。失败统一返回 `AUTH_INVALID_CREDENTIALS`，不区分用户不存在、禁用、锁定或密码错误。

`POST /auth/refresh` 只读取 refresh Cookie，成功后轮换 Cookie 并返回新的 access token；`POST /auth/logout` 撤销当前 refresh token family 并清除 Cookie。`POST /auth/change-password` 请求为 `{old_password,new_password}` 且携带当前用户 `If-Match`；首次改密允许使用一次性临时凭据作为 old_password，成功后清除 `force_password_change` 并递增 `authz_version`。

refresh token 每次使用后轮换；重放旧 token 时撤销整个 token family，并返回 `TOKEN_REPLAYED`。改密、角色变更、禁用和解锁后，旧会话按 `authz_version` 失效。

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

服务端生成一次性临时凭据并只在创建/重置响应中返回，明文不落库；凭据默认 24 小时内有效且只能成功使用一次。用户首次登录必须改密。若部署采用外部邀请通道，可用一次性 activation token 替代临时密码。

创建/重置成功响应只在本次返回临时凭据：

```json
{"data":{"user":{...},"temporary_credential":"<show-once>","temporary_credential_expires_at":"2026-09-11T00:00:00Z"}}
```

客户端不得缓存或再次展示 `temporary_credential`。

用户对象统一返回 `id、uid、username、display_name、role、status、force_password_change、version、authz_version、last_login_at、created_at、updated_at`；临时凭据过期时间不返回给普通用户；缺失的 uid 或 username 返回 `null`，不返回密码哈希。

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

`AUTH_REQUIRED`、`TOKEN_EXPIRED`、`TOKEN_REPLAYED`、`AUTH_INVALID_CREDENTIALS`、`FORBIDDEN`、`RESOURCE_NOT_FOUND`、`VALIDATION_ERROR`、`USER_IDENTITY_ONE_OF_REQUIRED`、`USER_IDENTITY_CONFLICT`、`CONFLICT_VERSION`、`LAST_SUPER_ADMIN_PROTECTED`、`RATE_LIMITED`、`AUDIT_WRITE_FAILED`。登录时 disabled/locked 统一映射为 `AUTH_INVALID_CREDENTIALS`，不单独暴露账户状态。

HTTP 映射固定为：401（未认证、过期或 refresh 失败）、403（权限/目标角色限制）、404（资源不存在）、409（版本冲突、最后超管保护或 refresh 重放）、422（参数校验失败）、429（限流）。错误 `details` 不得包含密码、token、密钥或内部堆栈。
