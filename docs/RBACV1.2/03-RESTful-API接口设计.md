# RBAC V1.2 RESTful API 接口设计

## 1. 通用约定

- Base URL：`/api/v1`；
- Content-Type：`application/json; charset=utf-8`；
- 鉴权：`Authorization: Bearer <access-token>`；
- 写操作使用 `If-Match: <version>`，创建操作除外；
- 时间响应沿用现有 UTC Unix 毫秒格式；
- 分页沿用 `page`、`page_size`，page_size 最大 100；
- 成功响应沿用 `{"data": ...,"request_id":"..."}`；
- 错误响应沿用 `{"error":{"code":2206,"message":"..."},"request_id":"..."}`；
- 未知 Query、Body 字段一律 `422 / 2206`；
- code 在创建后不可修改，不提供 rename 接口；
- 归档接口使用 DELETE，但实现为软删除；
- 所有角色/权限写接口默认仅 super_admin 具备权限；授权依据仍为能力码而非角色名称。

## 2. 接口清单

### 2.1 新增接口

| 分类 | Method | Path | 需要权限 | 成功 |
|---|---|---|---|---:|
| 角色 | GET | /admin/roles | role.read | 200 |
| 角色 | POST | /admin/roles | role.create | 201 |
| 角色 | GET | /admin/roles/{id} | role.read | 200 |
| 角色 | PATCH | /admin/roles/{id} | role.update | 200 |
| 角色 | PATCH | /admin/roles/{id}/status | role.status | 200 |
| 角色 | DELETE | /admin/roles/{id} | role.archive | 204 |
| 角色 | PUT | /admin/roles/{id}/permissions | role.permission.assign | 200 |
| 角色 | PUT | /admin/roles/{id}/manage-scopes | role.scope.assign | 200 |
| 权限 | GET | /admin/permissions | permission.read | 200 |
| 权限 | POST | /admin/permissions | permission.create | 201 |
| 权限 | GET | /admin/permissions/{id} | permission.read | 200 |
| 权限 | PATCH | /admin/permissions/{id} | permission.update | 200 |
| 权限 | PATCH | /admin/permissions/{id}/status | permission.status | 200 |
| 权限 | DELETE | /admin/permissions/{id} | permission.archive | 204 |
| 用户 | GET | /admin/user-role-options | user.read | 200 |

### 2.2 现有接口更新

| 接口 | 兼容更新 |
|---|---|
| POST /admin/users | 新格式使用 login_identifier + identifier_type；兼容期仍接受旧 uid 或 username；role 从数据库校验 |
| GET /admin/users | search 统一搜索 login_identifier/display_name；role Query 接受动态 code |
| GET/PATCH 用户详情 | User 增加 login_identifier、identifier_type、role_id、role_name |
| PUT /admin/users/{id}/role | role 仍为 code；同时更新 role_id |
| 登录/刷新 User | 增加统一登录标识和动态角色字段 |
| GET /client/me/permissions | 增加角色元数据和 role_authz_version，capabilities 改为动态结果 |

## 3. 数据模型

### 3.1 RoleSummary

~~~json
{
  "id": "31000000-0000-4000-8000-000000000001",
  "code": "quality_inspector",
  "name": "质检员",
  "description": "查看检测结果并执行复核",
  "system_key": null,
  "status": "active",
  "protected": false,
  "version": 1,
  "authz_version": 1,
  "user_count": 12,
  "permission_count": 3,
  "created_at": 1790240400000,
  "updated_at": 1790240400000
}
~~~

`protected=true` 表示内置角色，不等同于该角色拥有全部权限。

### 3.2 RoleDetail

RoleDetail 包含 RoleSummary 全部字段，并增加：

~~~json
{
  "permissions": [
    {
      "id": "32000000-0000-4000-8000-000000000001",
      "code": "business.read",
      "name": "业务查看",
      "module": "business",
      "enforcement": "server",
      "source": "system",
      "status": "active",
      "required_for_super_admin": false
    }
  ],
  "manageable_roles": [
    {
      "id": "31000000-0000-4000-8000-000000000002",
      "code": "operator",
      "name": "操作员",
      "status": "active"
    }
  ]
}
~~~

### 3.3 Permission

~~~json
{
  "id": "32000000-0000-4000-8000-000000000099",
  "code": "report.export",
  "name": "导出报表",
  "description": "允许业务客户端导出检测报表",
  "module": "report",
  "enforcement": "client",
  "source": "custom",
  "status": "active",
  "required_for_super_admin": false,
  "protected": false,
  "version": 1,
  "role_count": 2,
  "created_at": 1790240400000,
  "updated_at": 1790240400000
}
~~~

### 3.4 User 兼容扩展

~~~json
{
  "id": "10000000-0000-4000-8000-000000000002",
  "login_identifier": "U10086",
  "identifier_type": "uid",
  "uid": "U10086",
  "display_name": "Alice",
  "role": "quality_inspector",
  "role_id": "31000000-0000-4000-8000-000000000001",
  "role_name": "质检员",
  "status": "active",
  "version": 2,
  "authz_version": 2
}
~~~

正式字段：

| 字段 | 说明 |
|---|---|
| login_identifier | 唯一登录标识展示值 |
| identifier_type | username/uid 来源标签 |
| display_name | 昵称/展示名称，不参与登录和唯一性 |

`/api/v1` 兼容字段：

- `identifier_type=uid` 时额外返回 `uid=login_identifier`，省略 `username`；
- `identifier_type=username` 时额外返回 `username=login_identifier`，省略 `uid`；
- 兼容字段由 API 动态生成，不对应数据库物理列；
- 新客户端必须读取正式字段，不应继续通过 `uid ?? username` 恢复登录标识。

现有 `role` 字段保持为角色 code。客户端展示必须优先使用 `role_name`，不能继续把 code 映射为四个固定中文名称。

## 4. 角色接口

### 4.1 GET /admin/roles

Query：

| 参数 | 约束 |
|---|---|
| page/page_size | 默认 1/20，page_size 1–100 |
| search | 最多 128 Unicode 字符，搜索 code/name/description |
| status | active/disabled/archived；默认不含 archived |
| system | all/system/custom，默认 all |
| sort | code、-code、name、-name、created_at、-created_at、user_count、-user_count |

成功：`Page<RoleSummary>`。

### 4.2 POST /admin/roles

请求：

~~~json
{
  "code": "quality_inspector",
  "name": "质检员",
  "description": "查看检测结果并执行复核",
  "permission_ids": [
    "32000000-0000-4000-8000-000000000001"
  ],
  "manageable_role_ids": []
}
~~~

规则：

- `code`、`name` 必填；
- `permission_ids`、`manageable_role_ids` 可省略，默认空；
- 数组最大分别 500/100，必须为无重复 UUID；
- 只能关联 active、未 archived 的资源；
- 调用者需具备 role.create；若初始数组非空，还分别需要 role.permission.assign / role.scope.assign；
- 创建结果为 active；
- code 冲突返回 `409 / 2227`。

成功：201，data 为 RoleDetail。

### 4.3 GET /admin/roles/{id}

成功：200，data 为 RoleDetail。不存在或已按默认策略隐藏的资源返回 `404 / 2205`。

### 4.4 PATCH /admin/roles/{id}

Header：`If-Match` 必填。

请求至少包含一个字段：

~~~json
{
  "name": "高级质检员",
  "description": "查看、复核并标记检测结果"
}
~~~

只允许 name、description；不接受 code、system_key、permissions、status。实际值变化才递增 version，不递增 authz_version。

### 4.5 PATCH /admin/roles/{id}/status

~~~json
{"status":"disabled"}
~~~

只接受 active/disabled：

- super_admin 不允许 disabled：`409 / 2231`；
- 实际变化时递增 version/authz_version；
- disabled 后该角色用户的 access 和 refresh 请求均失败；
- 恢复 active 不自动恢复已过期 token，用户需重新登录或使用仍有效 refresh token；
- 提交原状态为幂等成功，不递增版本。

### 4.6 DELETE /admin/roles/{id}

Header：`If-Match` 必填，无请求体。

规则：

- 内置角色返回 `409 / 2231`；
- 有任何用户引用返回 `409 / 2229`；
- Service 在事务中删除 role_permissions 和 role_manage_scopes 关系，再将角色标记 archived；
- 已 archived 再次请求返回 `404 / 2205`；
- 成功返回 204，无 body。

### 4.7 PUT /admin/roles/{id}/permissions

使用全量替换而不是增量 PATCH，避免并发下客户端误判最终集合。

~~~json
{
  "permission_ids": [
    "32000000-0000-4000-8000-000000000001",
    "32000000-0000-4000-8000-000000000099"
  ]
}
~~~

规则：

- Header `If-Match` 为角色 version；
- permission_ids 必填，允许空数组，最多 500；
- 目标角色和权限必须非 archived；
- disabled 权限不能加入；已存在但后来 disabled 的关系可以在本次保留或移除；
- super_admin 的 required_for_super_admin 权限不可缺失，否则 `409 / 2233`；
- 实际集合变化才递增 role version/authz_version；
- 成功返回更新后的 RoleDetail。

### 4.8 PUT /admin/roles/{id}/manage-scopes

~~~json
{
  "target_role_ids": [
    "31000000-0000-4000-8000-000000000002"
  ]
}
~~~

规则：

- Header `If-Match` 为角色 version；
- target_role_ids 必填，允许空数组，最多 100；
- 目标角色必须未 archived；disabled 角色允许保留或加入范围，以便授权管理员查看其用户并在恢复角色后继续管理；
- 允许包含自身角色，但用户自操作限制仍生效；
- super_admin 必须保留对所有非 archived 角色的管理范围；服务端创建新角色时自动为 super_admin 增加该 scope；
- 实际集合变化才递增 role version/authz_version。

创建新角色时，自动扩展 super_admin scope 同样会递增内置 super_admin 角色的 version/authz_version；创建请求成功后，当前 super_admin 的下一次请求可能需要 refresh。

## 5. 权限接口

### 5.1 GET /admin/permissions

Query：

| 参数 | 约束 |
|---|---|
| page/page_size | 默认 1/20，最大 100 |
| search | 搜索 code/name/description |
| module | 精确模块过滤 |
| status | active/disabled/archived；默认不含 archived |
| source | all/system/custom |
| enforcement | server/client |
| sort | code、-code、module、-module、created_at、-created_at、role_count、-role_count |

成功：`Page<Permission>`。

### 5.2 POST /admin/permissions

~~~json
{
  "code": "report.export",
  "name": "导出报表",
  "description": "允许业务客户端导出检测报表",
  "module": "report"
}
~~~

服务端固定写入 `enforcement=client`、`source=custom`、`status=active`、`required_for_super_admin=false`。

客户端若提交 enforcement、source、status 或安全底线字段，返回 `422 / 2206`。code 不得以 `auth.`、`user.`、`role.`、`permission.`、`audit.`、`behavior.`、`system.` 开头，违反时返回 `422 / 2206`。code 冲突返回 `409 / 2228`。成功：201。

### 5.3 GET /admin/permissions/{id}

成功：200，data 为 Permission。

### 5.4 PATCH /admin/permissions/{id}

Header `If-Match` 必填。只允许 name、description、module：

~~~json
{
  "name": "导出质检报表",
  "module": "quality_report"
}
~~~

system 权限同样允许修改显示名称、描述和模块分类；code、source、enforcement、安全底线标记不可修改。实际变化才递增 version。

### 5.5 PATCH /admin/permissions/{id}/status

~~~json
{"status":"disabled"}
~~~

规则：

- 只接受 active/disabled；
- `required_for_super_admin=true` 不允许 disabled，返回 `409 / 2232`；
- disabled 时保留 role_permissions，但权限不再生效；
- active/disabled 任一方向的实际变化，都在同一事务递增所有关联角色的 version/authz_version；
- 单权限最大关联角色数限制为 1000；超过该值拒绝并要求专用运维迁移，避免超大事务；
- 成功返回 Permission。

### 5.6 DELETE /admin/permissions/{id}

Header `If-Match` 必填。

- system 或安全底线权限返回 `409 / 2232`；
- 仍被任何角色引用返回 `409 / 2230`；
- 自定义、无引用权限可归档；
- 成功 204。

## 6. 用户角色选项

### GET /admin/user-role-options

用于用户列表筛选、创建和角色分配，不要求调用者拥有 `role.read`，但要求 `user.read`。

响应只包含调用者 manage scope 内 active 角色：

~~~json
{
  "data": {
    "items": [
      {
        "id": "31000000-0000-4000-8000-000000000002",
        "code": "operator",
        "name": "操作员"
      },
      {
        "id": "31000000-0000-4000-8000-000000000001",
        "code": "quality_inspector",
        "name": "质检员"
      }
    ]
  },
  "request_id": "role-options-001"
}
~~~

该接口不分页，最大返回 100 个角色，按 name、code 排序。超过 100 个可管理角色时 readiness 报告配置超限，V1.2 不支持继续增长。

## 7. 现有用户接口规则更新

### 7.1 创建用户

推荐请求：

~~~json
{
  "login_identifier": "U10086",
  "identifier_type": "uid",
  "display_name": "Alice",
  "role": "quality_inspector"
}
~~~

字段规则：

| 字段 | 必填 | 约束 |
|---|---:|---|
| login_identifier | 是 | 原始 UTF-8 最多 512 字节；NFC 后 1–64 Unicode 字符；case-fold canonical 最多 128 字符 |
| identifier_type | 是 | username / uid；只表示来源 |
| display_name | 是 | 1–128 Unicode 字符，无控制字符 |
| role | 是 | active 且在调用者 manage scope 内的角色 code |
| password_hash | 否 | 沿用现有 64 位小写 SHA-256 摘要规则 |

`login_identifier` 无论类型均执行 trim + Unicode NFC + locale-independent case fold，使用统一唯一命名空间。`identifier_type` 不改变匹配和唯一性规则。登录标识创建后不可修改，display_name 仍可通过现有用户更新接口修改。

兼容期内，`POST /admin/users` 还接受第一阶段请求：

~~~json
{"uid":"U10086","display_name":"Alice","role":"quality_inspector"}
~~~

或：

~~~json
{"username":"alice","display_name":"Alice","role":"quality_inspector"}
~~~

兼容规则：

- 旧请求中的 uid 映射为 `login_identifier + identifier_type=uid`；
- 旧请求中的 username 映射为 `login_identifier + identifier_type=username`；
- 新字段与 uid/username 不能混用，uid 和 username 仍不能同时出现；
- 旧格式互斥错误继续返回 `422 / 2207`；
- 新格式缺字段、混用或类型非法返回 `422 / 2206`；
- canonical 冲突继续返回 `409 / 2208`，不泄露冲突账号类型；
- 响应 Header 增加 `Deprecation: true` 和指向 V1.2 文档的 `Link`；服务访问日志按聚合计数记录旧格式使用，不记录标识原文；
- 旧格式移除日期必须在后续破坏性 API 版本中单独公告，V1.2 不直接删除。

后端按 role code 查找 active 角色，并同时检查调用者具备 `user.create`、manage scope 包含目标角色且目标角色未 archived。未知 code 使用 `404 / 2205`；disabled/archived 角色使用 `409 / 2234`。

### 7.2 用户角色变更

`PUT /admin/users/{id}/role` 请求保持不变：

~~~json
{"role":"quality_inspector"}
~~~

调用者必须同时能管理目标用户当前角色和新角色，避免“可管理低权限用户，却把其转交到不可管理高权限角色”的越权。

### 7.3 用户列表

- role Query 由四值 enum 改为动态 code；
- search 统一匹配 `login_identifier` 和 `display_name`，不再分别搜索 uid/username；
- 结果范围为调用者 manage scope 和显式 role filter 的交集；
- sort=role 按角色 code 排序，保持兼容；新增 sort=role_name；
- 不在范围内的显式角色仍返回 `403 / 2204`，不泄露用户数量。

### 7.4 审计与行为事件账号字段

安全审计和行为事件查询正式增加：

| 新字段 | 说明 | V1 兼容字段 |
|---|---|---|
| actor_login_identifier | 安全审计操作者登录标识 | actor_username |
| user_login_identifier | 行为事件用户登录标识 | username |

兼容字段继续返回与新字段相同的字符串，但名称仅为历史兼容，不表示账号一定是 username 类型。新前端和新客户端必须读取正式字段。搜索参数 `actor_search/user_search` 统一匹配 `display_name + login_identifier`。

## 8. 当前用户权限接口更新

### GET /client/me/permissions

响应：

~~~json
{
  "data": {
    "user_id": "10000000-0000-4000-8000-000000000002",
    "role": "quality_inspector",
    "role_id": "31000000-0000-4000-8000-000000000001",
    "role_name": "质检员",
    "status": "active",
    "force_password_change": false,
    "authz_version": 3,
    "role_authz_version": 7,
    "capabilities": [
      "business.read",
      "report.export"
    ]
  },
  "request_id": "permissions-002"
}
~~~

`authz_version` 继续表示用户授权版本，保持旧客户端兼容。capabilities 只返回 active 权限，按 code 升序，数组去重。

## 9. 新错误码

现有 2200–2226 和 2299 保持数值与含义不变。V1.2 在 admin 模块同一稳定错误域中追加：

- `2207 USER_IDENTITY_ONE_OF_REQUIRED` 保留，只供旧 uid/username 请求兼容校验使用，不用于新 DTO；
- `2208 USER_IDENTITY_CONFLICT` 继续表示统一 canonical 登录标识冲突，不区分来源类型；

| code | 名称 | HTTP | 触发 |
|---:|---|---:|---|
| 2227 | ROLE_CODE_CONFLICT | 409 | 角色 code 已存在或已归档占用 |
| 2228 | PERMISSION_CODE_CONFLICT | 409 | 权限 code 已存在或已归档占用 |
| 2229 | ROLE_IN_USE | 409 | 角色仍被用户引用 |
| 2230 | PERMISSION_IN_USE | 409 | 权限仍被角色引用 |
| 2231 | PROTECTED_ROLE | 409 | 修改内置角色受保护属性或状态 |
| 2232 | PROTECTED_PERMISSION | 409 | 修改系统/底线权限受保护属性或状态 |
| 2233 | SUPER_ADMIN_SECURITY_FLOOR | 409 | 尝试移除超管安全底线 |
| 2234 | ROLE_NOT_ASSIGNABLE | 409 | 目标角色 disabled/archived |

数据库错误、异常文本或下层错误码不能穿过 API 边界。唯一键冲突必须在 Repository/Service 边界映射为 2227/2228；其他数据库失败继续映射为 2220–2222。

## 10. 安全审计事件

| 事件 | 结果 | 关键 details |
|---|---|---|
| role.created | success/failure/denied | role_id、code、初始权限数 |
| role.updated | success/failure/denied | changed_fields |
| role.status.changed | success/failure/denied | before/after |
| role.archived | success/failure/denied | role_id、code |
| role.permissions.changed | success/failure/denied | added_codes、removed_codes、authz_version |
| role.scopes.changed | success/failure/denied | added_role_codes、removed_role_codes |
| permission.created | success/failure/denied | permission_id、code |
| permission.updated | success/failure/denied | changed_fields |
| permission.status.changed | success/failure/denied | before/after、affected_role_count |
| permission.archived | success/failure/denied | permission_id、code |

审计中不保存完整请求体；数组最多记录 100 个 code，超出时记录 count 和截断标记。

## 11. OpenAPI 更新要求

`tools/szx_admin/web/api-docs/openapi.json` 必须同步：

- 新增 15 个 operation；
- 新增 RoleSummary、RoleDetail、Permission、RoleOption schema；
- Role 从四值 enum 改为 pattern string；
- 新增 LoginIdentifierType，User 增加 login_identifier/identifier_type/动态角色字段；
- CreateUser schema 以新格式为主，并用兼容 schema 描述旧 uid/username 格式及互斥规则；
- 标记 uid/username 为 deprecated，示例优先使用新格式；
- 新增错误码示例；
- 所有写接口声明 If-Match；
- DELETE 204 不声明 JSON body；
- operationId 保持唯一并用于前端类型生成；
- Swagger 页面说明“自定义权限不会自动保护服务端接口”。

发布前使用 OpenAPI 校验器验证引用、示例和状态码，并运行前端 API 类型检查。
