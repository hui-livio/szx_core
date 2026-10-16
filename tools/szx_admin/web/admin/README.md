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

Vue 组件不直接调用 HTTP 或实现业务操作。新增功能时将业务行为加入对应 ViewModel，展示组件通过 props/model 输入和 emits 输出。弹窗的焦点管理、Escape 关闭和遮罩由 PrimeVue Dialog 处理；保存期间禁止关闭。

## UI 组件与主题

主题与通用控件集中在独立目录 `src/ui/lumina/`，当前仍是应用内源码模块，**没有独立 npm 包、工作区包管理或单独的库构建**。后续可在边界稳定后再抽为 UI 库。

```text
src/ui/lumina/
├── components/   # 按钮、输入、下拉、表格、弹窗、日期时间、分页、背景、通用背景面板、共享标记、主题/性能模式开关
├── composables/  # 主题连续缩放
├── theme/        # Aura preset、主题安装、通用日历 locale、CSS 与字体资源
├── utils/        # 本地日期时间格式化/校验；不包含 UTC 查询规则
├── index.ts      # TypeScript / Vue 控件的统一导出入口
├── styles.css    # 样式入口（调用方显式引入）
└── README.md     # 控件与接入示例、边界和验证方式
```

后台组件通过统一入口使用 `LuminaButton`、`LuminaInput`、`LuminaSelect`、`LuminaTable`、`LuminaColumn` 等，不直接导入 PrimeVue。`ModalDialog`、`PaginationBar`、`LocalDateTimePicker` 仅是业务文案适配层；权限、角色、API、路由、会话和右键策略仍归应用所有。UI 目录不导入应用的 `domain`、ViewModel、Pinia 或 vue-i18n。

- `src/ui/lumina/theme/aura.ts`：映射 Aura token 到现有 `--lg-*` CSS 变量。
- `src/ui/lumina/theme/glass/`：光晕、玻璃视觉及字体资源，字体许可证随资源保留。
- `src/ui/lumina/theme/tokens.css`、`controls.css`：通用表面色、低特效规则、控件状态与圆角/阴影。
- `src/styles/layout.css`、`data.css` 和 `src/style.css`：仅保留后台布局及角色标签、表格操作等应用样式。

CSS 层顺序保持 `lumina → primevue → app`。安装使用 `createLuminaUi()`，组件按需导入，样式显式引入；详细说明见 [Lumina UI 接入说明](src/ui/lumina/README.md)。依赖仍固定为 PrimeVue `4.5.5`、`@primeuix/themes` `2.0.3`，未新增依赖。

主题仍使用 `<html data-theme="light|dark" data-fx="lite|full">`，默认低特效和 Ripple 关闭（完整动效保留粒子与光晕），尊重系统减少动态效果偏好。缩放由 `useLuminaScale()` 管理。日期时间保持 24 小时制、秒精度和本地字符串模型，后台继续通过 `toUtcSeconds` 生成 UTC 查询参数。主操作按钮统一使用 `variant="primary"`。品牌 S 和用户头像复用 `LuminaMark`，统一蓝紫渐变底色与白色前景，支持独立配置尺寸与圆角；暗色 hover/active 使用专属语义色，不出现浅色块。圆形开关按物理像素对齐并使用柔和 SVG 描线。

开发中修改 `src/ui/lumina/theme/` preset 时完整刷新页面，重新初始化样式注册表；会话不持久化，需要重新登录。登录页和顶部栏均提供紧凑的语言入口、性能模式与深色主题开关；两处共享 `ui-preferences` Pinia store，登录、退出及组件重建不会重置状态，开发热更新也保留当前选择；主题/特效偏好分别保存到 `szx-admin-theme` 和 `szx-admin-effects`，可选自定义主题色保存到 `szx-admin-brand`，只在用户切换时写入，默认浅色 + 低特效。登录卡片、首页、用户/权限/角色管理、日志、修改密码及 404 页面统一使用通用 `LuminaPanel` 的 glass 材质；性能指标 `LuminaCard` 显式使用相同 glass 材质，均响应深浅色及性能开关。浮层、弹窗与粘性表头仍保留不透明语义表面保证可读性；玻璃模式采用半透明表面与高光模拟，不执行背景模糊采样；切换不改变内容布局。浏览器禁用存储时仍可切换当前页面，系统减少动态效果偏好始终优先。验证用的独立页面位于 `tests/fixtures/lumina/`，不接入后台路由、Pinia 或 API，不进入生产构建。

## 页面和行为

页面全局禁用浏览器原生右键菜单，包括通过 Teleport 挂载的弹窗和下拉浮层；不影响其他鼠标操作及键盘导航。不提供“跳到主要内容”入口。

- `/login`：UID/用户名登录，原始密码 UTF-8 SHA-256 摘要提交。
- `/change-password`：超管首次强制改密；其他角色可跳过建议；成功后重新登录。前端按角色校验：操作员/技术员（engineer，界面显示工程师）至少 6 个字符，可使用纯数字或纯字母；管理员/超级管理员至少 8 个字符且包含字母和数字。
- `/users`：搜索、筛选、排序、分页、详情、创建、展示名编辑、启停、解锁、重置密码、操作员和工程师角色分配。
- `/logs`：按事件、结果、用户及本地时间区间查询；事件名称支持中文或事件代码关键词（例如“登录”、`login`、`auth.login`），使用后端 `event_search`，需要更新后端。“搜索用户”同时匹配展示名、用户名和 UID，不匹配内部 UUID。列表显示用户名（UID 账号显示 UID）；搜索与分页均由后端执行。请求时间转为 UTC 秒精度，详情脱敏展示。
- `/home`：当前账号与后端返回的权限能力。正常登录后四种角色默认进入此页；强制改密和密码推荐流程优先处理，跳过推荐后也回到此页。业务操作在对应业务客户端进行。

管理员只管理操作员/工程师；超管管理四种角色。前端限制用于交互，服务端仍是权限判定方。用户更新带 `If-Match`；2209 冲突后必须重新加载并确认，不自动覆盖。

任何用户都不能给自己分配角色或禁用自己；超出管理范围的用户操作按钮不可用，后端同样拒绝。

Access/CSRF token 仅保存在 API 客户端闭包内，refresh token 由 HttpOnly Cookie 管理。会话没有 localStorage/sessionStorage 持久化（语言、主题和特效仅保存视觉偏好），刷新页面后需要重新登录；当前也不做跨标签页会话同步。同一页面并发刷新合并为一次。429/5xx 保留会话并提示手动重试；失败的写请求不自动重复执行，只有明确的 access 鉴权失败会刷新后重试一次。退出仅在服务端成功返回后清除内存会话。

发请求前，access token 已过期或剩余有效期不超过 5 秒时，先调用 `POST /auth/refresh`；收到 `401 / 2200` 或 `401 / 2201` 时也会刷新并最多重试一次。刷新使用 HttpOnly Cookie 和 `X-CSRF-Token`，服务端轮换 refresh token。页面闲置时不定时刷新；默认连续空闲超过 30 分钟后，恢复操作会因 refresh 空闲过期而要求重新登录。成功刷新会重置空闲窗口，但不会延长默认 7 天的会话族绝对期限。

创建/重置返回的临时密码仅在当前弹窗展示，关闭或离开页面立即清除，不写入 Pinia 或浏览器存储。不要将它复制到日志或工单中。

重置密码也可手动填写新密码并确认，按目标用户角色校验；留空则随机生成。指定密码不回显，重置自己的密码后返回登录页；随机重置时先展示临时密码，确认记录后再退出。两种方式均使旧密码和已有会话失效，需要后端支持重置请求的 `password_hash` 字段。

创建用户可填写初始密码和确认密码（操作员/技术员至少 6 个字符，可用简单密码；管理员/超级管理员至少 8 个字符，包含字母和数字），以 SHA-256 摘要提交；留空仍自动生成临时密码。指定密码不会回显，也没有临时密码的 24 小时有效期；首次登录改密策略不变。成功、取消或离开页面清空密码。此功能需要支持创建请求 `password_hash` 字段的新版后端。

登录成功后立即撤下登录页面并切换到后台；目标页面资源加载期间显示加载提示，页面数据在挂载后独立加载，接口延迟或失败不让登录表单出现在后台工作区。首次强制改密流程继续优先进入改密页。

## 界面语言

支持简体中文、日语、韩语、英语、法语和俄语。英语仅提供一个 `English` 选项，内部统一使用 `en-US`，不区分 US/UK。已有的 `en-GB` 浏览器语言或本地语言偏好会自动匹配到该英语选项；切换语言后保存统一语言码。

## 页面主题

全站默认保留 Lumina Glass 的靛紫渐变、光晕、玻璃质感和字体令牌，主题及字体从 `/mnt/d/share/lumina-glass` 引入并保存在 `src/ui/lumina/theme/glass/`，构建与部署不依赖该外部目录。字体许可证与归属说明随字体一起保留。

入口 HTML 默认设置 `data-theme="light"`、`data-fx="lite"`，内联引导脚本在首帧前恢复已保存的深浅色及动效档位；无偏好时关闭动画、过渡、平滑滚动与毛玻璃，保留静态主题视觉；系统减少动态效果偏好同样禁用动画与过渡。后台布局适配手机与桌面，并沿用主题的连续比例 `--k`（限制为 0.92–1.5）；管理页面需要正常滚动，因此不使用演示登录卡片的高度压缩约束。

业务组件复用原有样式类，通用控件适配集中在 `src/ui/lumina/`，后台布局保留在 `src/style.css` 和 `src/styles/layout.css`。后台工作区固定在视口内，顶部栏和页脚保持可见；用户管理、安全审计和用户行为页面仅在数据表格内滚动，表头和分页保持可见。窄屏与较矮窗口限制筛选区域高度并允许其单独滚动，底部采用紧凑间距，为表格保留更多空间。各子工作区移除主标题上方的重复小标题，并采用统一的紧凑顶部间距。

窄屏（视口宽度不超过 720px）将语言、外观和账号操作合并到顶部品牌区右侧，导航保留在下方；空间不足时操作区自动换行。语言入口在所有屏幕尺寸统一使用圆形图标按钮，点击展开语言选项，支持键盘选择与 Esc 关闭。宽屏仍保留独立顶部栏。账号头像显式启用按钮模式，点击后弹出“修改密码 / 退出”菜单；用户名与角色仍直接显示，强制改密时菜单只提供退出。账号菜单和共享下拉选项面板均带小尺寸吸附三角，随弹层上下位置及屏幕边缘避让对齐触发控件。

所有业务弹窗统一复用公共 `LuminaDialog`，由 `LuminaPanel` 提供内容面板。Esc 与关闭按钮走同一关闭请求，实时检查忙碌状态；打开时正在加载的弹窗在加载结束后也能用 Esc 关闭。嵌套时每次只关闭最上层，存在下拉、日历或菜单时优先关闭子弹层，再按 Esc 关闭父框。关闭后恢复触发按钮焦点，保存中的关闭保护与未保存修改确认继续生效。

表格和图表在公共控件层跳过内容未变的更新：表格复用等价的行数组及未变行，图表比较 option、回调身份和有效外观后再提交 `setOption`。机器性能页按磁盘 ID 与图表实际使用的容量值缓存 option，CPU/内存轮询不会重复更新未变的磁盘饼图；语言、主题和容量变化仍会更新。数字与坐标轴时间格式化器按语言复用。调用方主动清空表格时不保留历史行，避免越过权限及查询边界。

### 背景动效与低占用策略

参考 `/mnt/d/share/client` 的样式更新，但不整目录覆盖：保留 admin 的默认主题色、多语言、权限/接口契约、表格稳定行、图表更新快照及本地字体许可证。侧栏移除重复品牌说明与不代表真实连接状态的绿点。缩放保留原有 0.92–1.5 可读性下限，增加 rAF 合并、DPR 与 visualViewport 变化处理；卡片宽度阈值与 `--k` 同比例缩放。

- 完整动效包含 3 个漂浮光晕、桌面最多 8 个上升粒子、窄屏最多 5 个粒子；精细指针设备提供鼠标跟随光晕。
- 全部背景共用一个最高 30 FPS 的时钟（定时器 + rAF），不改变业务 UI 的刷新率。每帧只写 `transform` / `opacity`，不触发 Vue 响应式渲染，也不读取布局；慢帧直接按时间定位，不补帧。
- 光球最长边不超过 480 CSS px / 768 物理像素；跟随光晕不超过 280 CSS px / 448 物理像素。使用静态径向渐变，不使用全屏噪声滤镜、运动缩放、粒子 blur/box-shadow 或 canvas 渲染循环。
- 低特效、后台标签页和系统减少动态效果均停止计时器/rAF、移除指针监听、释放 `will-change` 提示并隐藏粒子/跟随光晕，只留静态背景。卸载清理全部观察器、媒体查询和事件订阅。
- `LuminaPanel` 使用共享的 `--lg-panel-background` / `--lg-panel-highlight` 令牌，按主题与动效档位变化；高光合并到背景，不单建覆盖内容的伪元素层。表格普通行与 table 背景透明以露出面板，hover、选中态、粘性表头及浮层继续保持语义表面；完整模式也不进行移动背景的 `backdrop-filter` 采样。

主题色不会自动改成参考 client 的蓝青色。登录页和顶栏的调色板入口支持自定义主色、明暗模式共享、刷新恢复及一键恢复默认；空偏好不会覆盖出厂 token。明亮自定义色自动匹配深色前景。色阶通过 UI 模块的 `applyLuminaBrand()` 生成，业务页面继续复用 PrimeVue/Lumina token，不引入新的组件库。

背景专项验证位于 `tests/e2e/background.spec.ts`、`tests/ui/background.test.ts` 与 `tests/ui/brand.test.ts`；各子页面的四种主题/动效组合由共享 `tests/e2e/surface.ts` 断言，并保留桌面/移动表格滚动回归。可复现的渲染代理指标采样：

```sh
# 终端 1：启动开发服务器（不可同时运行使用相同端口的 Playwright webServer）
npm run dev -- --port 5183 --strictPort
# 终端 2：可选参考目录只用于本地比较，构建/运行不依赖它
npm run profile:background -- --reference /mnt/d/share/client
```

结果位于 `test-results/background-profile/`，包含 1080p/4K、DPR=2 下的完整动效/低特效截图、Chromium timeline trace 和 `summary.json`。脚本记录主线程耗时、Paint/RasterTask 数量、图层面积和精灵面积估算；参考项重建 client 的 3 个光球、14 个粒子、跟随光晕、网格与噪声 CSS，不加载 client 业务。这些是隔离背景场景的浏览器渲染代理指标，**不是显卡利用率或实测显存**；生产机器仍需用目标显卡、分辨率及完整页面进行 DevTools/系统 GPU 验收。不要将面积变化百分比直接当作 GPU 利用率降幅。

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

生产构建保留默认 500 kB 分块告警阈值；`vite.config.ts` 使用 Rolldown `codeSplitting.groups` 将 ECharts 与 ZRender 分别输出为 `charts`、`chart-renderer`，避免机器性能页合并为超大 JS。性能路由继续按需加载，登录页不预加载图表包。拆包调整需用生产 `dist/` 验证图表加载与渲染，不仅验证开发服务器。

`npm run build` 生成以 `/admin/` 为资源和路由前缀的 `dist/`，开发服务器仍使用 `/`。Vue Router 自动使用构建时的 BASE_URL，接口仍走同源 `/api/v1`。

构建 `szx_admin_service` 时，会自动执行 `npm run build`，并将新生成的 `dist/` 部署到可执行文件目录下的 `web/admin/`；即使后端无需重新链接，也会重新构建并部署前端。启动新版 `szx_admin_service.exe` 后访问 `http://<host>:<port>/admin`；服务会跳转到 `/admin/`，支持 `/admin/login`、`/admin/users`、`/admin/logs` 等页面直接打开和刷新。首次创建部署目录后需重启服务。

自定义静态目录时，先执行 `npm run build`，再在仓库根目录执行 `./scripts/deploy_admin_web.ps1 -StaticRoot <server.staticRoot的实际目录>`。脚本会把 dist 内容放在 staticRoot 的 `admin` 子目录；保持 `spaFallback: true`，只公开构建产物。API 和不存在的 JS/CSS 文件不会回退为 HTML。

CMake 配置时定位 npm；构建 `szx_admin_service` 时自动构建并复制前端产物。生产使用 HTTPS 同源提供页面与 API。局域网普通 HTTP 下 Web Crypto 不可用时，需要 HTTPS 才能完成密码摘要和登录。

接口依据：[RESTful API 接口文档](../../../../docs/RBAC/05-RESTful-API接口文档.md)。

## 第二阶段：自定义权限

登录有 `permission.read` 的账号后可从“权限管理”进入 `/permissions`。默认超管拥有 RBAC 管理能力；普通管理员不自动新增授权。提供 client/custom 权限创建、说明编辑、启停、归档及给现有角色分配启用的自定义权限。归档必须先解除角色关联，内置权限只读；code/module 不可更改，版本冲突需重新加载。

`features/permissions` 管理加载、取消、表单和乐观锁，`api/rbac.ts` 提供类型化接口。UI 继续复用 Lumina 表格、对话框、输入、下拉和主题滚动条。权限、角色管理及角色权限关联文案提供中文、英文、日文、韩文、法文和俄文翻译；接口返回的名称、代码和说明保留原始业务数据。

### 角色管理

`/roles` 提供角色目录、创建、编辑名称/说明、启停、归档，以及完整权限集合与角色管理范围配置。写操作同时要求相应 role.* 能力和目标 manageable；归档必须没有用户引用并输入 code 确认。内置角色不可归档，super_admin 不可停用，必需权限和全部非归档角色的管理范围不可移除。未知已有关系保留；已停用权限允许解绑，不允许新增分配。

所有角色写请求携带当前 version；冲突显式重新加载，关闭或离页提示未保存修改。保存后刷新会话/自身权限。登录/刷新中的 role_id/role_authz_version 变化会使旧 capability 缓存失效。用户角色筛选使用 manageable_roles，创建/分配使用其中启用的 assignable_roles，支持自定义角色 code；停用角色仍可筛选用户但不可新增绑定。用户、审计和性能路由按能力开放，不按固定 admin 名称判断。

后端追加迁移 12，解除 users.role 的旧四角色 CHECK 并扩为 VARCHAR(64)，修复旧角色分配遗漏的 role_id；迁移 1–11 校验值不变。`scripts/test_admin_metrics.py --rbac` 仅在隔离临时库验证角色生命周期及关联保护，禁止在现有应用库运行回归写操作。

角色管理 review 修复：用户目录的 SQL 条件按通用角色 code 语法校验，不再限制四种内置角色；新增角色后默认目录和自定义角色筛选均可用。迁移 12 在执行前检查旧角色 CHECK 是否存在，断点重启仅跳过已完成的删除步骤，仍执行列定义及数据修复并按原 checksum 记录日志。权限读取按会话授权代次隔离，同代次合并请求；旧响应/旧失败不会覆盖新会话，授权版本在读取过程中变化时等待当前代次结果。

`/logs` 的导航、路由和会话更新统一允许 audit.read 或 behavior.read；只展示有权限的标签，初始选择可访问标签，只加载授权数据。权限撤销会清空对应表格/详情并隔离在途旧响应；仍有另一种日志读取能力时留在日志页，否则回 Home。

### 角色与权限关联查看

拥有 `role.read` 的账号可从角色管理或权限管理页进入 `/roles/permissions`。角色目录的“查看关联”携带 role 查询参数；查看归档角色时也带包含归档参数。选择一个角色即可查看全部关联，包括内置、自定义和停用权限，区分“已关联”和“当前生效”。只读查看不需要角色权限分配能力；若没有 `permission.read`，只显示关联 ID，不暴露权限目录或推断未知状态。

角色/权限状态变更后可刷新；快速切换角色会取消旧读取，版本不一致会要求重载。页面不发送 PUT/PATCH/POST/DELETE，Home 的当前账号权限列表保留不变。查看页业务代码在 `features/roles/useRolePermissions.ts`，复用 `api/rbac.ts` 与独立 Lumina UI。

侧栏与 Home 快捷入口共用 `navigation.ts` 的顺序：首页、用户、角色、权限、审计、性能。入口沿用原有角色/能力限制，Home 最后提供改密按钮，并使用自适应网格布局。表格页面通过路由 `tableLayout` 标记接入工作区布局，角色与权限页和权限管理页的横向、纵向滚动均由表格内部承载，标题和筛选区域保持固定。
