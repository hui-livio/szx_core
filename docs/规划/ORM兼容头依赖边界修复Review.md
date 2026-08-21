# ORM 兼容头依赖边界修复 Review

## 修改前 Review

| 项目 | 内容 |
|---|---|
| 改动范围 | 仅调整 `Db/PcbProductRow.h` 与 `Db/Internal/PcbOrmRows.h` 的 `pcb_product` 定义归属；前者恢复纯标准库定义，后者包含它并删除重复类型 |
| 影响范围 | 所有包含 `PcbProductRow.h` 的 Engine/应用/测试编译单元；DBOperation/DAO 继续通过 Internal 头获取同一 `pcb_product` |
| 保持不变 | 类名、namespace、字段顺序/类型/默认值、ORM 反射可见性、ABI 布局、schema、序列化和运行时行为 |
| 风险等级 | 中；若仍存在重复定义或 Internal 未包含产品头，DB 目标会编译失败；字段布局变化会破坏 ORM 映射 |
| 验证计划 | 新增/扩展纯头编译契约，证明不提供 ormpp include 也可包含；ORM rows contract、DB target、`PCBDetect`、全量 CTest、include 扫描与 diff check |
| 回滚方式 | 独立提交整体撤销；恢复旧包含关系（但会重现非 DB 目标缺少 `ormpp/utility.hpp`） |

本次不将 `ormpp::ormpp` 从 PRIVATE 改为 PUBLIC，不移动其他 ORM row，不修改 DBOperation/DAO 或业务代码；当前 5.4.4b 模型权重迁移保留在工作区，但不纳入本修复提交。

## 修改后 Review

- 实际改动：`pcb_product` 按原字段顺序和类型移回纯标准库 `PcbProductRow.h`；`PcbOrmRows.h` 改为包含该头，其他 ORM row 不变；新增一个不链接 `ormpp::ormpp` 的独立编译契约。
- 与计划偏差：无。未改动 ormpp 的 PRIVATE 链接性、未将 Internal 头暴露给新调用方。
- 影响范围复核：非 DB 调用方包含 `PcbProductRow.h` 时只需 C++ 标准头；DBOperation/DAO 仍在同一 namespace 获得唯一 `pcb_product` 定义。静态契约和既有 ORM 契约确认主要类型与默认值不变。
- 风险发现：无高/中风险。`PcbProductRow` 仍是迁移期 DB row 而非最终领域 DTO，属于阶段 5/9 继续收缩的低风险遗留。
- 验证结果：无 ormpp 依赖的 ProductRow contract 与 ORM rows contract 2/2 通过；使用 `scripts/build.py` 初始化 MSVC 环境后，Detection target 及 `PCBDetect` 编译通过；全量 CTest 709/709 通过，含 DB suite 与 architecture boundaries。
- 提交隔离：只暂存两个 row 头、新契约测试、该测试的 CMake 接线与本 Review；5.4.4b 未完成代码及用户文档全部排除。
- 结论：根因已消除，无未处理高/中风险，允许独立提交。
