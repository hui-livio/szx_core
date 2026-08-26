# UIToolkits 双 UI 开发计划

## 1. 背景

当前 `UIToolkits` 以 Qt 6/QML 为主要使用方式，组件、主题和资源集中在 `components/UIToolkits/`。为兼容使用 QWidget 的既有应用，计划在同一个 `UIToolkits` 动态库中同时提供：

- 现有 QML 组件：通过 `import UIToolkits` 使用；
- QWidget 组件：通过 `#include <UIToolkits/...>` 使用。

本计划不新增独立的 `UIToolkitsWidgets.dll`，两套 UI API 共享主题、字体、图标和通用数据定义，但分别实现 Qt Quick 和 QWidget 的绘制、事件及生命周期逻辑。

## 2. 目标与范围

### 2.1 完成目标

完成后应满足：

1. 现有 QML demo 和 QML module 行为保持兼容；
2. `UIToolkits` 同一个共享库导出首批 QWidget 组件；
3. QML 应用、QWidget 应用均可链接同一个 `UIToolkits` CMake target；
4. 两套组件使用一致的主题 token、字体和图标资源；
5. 安装后的独立 consumer 可以分别验证 QML 和 QWidget 使用方式；
6. DLL 不创建或销毁 `QGuiApplication`/`QApplication`。

### 2.2 首期范围

首期实现以下基础控件：

| QML 组件 | QWidget 组件建议名称 |
| --- | --- |
| `UTButton` | `UIToolkits::UTButton` |
| `UTLabel` | `UIToolkits::UTLabel` |
| `UTLineEdit` | `UIToolkits::UTLineEdit` |
| `UTCheckBox` | `UIToolkits::UTCheckBox` |
| `UTSwitch` | `UIToolkits::UTSwitch` |
| `UTCombobox` | `UIToolkits::UTComboBox` |
| `UTSpinBox` | `UIToolkits::UTSpinBox` |
| `UTDoubleSpinBox` | `UIToolkits::UTDoubleSpinBox` |
| `UTSlider` | `UIToolkits::UTSlider` |
| `UTProgressBar` | `UIToolkits::UTProgressBar` |
| `UTCard` | `UIToolkits::UTCard` |
| `UTDialog` | `UIToolkits::UTDialog` |
| `UTMessageDialog` | `UIToolkits::UTMessageDialog` |

图表、树视图、图片查看器、日期选择器、窗口级组件和复杂交互控件列入后续阶段，不阻塞首期交付。

## 3. 总体架构

```text
UIToolkits.dll
├── QML API
│   └── import UIToolkits
├── QWidget API
│   └── UIToolkits::UTButton / UTLineEdit / ...
└── Shared UI foundation
    ├── theme tokens
    ├── fonts
    ├── icons
    └── common data/formatting
```

QML 和 QWidget 不互相依赖：

- QML 组件不包装 QWidget；
- QWidget 组件不创建或持有 QML 对象；
- 主题和资源层不得依赖 `QQuickItem` 或具体 QWidget；
- 两套组件可以共享相同的属性语义和默认值，但不强行共享实现。

## 4. 关键技术闸门：QML plugin 与单 DLL

当前工程使用 `qt_add_qml_module(UIToolkits ...)`，构建时可能产生独立的 QML plugin。第一阶段必须确认“严格只交付一个 DLL”是否满足 Qt 6.8 的 QML 模块发现和类型注册要求。

需要验证：

- `NO_PLUGIN` 或等效配置是否适用于当前模块；
- QML 类型注册、QML 缓存和资源加载是否正常；
- `qmlimportscanner`、`qmlcachegen` 和 `windeployqt` 是否能正确部署；
- 安装后的 QML consumer 是否只依赖 `UIToolkits.dll`；
- 若 Qt 仍要求独立 plugin，是否接受“一个主 DLL + QML plugin”的最终交付形式。

该验证完成前，不修改现有 QML module 的运行机制。首要目标是保持 QML 稳定，再决定是否合并 plugin。

## 5. 分阶段实施

### M1：单 DLL/QML 可行性验证

**工作内容**

- 建立最小 QML consumer；
- 建立最小 QWidget consumer；
- 对比默认 plugin 配置和单 DLL 配置；
- 记录 Debug/Release 的实际 DLL 依赖；
- 确定安装树中允许出现的文件。

**交付物**

- QML consumer 验证结果；
- QWidget consumer 验证结果；
- 单 DLL 可行性结论；
- 如无法严格单 DLL，记录兼容的 plugin 交付方案。

### M2：公共主题和资源层整理

建议将主题相关代码整理为：

```text
components/UIToolkits/src/theme/
├── UTTheme.h/.cc
├── UTThemeTokens.h
├── UTFontManager.h/.cc
└── UTIconManager.h/.cc
```

**工作内容**

- 抽取颜色、字号、间距、圆角和控件高度等 token；
- 保持字体资源路径和安装路径稳定；
- 提供 QML 与 QWidget 均可调用的主题接口；
- 明确主题切换后的刷新机制；
- 确保公共主题层不依赖 Quick 或具体 QWidget 控件。

**验收**

- QML 现有组件行为不变；
- 主题 token 可被 QWidget 实现使用；
- 字体和图标在构建树、安装树均可加载。

### M3：CMake、公共头和导出接口

**工作内容**

- 在现有 `UIToolkits` target 中加入 `Qt6::Widgets`；
- 保持 `UIToolkits` 为唯一共享库目标；
- 建立 `include/UIToolkits/` 公共头路径；
- 增加 Widget 源码目录和导出宏；
- 更新安装规则、package config 和 build-tree consumer；
- 不新增 `UIToolkitsWidgets.dll`。

建议的链接关系：

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Quick
    QuickControls2
    QmlIntegration
    Widgets
)

target_link_libraries(UIToolkits
    PUBLIC
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
    PRIVATE
        Qt6::QmlIntegration
)
```

### M4：首批 QWidget 控件

建议目录：

```text
components/UIToolkits/
├── include/UIToolkits/
│   ├── UTButton.h
│   ├── UTLabel.h
│   ├── UTLineEdit.h
│   ├── UTCheckBox.h
│   ├── UTSwitch.h
│   ├── UTComboBox.h
│   ├── UTSpinBox.h
│   ├── UTDoubleSpinBox.h
│   ├── UTSlider.h
│   ├── UTProgressBar.h
│   ├── UTCard.h
│   ├── UTDialog.h
│   └── UTMessageDialog.h
└── src/widgets/
    ├── UTButton.cc
    ├── UTLabel.cc
    └── ...
```

**实现要求**

- 使用 Qt Widgets 原生基类和信号槽机制；
- 复用公共主题 token，不复制一套独立颜色常量；
- 公共头只暴露稳定的 QWidget API；
- 不暴露第三方异常类型；
- GUI 线程和对象所有权遵循 Qt 规则；
- 析构、事件和信号回调不得让异常跨出 Qt 边界。

### M5：示例和测试

新增示例：

```text
tools/UIToolkitsWidgetsApp/
```

示例覆盖：

- 基础控件展示；
- 主题切换；
- 对话框；
- 中文字体；
- 高 DPI；
- QML 与 QWidget 在同一进程中的使用边界。

测试分层：

1. **编译测试**：公共头可独立包含，consumer 可链接 `UIToolkits`；
2. **Widget 行为测试**：属性、信号、键盘鼠标、禁用状态和主题刷新；
3. **QML 回归测试**：现有 demo、module、字体和资源继续可用；
4. **安装测试**：仅通过 `find_package(SZXComponents CONFIG REQUIRED)` 使用安装包。

### M6：文档和发布验收

更新以下文档：

- `README.md`；
- `docs/使用/组件集成使用指南.md`；
- `docs/规划/AOI基础组件迁移说明.md`；
- QML/Widget 对应关系和 API 使用说明。

发布前确认：

- Debug/Release 配置和构建通过；
- QML demo 全部启动；
- QWidget demo 启动并完成基础交互；
- 安装包 consumer 构建通过；
- 连续 configure/build 不修改源码树；
- 最终 DLL/plugin 交付形态已写入安装文档。

## 6. 生命周期和使用约束

### 应用对象

`UIToolkits` DLL 不创建或销毁应用对象：

- QML 应用由使用者创建 `QGuiApplication` 或 `QApplication`；
- QWidget 应用由使用者创建 `QApplication`；
- 控件必须在 GUI 线程创建和使用。

### 混用规则

- 同一进程可以同时使用 QML 和 QWidget；
- QML 类型和 QWidget 对象不直接互相转换；
- 主题管理可以共享，但控件实例生命周期各自管理；
- DLL 不保存未经所有权约束的全局窗口或控件指针。

## 7. 风险与应对

| 风险 | 影响 | 应对 |
| --- | --- | --- |
| QML plugin 无法真正合并到单 DLL | 交付文件数量增加 | M1 先验证；必要时接受主 DLL + plugin 的稳定方案 |
| Quick 与 Widgets 依赖同时引入导致部署变复杂 | 安装包和启动失败 | 增加 QML/Widget 独立 consumer 和 windeployqt 验证 |
| 两套控件视觉不一致 | 使用体验不统一 | 先抽取主题 token，禁止实现中散落颜色常量 |
| QWidget 公共 API 过早固化 | 后续 ABI 兼容成本高 | 首批只发布基础控件，使用导出宏和稳定命名 |
| Widget 控件跨线程使用 | 崩溃或未定义行为 | 文档明确 GUI 线程约束，测试中加入线程边界检查 |
| 复杂控件迁移拖慢首期 | 计划延期 | 图表、树视图和图片查看器延后到第二阶段 |

## 8. 首期验收标准

首期完成必须同时满足：

- `UIToolkits` 同一共享库提供至少 6 个 QWidget 控件；
- 现有 QML demo 和 module 通过回归验证；
- QML consumer 和 QWidget consumer 均链接同一个 `UIToolkits` target；
- 主题和字体资源在两套控件中可用；
- 安装后的独立 consumer 构建通过；
- Debug/Release 构建通过；
- 明确并记录 QML plugin 是否必须独立交付；
- 复杂控件不纳入首期阻塞范围。

## 9. 后续阶段

首期稳定后，再按以下顺序扩展：

1. `UTTabBar`、`UTScrollBar`、列表和树视图；
2. 日期/时间选择器和复杂输入控件；
3. 图片查看器和多矩形覆盖层；
4. 图表组件；
5. 窗口级组件、菜单和复杂弹窗；
6. QML/Widget 视觉一致性快照和跨平台验证。

