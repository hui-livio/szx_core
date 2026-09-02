# 单项目 RABC RESTful API 接口文档

## 1. 通用约定

Base URL：`/api/v1`。认证头：`Authorization: Bearer <access_token>`。成功使用 200/201/204；分页响应 `{items,total,page,page_size}`；错误响应 `{code,message,details,request_id}`。

## 2. 认证

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/auth/login` | 登录 |
| POST | `/auth/refresh` | 刷新令牌 |
| POST | `/auth/logout` | 登出 |
| POST | `/auth/change-password` | 修改密码 |

## 3. 用户管理

| 方法 | 路径 | 权限 | 说明 |
|---|---|---|---|
| GET | `/admin/users` | admin/super_admin | 用户列表 |
| POST | `/admin/users` | admin/super_admin | 创建用户 |
| GET | `/admin/users/{id}` | admin/super_admin | 用户详情 |
| PATCH | `/admin/users/{id}` | admin/super_admin | 编辑用户 |
| PATCH | `/admin/users/{id}/status` | admin/super_admin | 启用/禁用 |
| PUT | `/admin/users/{id}/role` | super_admin | 分配四类固定角色 |
| POST | `/admin/users/{id}/reset-password` | super_admin | 重置密码 |

创建用户请求必须且只能提供 `uid` 或 `username`：

```json
{"uid":"U10086","display_name":"Alice","role":"operator"}
```

同时提供或同时缺失返回 `USER_IDENTITY_ONE_OF_REQUIRED`；标识重复返回 `USER_IDENTITY_CONFLICT`。

## 4. Client 当前用户权限与行为日志

### 4.1 当前用户权限

`GET /client/me/permissions` 返回客户端业务判断所需的固定角色和能力码：

```json
{"data":{"role":"engineer","capabilities":["business.read","business.operate","engineering.configure"]}}
```

客户端据此控制菜单、按钮和流程分支；实际业务 API 仍必须在服务端按角色校验。

业务服务端自动记录 Client 行为；如需前端埋点，使用非可信采集接口：

```http
POST /client/behavior-events
Authorization: Bearer <access_token>
Content-Type: application/json

{"event_name":"report.view","result":"success","details":{"source":"dashboard"}}
```

返回 `202 Accepted` 和 `event_id`。Admin 查询：`GET /admin/user-logs`，需要 `admin` 或 `super_admin`。

## 5. 错误码

`AUTH_REQUIRED`、`TOKEN_EXPIRED`、`FORBIDDEN`、`RESOURCE_NOT_FOUND`、`VALIDATION_ERROR`、`USER_IDENTITY_ONE_OF_REQUIRED`、`USER_IDENTITY_CONFLICT`、`CONFLICT_VERSION`、`RATE_LIMITED`。
