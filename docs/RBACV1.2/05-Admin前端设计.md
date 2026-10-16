# RBAC V1.2 Admin 前端设计

## 1. 目标

在现有 Vue 3 + TypeScript Admin 中增加角色与权限管理，并移除“四种固定角色”的前端假设。前端负责可用性和操作引导，所有授权、目标范围和安全底线仍由后端最终判定。

## 2. 信息架构

建议新增导航：

~~~text
首页
用户管理
权限管理
  ├─ 角色
  └─ 权限
日志
修改密码
~~~

路由：

| 路由 | 页面 | 页面权限 |
|---|---|---|
| /roles | 角色列表 | role.read |
| /roles/new | 新建角色 | role.create |
| /roles/:id | 角色详情/编辑 | role.read |
| /permissions | 权限列表 | permission.read |
| /permissions/new | 新建自定义权限 | permission.create |
| /permissions/:id | 权限详情/编辑 | permission.read |

路由守卫使用 `auth.can(permissionCode)`，不再使用 `isAdmin(role)`。没有权限时隐藏导航；直接访问仍展示 403 页面，不能只重定向到首页造成误解。

## 3. 类型模型改造

现有：

~~~ts
export type Role = 'operator' | 'engineer' | 'admin' | 'super_admin'
~~~

改为：

~~~ts
export type RoleCode = string

export interface RoleOption {
  id: string
  code: RoleCode
  name: string
}

export interface User {
  id: string
  login_identifier: string
  identifier_type: 'username' | 'uid'
  role: RoleCode
  role_id: string
  role_name: string
  // 其余现有字段
}
~~~

删除或替换以下硬编码：

- `roleLabels: Record<Role, string>`；
- `manageableRoles()`；
- `isAdmin()`；
- 根据 operator/engineer/admin/super_admin 计算密码规则；
- 创建用户表单中 uid/username 两个互斥输入和 `uid ?? username` 展示逻辑；
- 用户筛选、创建和改角色表单中的固定 option；
- router 中 `meta.admin`；
- 首页中固定 capability 中文表作为唯一来源。

展示角色时：

~~~text
role_name 非空 -> 显示 role_name
否则 -> 显示 role code
~~~

已知系统权限可以保留本地中文翻译；未知自定义权限显示后端 name，最后回退 code。

## 4. 权限加载与路由守卫

登录/刷新后继续请求 `GET /client/me/permissions`。Store 增加：

~~~ts
can(code: string): boolean
canAny(codes: string[]): boolean
roleId: string
roleName: string
roleAuthzVersion: number
~~~

处理授权版本变化：

- API 返回 401 时沿用现有 refresh 流程；
- refresh 成功后重新加载 permissions；
- refresh 失败清理内存状态并回登录页；
- 403 不自动登出，展示稳定错误；
- 角色权限、角色范围或权限状态编辑成功后，如果影响当前用户角色，下一请求可能 401；新建角色也会自动扩展 super_admin scope 并影响当前 super_admin。前端应提示“权限已更新，请重新验证会话”，然后走 refresh；
- 不把 capability 列表持久化到 localStorage。

## 5. 角色列表

### 5.1 布局

- 顶部：标题、说明、“新建角色”按钮；
- 筛选：关键字、状态、系统/自定义；
- 表格：名称、code、类型、状态、用户数、权限数、更新时间、操作；
- 分页沿用现有列表组件和样式；
- archived 默认隐藏，筛选后可查看只读详情。

### 5.2 操作

| 操作 | 前置条件 | 说明 |
|---|---|---|
| 新建 | role.create | 进入新建页 |
| 编辑 | role.update | 修改名称/描述 |
| 配置权限 | role.permission.assign | 进入角色权限面板 |
| 配置范围 | role.scope.assign | 进入管理范围面板 |
| 启停 | role.status | protected 规则由 API 返回并在 UI 提示 |
| 归档 | role.archive | 需输入角色 code 二次确认 |

归档按钮在 `user_count > 0` 时禁用并展示原因；后端仍必须再次校验。

## 6. 角色编辑

建议使用详情页而不是大型弹窗，分为三个区域：

### 6.1 基本信息

- code：创建时可填写；编辑时只读；
- 名称：1–128 字符；
- 描述：最多 512 字符；
- 状态和内置标记；
- version 冲突后提示刷新，不自动覆盖。

### 6.2 权限配置

- 按 module 分组；
- 搜索 code/name/description；
- 标识系统/自定义、服务端/客户端、active/disabled；
- disabled 权限已关联时显示“已停用”，允许取消勾选但不能新勾选；
- super_admin 安全底线权限勾选框固定且显示保护原因；
- 保存时发送完整 permission_ids；
- 页面保留加载时 version，保存使用 If-Match；
- 未保存离开时弹出确认。

权限数量较多时使用虚拟列表或分页搜索，但最终提交完整集合。V1.2 上限 500，可先采用分组折叠和前端过滤，验证 500 项交互性能。

### 6.3 可管理角色

- 列出 active、非 archived 角色；
- 文案明确：“范围只决定可以管理哪些角色的用户，还需要 user.* 权限”；
- 允许选择自身角色，但提示不能修改自己的角色或禁用自己；
- super_admin 的必选范围不可取消；
- 保存使用全量 target_role_ids。

## 7. 权限列表与编辑

### 7.1 权限列表

筛选：

- 关键字；
- module；
- status；
- source；
- enforcement。

列：

- name/code；
- module；
- system/custom；
- server/client；
- status；
- 角色数；
- 更新时间；
- 操作。

页面固定显示说明：

> 在此创建的自定义权限用于客户端或业务模块消费，不会自动保护 SZX Admin 后端接口。

### 7.2 创建权限

字段：

- code：实时校验格式，创建后不可修改；
- name；
- description；
- module。

enforcement 和 source 不提供可编辑控件，页面只读说明“自定义 / Client”。

code 输入框还需提示并校验 SZX Admin 保留前缀（auth、user、role、permission、audit、behavior、system）；后端仍是最终校验方。

### 7.3 编辑权限

- code/source/enforcement/安全底线只读；
- name/description/module 可编辑；
- system 权限允许编辑展示信息；
- protected 权限不显示停用/归档入口；
- role_count > 0 时归档入口禁用；
- 归档需输入权限 code 二次确认。

## 8. 用户管理更新

### 8.1 登录标识表单

创建用户只显示：

- “登录账号”单一输入框，对应 `login_identifier`；
- “账号类型”选择，值为 username/uid，默认 username；
- “显示名称/昵称”输入框，对应 `display_name`。

界面文案必须说明：

- 账号类型只表示来源，不影响登录方式和大小写规则；
- 登录账号创建后不可修改；
- 登录账号不区分大小写且全系统唯一；
- 显示名称可重复、可修改，不能用于登录。

前端提交只使用新 DTO，不再发送 uid/username。读取 User 时优先使用 login_identifier/identifier_type；旧字段只用于兼容异常旧服务，不作为正常路径。

### 8.2 角色选项

进入用户页后请求 `GET /admin/user-role-options`。角色筛选、创建用户和变更角色共用该数据源。

- options 按 name/code 排序；
- code 只作为副文本；
- options 加载失败时禁用涉及角色的操作；
- 不能回退到固定四角色列表；
- 当前用户已有角色若后来 disabled，在详情中仍显示，但不能作为新分配选项。

### 8.3 用户表格

- 账号列显示 login_identifier，并以 username/UID 标签表示 identifier_type；
- 角色列显示 role_name，悬浮/副文本显示 code；
- 动态角色使用统一中性色 badge，不为每个 code 硬编码颜色；
- 内置角色可由 role_id/role metadata 决定是否采用特定图标，不能按名称猜测。

日志页面读取 `actor_login_identifier/user_login_identifier`；只有在连接旧服务且正式字段缺失时才回退 `actor_username/username`。页面统一称为“登录账号”，不称为“用户名”。

### 8.4 密码规则

当前前端按角色区分 6/8 字符规则。自定义角色无法安全继承该推断。V1.2 建议统一：

- 至少 8 个 Unicode 字符；
- 包含字母和数字；
- 创建、重置、改密使用同一规则；
- 服务端继续只接收 SHA-256 摘要时无法验证原始复杂度，因此前端规则属于用户体验，真实安全依赖临时凭据、强制改密、Argon2id 和限流。

若产品必须保留差异化密码规则，应单独设计 `password_policy_id`，不应继续按角色 code 分支；不纳入 V1.2。

## 9. 状态和错误处理

新增错误码中文映射：

| code | 文案建议 |
|---:|---|
| 2227 | 角色代码已存在或曾被使用 |
| 2228 | 权限代码已存在或曾被使用 |
| 2229 | 仍有用户使用此角色，无法归档 |
| 2230 | 仍有角色使用此权限，无法归档 |
| 2231 | 此内置角色受保护，不能执行该操作 |
| 2232 | 此系统权限受保护，不能执行该操作 |
| 2233 | 不能移除超级管理员的安全底线权限 |
| 2234 | 目标角色当前不可分配 |

错误展示统一包含 request_id 的可复制入口，便于与服务日志关联。不要显示后端下层诊断文本。

## 10. 可访问性与可用性

- 所有表单控件有 label 和错误关联；
- 权限树/分组支持键盘操作；
- 不能只用颜色表达 system/custom、active/disabled；
- 异步保存时禁用重复提交；
- destructive 操作使用明确名称和 code 二次确认；
- 大列表提供加载、空状态和错误状态；
- 403、409、422 区分展示；
- 响应式布局至少覆盖现有 Admin 支持宽度。

## 11. 前端测试

### 11.1 单元测试

- 动态角色展示和 fallback；
- 统一登录标识展示、账号类型标签和创建请求；
- 不允许新旧身份字段混发；
- capability 路由守卫；
- Role/Permission DTO 解析；
- code/name/description 校验；
- 权限集合差异和脏状态；
- 错误码映射；
- 401 refresh 后重新加载权限。

### 11.2 组件测试

- 500 权限分组选择；
- protected 项不可取消；
- disabled 已关联项可移除不可新增；
- If-Match 冲突展示；
- user-role-options 失败时禁止提交。

### 11.3 E2E

- super_admin 创建权限、创建角色、配置权限、创建该角色用户；
- 新用户登录只看到被授予菜单；
- 移除权限后旧会话失效，refresh 后菜单更新；
- admin 无 role.read 时看不到权限管理且直接访问 403；
- 角色有用户时不能归档；
- 最后一个超级管理员保护；
- 自定义中文角色名与 ASCII code 展示正确。

## 12. 文件影响

预计修改/新增：

~~~text
web/admin/src/api/types.ts
web/admin/src/api/admin.ts
web/admin/src/domain.ts
web/admin/src/router.ts
web/admin/src/stores/auth.ts
web/admin/src/composables/useUsersViewModel.ts
web/admin/src/pages/RolesPage.vue
web/admin/src/pages/RoleDetailPage.vue
web/admin/src/pages/PermissionsPage.vue
web/admin/src/pages/PermissionDetailPage.vue
web/admin/src/components/rbac/*
web/admin/tests/*
~~~

应复用现有页面布局、表格、表单、alert 和样式约定，不为 V1.2 重做 Admin 视觉系统。

## 本轮首切片（2026-10-14）

已新增 `/permissions` 独立业务页面（使用现有 Lumina 控件，不反向依赖 UI 目录）：权限目录、自定义权限 CRUD/启停/归档、角色自定义权限分配。授权入口/按钮按实际 capability 控制；列表只提供当前目录的本地关键词/来源筛选，不伪称接口已支持服务端分页。版本冲突必须显式重新加载。

本轮角色分配不提供内置 server 权限的删除操作，完整 PUT 时保留其 ID；已有停用的自定义权限允许解绑，但不能新增分配；后端仍强制保护超管必需集合。自定义角色页面和用户编辑器动态角色改造不属于此次首切片，后续按实施计划推进。

## 只读角色－权限关联页

独立查看页现位于 `/roles/permissions`，角色管理页和权限管理页提供入口，要求 `role.read`，不要求 `role.permission.assign`。Home 保持“当前账号权限”语义，不把其他角色的授权混入自己的 capabilities。

页面可选择任意接口返回角色（默认包括启用/停用，可勾选归档），读取完整关联集合，显示权限名称/代码/模块、内置或自定义、服务端或客户端、权限状态及当前是否生效。停用/归档角色的全部关联权限均标记不生效，权限本身停用也不生效；不会只展示可编辑的自定义子集。

完整权限详情要求 `permission.read`；仅有 `role.read` 时允许查看关联 ID，不请求权限目录，也不猜测未知权限的生效状态。权限目录读取失败时显示错误并降级到 ID，角色关联读取失败时清空旧数据并提供重试。查看页不发送写请求；实际变更继续使用权限管理中的受控分配流程。

每次选择/刷新按“角色版本 → 关联 ID → 当前权限目录 → 角色版本”读取，角色 version/authz_version 变化则拒绝拼接并提示重新加载。切换角色及离页取消旧请求，并用 epoch 防止适配器忽略取消时旧响应覆盖当前角色。只为当前选择角色加载关系，不逐角色发起 N+1 请求。

## 角色管理交付（2026-10-14）

`/roles` 已升级为角色管理目录，提供搜索、状态/来源过滤、包含归档、用户数/权限数，以及创建、资料编辑、启停、归档、完整权限集合和管理范围配置。复用 Lumina 表格及对话框，表格在工作区内部滚动；目录接口尚不提供分页或更新时间，因此不伪造这两个字段。

写入口同时检查 capability 和接口返回的 `manageable`；已归档角色只读，内置角色不可归档，super_admin 不可停用。归档在用户引用数大于零或未知时禁用，服务端再次检查；提交必须输入角色 code。权限集合保留未知已有 ID，允许解绑已停用权限，不允许新增停用权限，super_admin 必需权限固定。范围只控制目标角色，不替代 user.*；super_admin 的全部非归档角色范围固定。

编辑器读取前后校验角色版本，不发布拼接的旧快照；写请求带 If-Match。2209 必须显式重新加载，不自动覆盖。未保存修改在关闭和路由离开时通过主题对话框确认，浏览器刷新使用 beforeunload。保存后显式刷新会话和自身权限；登录/刷新 DTO 增加 role_id/role_authz_version，角色授权版本变化时失效旧 capability 缓存并重载。

用户筛选接入 `/client/me/permissions` 的 `manageable_roles`，编辑器使用其中启用的 `assignable_roles`，支持自定义 code；停用角色可筛选用户但不可新增绑定。未知角色显示 code，内置默认角色保留本地翻译。旧响应没有该字段时仅保留原内置目录的兼容回退。用户、审计、性能路由和导航按实际 capability 授权，不再要求固定 admin 名称；用户写操作分别按 user.* 控制。用户自己改角色和禁用自身的限制不变。六种界面语言均提供角色管理文案。

权限读取按会话授权代次合并与隔离：旧账号、旧用户授权版本或旧角色授权版本的响应/失败不可发布；读取自身触发 refresh 时转而等待当前代次请求。登录、登出和跨账号切换均不能复用旧在途结果。

日志页 `/logs` 要求 audit.read 或 behavior.read 任一能力，导航、守卫及会话更新使用同一规则；只渲染授权标签并默认选择可用标签，不向无权限接口发起新的列表读取。撤销某能力时清除对应表格与详情，忽略旧响应并切换到剩余标签；两种能力均失效时返回 Home。
