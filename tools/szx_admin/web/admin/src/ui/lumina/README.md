# Lumina UI 源码模块

这是应用内的独立 UI 目录，**不是 npm 包**。当前不提供独立 `package.json`、库构建配置或发布流程。它集中管理主题、通用控件和 UI 机制，方便其他 Vue 页面复用，也为以后抽包保留清晰边界。

## 目录与边界

```text
lumina/
├── components/     # LuminaButton/Input/Select/Table/Dialog/DateTimePicker/Pagination/Background
├── composables/    # 主题缩放与 resize 生命周期
├── theme/
│   ├── aura.ts     # Aura preset 与现有 CSS token 映射
│   ├── install.ts  # PrimeVue 安装与 locale 更新入口
│   ├── locale.ts   # Intl 日历名称 + 调用方提供的文案
│   ├── tokens.css  # 通用色彩、表面色、低特效模式
│   ├── controls.css
│   └── glass/      # 原有 Lumina Glass 主题、字体及许可证
├── utils/dateTime.ts
├── index.ts        # 控件、类型、工具的统一入口
└── styles.css      # 显式样式入口
```

允许依赖 Vue、PrimeVue 和 PrimeUIX；禁止反向依赖应用的 API、domain、路由、Pinia、ViewModel 或 vue-i18n。角色、权限、业务状态文案及 UTC 查询规则由调用方管理。禁用浏览器右键菜单也是应用策略，不属于 UI 模块。

当前依赖沿用应用已安装版本：Vue 3.5、PrimeVue 4.5.5、`@primeuix/themes` 2.0.3。没有新增依赖。若复制该目录到另一 Vue 应用，调用方需要提供这些依赖。

## 安装与样式

在应用入口安装一次，样式也只需引入一次：

```ts
import { createApp } from 'vue'
import { createLuminaUi } from './ui/lumina'
import './ui/lumina/styles.css'
import App from './App.vue'

createApp(App).use(createLuminaUi()).mount('#app')
```

控件按需显式导入，不默认注册全局组件。`styles.css` 包含基础排版/reset、主题字体和组件样式；它不包含后台侧栏、页面布局、业务角色标签或操作按钮权限。应用样式可以放在 `app` CSS 层覆盖通用外观。

使用 `<html data-theme="light|dark" data-fx="lite|full">` 控制主题与特效。缺少 `data-fx="full"` 时默认关闭装饰动效，系统减少动态效果偏好同样生效。需要原有连续比例时，在根组件的 setup 中调用 `useLuminaScale()`；初始 `--k` 为 1，缩放范围为 0.92–1.5。

## 基础控件

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { LuminaButton, LuminaInput, LuminaSelect } from './ui/lumina'

const name = ref('')
const role = ref('reader')
const options = [
  { value: 'reader', label: 'Reader' },
  { value: 'editor', label: 'Editor' },
]
</script>

<template>
  <LuminaInput v-model="name" aria-label="Name" required maxlength="128" />
  <LuminaSelect
    v-model="role"
    :options="options"
    option-label="label"
    option-value="value"
    aria-label="Role"
  />
  <LuminaButton variant="primary" type="submit">Save</LuminaButton>
</template>
```

- `LuminaButton`：`variant="primary|secondary|text|danger"`，默认 secondary；默认 `type="button"`，表单提交需明确 `type="submit"`。支持 disabled、loading、label，以及图标/默认等插槽。
- `LuminaInput`：字符串 `v-model`；原生输入属性、事件和 ARIA 属性透传到 InputText，不增加额外 DOM 包装。
- `LuminaSelect`：泛型 `v-model`、只读 options 输入；PrimeVue 的 option-label/option-value、ARIA 属性、事件和具名插槽透传。选中值类型由调用方的 `v-model` 决定，options 中的 option-value 必须与之匹配。

## 表格

```vue
<LuminaTable :value="rows" data-key="id" min-width="640px">
  <LuminaColumn header="Name">
    <template #body="{ data }">{{ data.name }}</template>
  </LuminaColumn>
  <template #empty>No records</template>
</LuminaTable>
```

`LuminaTable` 保留 DataTable 的属性、事件和插槽，默认最小表格宽度为 0，可用 `min-width` 指定数值像素或 CSS 长度。通用外观限定于 `.lumina-table`，不会覆盖日历表格。表格如何撑满工作区、分页/排序是否走服务端，由应用决定。

`LuminaColumn` 是 PrimeVue Column 的导出别名，不增加渲染包装：它是配置节点，需要保留原生身份，保证 DataTable 的列发现、注入和 body 插槽正常工作。业务行类型和字段内容不属于 UI 模块。

## 对话框与分页

```vue
<LuminaDialog v-if="open" title="Details" close-label="Close" :busy="saving" @close="open = false">
  <p>Caller-provided content</p>
  <template #footer>Caller-provided footer</template>
</LuminaDialog>

<LuminaPagination
  :page="page"
  :page-size="20"
  :page-count="pageCount"
  :total="total"
  :busy="loading"
  :summary="`Total ${total} records`"
  label="Pages"
  previous-label="Previous"
  next-label="Next"
  @change="loadPage"
/>
```

对话框使用调用方的 `v-if` 生命周期，发出 close 后由调用方卸载。busy 时禁止关闭；默认不允许点击遮罩关闭，不可拖拽；支持 default/header/footer 插槽。卸载后恢复打开动作的焦点，若已有另一个对话框则不抢占焦点。默认宽度为随 `--k` 缩放的 600px，调用方可通过 style 覆盖。关闭按钮保持宽大于高的圆角矩形。

分页使用 1 起始页码，不负责请求数据。summary、navigation、上一页/下一页等文案由调用方提供，忙碌期间禁止翻页。

## 日期时间与 locale

```vue
<LuminaDateTimePicker
  v-model="localTime"
  label="Local time"
  invalid-text="Enter a valid date and time."
/>
```

- model 是本地时间字符串 `YYYY-MM-DDTHH:mm:ss`，空值为 `''`。
- 输入框显示/手动输入格式为 `YYYY-MM-DD HH:mm:ss`；24 小时制，保留秒。
- 无效或尚未完成的手动输入不会覆盖已提交模型，并通过原生表单 validity 阻止提交；错误文案由调用方提供。
- 点击日历图标或按 ArrowDown 打开浮层，Escape 关闭；支持 Today/Clear。
- `formatDateTimeLocal` 和 `parseDateTimeLocal` 不进行 UTC 转换，解析拒绝日期溢出及无效时分秒；业务边界自行转换为 UTC。

日历名称由 Intl 生成，其他文案使用调用方的值，不依赖 vue-i18n：

```ts
import { createLuminaCalendarLocale, useLuminaLocale } from './ui/lumina'

// 在已安装 createLuminaUi 的组件 setup 内调用。
const updateLocale = useLuminaLocale()
updateLocale(
  createLuminaCalendarLocale(
    'en-US',
    {
      today: 'Today',
      clear: 'Clear',
      chooseDate: 'Choose date',
      chooseMonth: 'Choose month',
      chooseYear: 'Choose year',
      navigation: { nextSecond: 'Next second', prevSecond: 'Previous second' },
    },
    { firstDayOfWeek: 0 },
  ),
)
```

locale 必须是 Intl 支持的语言标识；非法标识遵循 Intl 的错误行为。`navigation` 可提供 prev/nextMonth、Year、Decade、Hour、Minute、Second。未提供的导航文案沿用 PrimeVue 默认值。后台的六种语言文案在 `src/i18n/primevue.ts` 中适配，未进入此目录。

## 当前应用适配层

后台仍保留 `ModalDialog.vue`、`PaginationBar.vue` 和 `LocalDateTimePicker.vue`，只负责从 vue-i18n 获取文案并传给通用控件。`domain.ts` 保留日期工具的兼容导出，但实现由此目录拥有；`toUtcSeconds` 留在业务侧。

## 验证

在 admin 目录执行：

```sh
npm run typecheck
npm run build
npm test
npm run test:e2e
npm run format:check
```

`tests/fixtures/lumina/index.html` 是不加载后台路由、Pinia、vue-i18n 或 API 的独立验证页；开发服务器可访问 `/tests/fixtures/lumina/index.html`，生产入口不包含它。浏览器验证覆盖属性/插槽/model 转发、表格列、分页、对话框焦点和忙碌状态、日期模型及暗色浮层。`tests/ui/lumina.test.ts` 校验目录依赖边界、缩放与日历 locale。

## 外观与特效开关

`LuminaAppearanceControls` 接收 `theme`（light/dark）和 `effects`（lite/full）两个 model，及调用方提供的 labels（theme、performance、light、dark、lite、full）。性能开关开启表示 lite，关闭表示 full；深色开关开启表示 dark。

控件只发出 model 更新，不负责保存偏好。调用方使用 `applyLuminaAppearance({ theme, effects })` 设置根元素属性；也可传入指定 HTMLElement。没有 DOM 时该函数不做操作，适用于 SSR 初始化。浏览器存储由应用自己的 `useUiPreferences` 管理，不进入 UI 目录。

## 共享标记（Logo / 用户头像）

`LuminaMark` 用同一个组件渲染品牌标记和用户初始字母，提供 `text`、`label`、`variant`、`size`、`radius`、`fontSize`、`fontWeight`。size/radius/fontSize 支持数值像素或 CSS 长度字符串；fontWeight 支持数值或 CSS 字重值，默认 500；variant 为 brand（蓝紫渐变）或 soft（随明暗主题变化的柔和底色）。

```vue
<LuminaMark text="S" :size="48" :radius="12" />
<LuminaMark text="超" variant="brand" :size="36" radius="50%" :font-size="16" :font-weight="500" />
```

未提供 label 时是装饰元素；提供 label 时使用 img 语义供辅助技术读取。当前登录页、侧栏 S 标记和顶部用户标记都复用它；应用中的用户头像同样使用 brand 渐变与白色文字，仅尺寸与圆角不同。

圆形外观开关的尺寸及 SVG viewport 通过 `useLuminaScale()` 对齐物理像素；使用柔和内描边、geometricPrecision 与不随缩放改变宽度的圆角 SVG 描线，降低小尺寸/非整数显示缩放下的硬边锯齿。明暗 hover、active 和 soft 表面使用语义化 token，应用不直接复用浅色底色。
