---
name: api-stability-inline-namespace
description: 固化 C++ 公共 API 并用 inline namespace 控制版本发布。凡是新增、修改、废弃、兼容、发布或评审库/DLL/模块 API，尤其涉及 ABI、语义版本、默认版本、旧版本并存、头文件导出和迁移策略时，都使用此技能。它要求外层命名空间保持稳定，通过 inline namespace vN 管理当前版本，并在发布前验证 API/ABI 与版本切换。
compatibility: C++20；适用于静态库、共享库、DLL 和跨模块头文件 API。需要现有项目的构建、测试及 ABI 检查工具（若项目已配置）。
---

# C++ API 固化与 inline namespace 发布

把公共 API 当作发布契约处理：调用方依赖的是稳定的名称、类型、语义和 ABI，而不是当前实现。使用稳定的外层 namespace 暴露 API，使用 `inline namespace` 选择默认发布版本；版本切换必须是明确、可回滚、可验证的发布动作。

## 触发条件

当任务提到 API 固定/冻结、版本发布、兼容旧客户端、ABI、breaking change、namespace 版本、`inline namespace`、v1/v2 共存、DLL 导出或公共头文件变更时，先遵循本技能，再进行编码。

## 标准布局

优先采用以下结构；版本号按项目约定替换，不要把版本号散落在调用方代码中：

```cpp
namespace company::product {

inline namespace v2 {
class Detector {
public:
    explicit Detector(DetectorOptions options);
    DetectionResult detect(const Image& image) const;
};
}

namespace v1 {
class Detector; // 仅在确有兼容需求时保留
}

} // namespace company::product
```

`company::product::Detector` 解析到当前 inline 版本；`company::product::v1::Detector` 和 `company::product::v2::Detector` 可显式选择。不要同时把多个版本声明为 `inline`，也不要用宏让同一构建产物在不同调用方解析到不同 ABI。

## 必须先做的调查

1. 识别 API 的所有消费者：本仓库模块、DLL/插件、测试、脚本、示例和外部安装包。
2. 搜索公共头、导出宏、namespace、版本宏、CMake 安装规则和符号导出：

   ```text
   rg -n "inline namespace|namespace .*v[0-9]|API|EXPORT|__declspec|install\(|target_include_directories|SOVERSION" .
   rg -n "#include .*public|company::product|product::v[0-9]" apps modules detectengine test docs
   ```

3. 建立 API 清单：符号、所属版本、可见性、所有权、输入输出、错误语义、线程模型、异常/生命周期约束及 ABI 风险。
4. 检查当前发布版本、最低支持版本、编译器/标准库 ABI、二进制包命名和文档中的版本承诺。

## API 固化规则

- 已发布的函数签名、类布局、枚举数值、结构体字段顺序、虚函数顺序、所有权和线程语义视为冻结。
- 不要通过修改已有参数、返回类型、默认值语义、枚举含义或类成员布局来“悄悄”修复 API；这类变更应进入新版本。
- 公共头文件中避免暴露私有实现、第三方类型、未固定布局的容器、裸 owning 指针和不稳定异常类型。
- 对跨 DLL 边界的资源提供成对的创建/销毁或明确 allocator 所有权；不要让不同运行时库释放彼此分配的内存。
- 使用明确的固定宽度整数、稳定的错误码/结果类型和可演进的选项结构；新增字段必须保持旧字段语义。
- `inline namespace` 只负责名称解析和版本选择，不替代 ABI 隔离、符号版本或运行时能力协商。
- 不要把 `using namespace`、宏别名或重载技巧当作兼容层；兼容层必须可审计且有移除计划。

## 版本发布流程

### 1. 分类变更

将变更归类为：

- **Patch**：实现修复，不改变可观察 API/ABI/语义。
- **Minor**：向后兼容地新增 API、枚举项或可选能力。
- **Major**：删除/重命名 API、改变布局、所有权、线程/错误语义或其他破坏性变更。

如果无法证明兼容，按破坏性变更处理，不要以 patch/minor 发布。

### 2. 选择版本策略

- 兼容新增：在当前版本中加入，并记录 API 基线。
- 破坏性变更：新增 `vN` inline namespace，保留旧版本适配层或旧版本库，直到支持窗口结束。
- 发布切换：只在一个受控提交中将 `inline namespace vN` 改为新版本，并同步文档、包名、导出符号和测试。
- 旧版本终止：先标记 deprecated、给迁移说明和截止版本；确认消费者迁移后再移除。

### 3. 验证发布

至少执行：

```text
cmake --build build --config Release
ctest --test-dir build --output-on-failure
git diff --check
```

并根据项目工具运行 API/ABI diff（如 `abi-dumper`/`abi-compliance-checker`、平台符号导出检查或等价工具）。验证新旧客户端分别链接，确认默认 namespace、显式旧版本 namespace、导出符号、包安装路径和运行时加载行为符合发布计划。

## 兼容示例

```cpp
namespace company::product {

inline namespace v2 {
using DetectionId = std::uint64_t;
struct DetectionResult { DetectionId id; int defect_count; };
}

namespace v1 {
struct DetectionResult { int defect_count; };
DetectionResult detect(const Image& image);
}

} // namespace company::product
```

新代码默认使用 `company::product::DetectionResult`；需要旧契约的代码显式使用 `company::product::v1::DetectionResult`。不要让 v1 返回值隐式转换成 v2，因为这会掩盖语义丢失；用命名适配函数并测试每个字段映射。

## 发布评审清单

- [ ] 默认 inline namespace 只有一个，且版本号与包/文档一致。
- [ ] API/ABI 基线已生成并与上一发布版本比较。
- [ ] 变更已分类为 patch/minor/major，理由可追溯。
- [ ] 旧版本消费者有明确的 include、链接和迁移路径。
- [ ] 公共头没有泄漏实现类型、诊断字符串或不稳定第三方 ABI。
- [ ] DLL 导出、库名、安装目录、CMake 配置包和运行时加载策略已同步。
- [ ] 默认版本与显式旧版本均有编译、链接和行为测试。
- [ ] deprecated API 有截止版本、替代 API 和迁移示例。
- [ ] 发布说明记录兼容性、回滚方式和支持窗口。

## 输出要求

完成任务时报告：变更分类与发布版本、inline namespace 切换点、保留的旧版本、受影响的消费者、API/ABI 检查结果、测试命令及任何未验证的兼容风险。若无法确认 `inline namespace` 是项目既定策略，先说明假设并避免擅自改变现有版本模型。
