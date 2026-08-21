/**
 * @file QmlPropertyMacros.h
 * @brief   QML 属性定义宏 — 简化 Q_PROPERTY、getter、setter、信号与成员变量的声明
 * @details 提供一组宏，用于在 QObject 派生类中快速声明 QML 属性。
 *          基础类型（int, bool, qreal, QString 等）自动生成完整的 getter/setter/信号/成员变量；
 *          自定义/对象类型仅声明 Q_PROPERTY 与 getter，setter 与信号由开发者自行实现。
 *
 * @author  livliu(yuanfeng1897@outlook.com)
 * @date    2026-06-24
 * @version 1.0
 */

#pragma once

// ============================================================================
// 宏设计说明
// ============================================================================
//
// 每个宏在展开后会设置访问控制符（public:/Q_SIGNALS:/private:），
// 因此建议将所有属性宏集中放置在 Q_OBJECT 宏之后、手写 public: 段之前。
//
// 命名约定：
//   name  — camelCase 属性名，用于 getter 函数名、信号名、成员变量名
//   Name  — PascalCase 后缀，用于 setter 函数名 (set + Name)
//
// 示例：
//   UT_QML_RW_PROPERTY(int, count, Count, 0)
//     展开为: Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
//             int count() const;
//             void setCount(int value);
//             void countChanged();        // 信号
//             int m_count{0};
//
//   UT_QML_OBJ_PROPERTY(ImageBridge*, imageBridge, ImageBridge)
//     展开为: Q_PROPERTY(ImageBridge* imageBridge READ imageBridge
//                        WRITE setImageBridge NOTIFY imageBridgeChanged)
//             ImageBridge* imageBridge() const;
//             void setImageBridge(ImageBridge* value);  // 声明，需手动实现
//             void imageBridgeChanged();                // 信号
//             ImageBridge* m_imageBridge{nullptr};
//
//   UT_QML_CONST_GETTER_PROPERTY(QAbstractItemModel*, model)
//     展开为: Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)
//             QAbstractItemModel* model() const;        // 声明，需手动实现

// ============================================================================
// 基础类型 — 读写属性 (Read-Write)
// ============================================================================

/**
 * @def    UT_QML_RW_PROPERTY
 * @brief  声明一个完整的基础类型 QML 属性（含 getter、setter、信号、成员）
 * @param  Type       C++ 类型 (int, bool, qreal, double, QString 等)
 * @param  name       属性名 (camelCase)，同时作为 getter 名与成员变量名 m_name
 * @param  Name       Setter 名后缀 (PascalCase)，setter = set + Name
 * @param  defaultVal 成员变量默认值
 *
 * @note   Setter 仅声明，需在 .cpp 中实现（通常包含赋值 + emit 信号）
 */
#define UT_QML_RW_PROPERTY(Type, name, Name, defaultVal)                                                                         \
    Q_PROPERTY(Type name READ name WRITE set##Name NOTIFY name##Changed)                                                         \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
    void set##Name(const Type &value);                                                                                           \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};

/**
 * @def    UT_QML_RW_PROPERTY_FINAL
 * @brief  声明一个带 FINAL 标记的完整基础类型 QML 属性
 */
#define UT_QML_RW_PROPERTY_FINAL(Type, name, Name, defaultVal)                                                                   \
    Q_PROPERTY(Type name READ name WRITE set##Name NOTIFY name##Changed FINAL)                                                   \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
    void set##Name(const Type &value);                                                                                           \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};

// ============================================================================
// 基础类型 — 只读属性 (Read-Only)
// ============================================================================

/**
 * @def    UT_QML_RO_PROPERTY
 * @brief  声明一个只读的基础类型 QML 属性（含 getter、信号、成员）
 * @param  Type       C++ 类型
 * @param  name       属性名 (camelCase)
 * @param  defaultVal 成员变量默认值
 *
 * @note   属性值由内部逻辑更新并 emit 信号；QML 端无法直接写入
 */
#define UT_QML_RO_PROPERTY(Type, name, defaultVal)                                                                               \
    Q_PROPERTY(Type name READ name NOTIFY name##Changed)                                                                         \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};

// ============================================================================
// 基础类型 — 读写属性，Setter 不需要额外逻辑时内联
// ============================================================================

/**
 * @def    UT_QML_RW_PROPERTY_INLINE
 * @brief  声明一个基础类型 QML 属性，setter 内联实现（仅赋值 + emit）
 * @param  Type       C++ 类型
 * @param  name       属性名 (camelCase)
 * @param  Name       Setter 名后缀 (PascalCase)
 * @param  defaultVal 成员变量默认值
 *
 * @note   适用于 setter 无需额外逻辑的简单属性，无需在 .cpp 中再实现
 */
#define UT_QML_RW_PROPERTY_INLINE(Type, name, Name, defaultVal)                                                                  \
    Q_PROPERTY(Type name READ name WRITE set##Name NOTIFY name##Changed)                                                         \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
    void set##Name(const Type &value)                                                                                            \
    {                                                                                                                            \
        if (m_##name == value)                                                                                                   \
            return;                                                                                                              \
        m_##name = value;                                                                                                        \
        Q_EMIT name##Changed();                                                                                                  \
    }                                                                                                                            \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};

// ============================================================================
// 对象/指针类型 — 读写属性
// ============================================================================

/**
 * @def    UT_QML_OBJ_PROPERTY
 * @brief  声明一个对象/指针类型的 QML 属性（声明 getter/setter/信号/成员）
 * @param  Type  指针类型 (ImageBridge*, SomeModel* 等)
 * @param  name  属性名 (camelCase)
 * @param  Name  Setter 名后缀 (PascalCase)
 *
 * @note   Getter 内联返回指针；Setter 仅声明，需手动在 .cpp 中实现
 *         成员变量默认初始化为 nullptr
 */
#define UT_QML_OBJ_PROPERTY(Type, name, Name)                                                                                    \
    Q_PROPERTY(Type name READ name WRITE set##Name NOTIFY name##Changed)                                                         \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
    void set##Name(Type value);                                                                                                  \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{nullptr};

// ============================================================================
// 对象/指针类型 — 常量属性
// ============================================================================

/**
 * @def    UT_QML_CONST_PROPERTY
 * @brief  声明一个只读常量对象/指针属性（QML 端不可写，C++ 端不变）
 * @param  Type  指针类型
 * @param  name  属性名 (camelCase)
 *
 * @note   适用于构造后不变的子对象引用（如子 Controller、Model）
 *         成员变量默认初始化为 nullptr
 */
#define UT_QML_CONST_PROPERTY(Type, name)                                                                                        \
    Q_PROPERTY(Type name READ name CONSTANT)                                                                                     \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{nullptr};

// ============================================================================
// Getter-only 属性 — 常量属性，不生成成员变量
// ============================================================================

/**
 * @def    UT_QML_CONST_GETTER_PROPERTY
 * @brief  声明一个只读常量 QML 属性，仅声明 getter，不生成成员变量
 * @param  Type  C++ 类型
 * @param  name  属性名 (camelCase)
 *
 * @note   适用于属性名与内部成员不一致、需要向上转型或计算返回值的 CONSTANT 属性。
 */
#define UT_QML_CONST_GETTER_PROPERTY(Type, name)                                                                                 \
    Q_PROPERTY(Type name READ name CONSTANT)                                                                                     \
public:                                                                                                                          \
    Type name() const;

// ============================================================================
// Getter-only 属性 — 只读属性，不生成成员变量
// ============================================================================

/**
 * @def    UT_QML_RO_GETTER_PROPERTY
 * @brief  声明一个只读 QML 属性，仅声明 getter 和默认 nameChanged 信号
 * @param  Type  C++ 类型
 * @param  name  属性名 (camelCase)
 *
 * @note   适用于由其他内部状态计算得到的属性，setter 由业务方法间接触发。
 */
#define UT_QML_RO_GETTER_PROPERTY(Type, name)                                                                                    \
    Q_PROPERTY(Type name READ name NOTIFY name##Changed)                                                                         \
public:                                                                                                                          \
    Type name() const;                                                                                                           \
Q_SIGNALS:                                                                                                                       \
    void name##Changed();                                                                                                        \
                                                                                                                                 \
private:

// ============================================================================
// 基础类型 — 只读属性，自定义信号名（多属性共享同一信号）
// ============================================================================

/**
 * @def    UT_QML_RO_PROPERTY_SIG
 * @brief  声明一个只读的基础类型 QML 属性，使用指定的通知信号
 * @param  Type       C++ 类型
 * @param  name       属性名 (camelCase)
 * @param  signalName 通知信号名（需在类中另行声明）
 * @param  defaultVal 成员变量默认值
 *
 * @note   用于多个属性共享同一通知信号的场景（如 dataChanged），
 *         信号由类自身在 signals: 段统一声明，宏不重复声明
 */
#define UT_QML_RO_PROPERTY_SIG(Type, name, signalName, defaultVal)                                                               \
    Q_PROPERTY(Type name READ name NOTIFY signalName)                                                                            \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};

// ============================================================================
// 基础类型 — 读写属性，自定义信号名
// ============================================================================

/**
 * @def    UT_QML_RW_PROPERTY_SIG
 * @brief  声明一个读写的基础类型 QML 属性，使用指定的通知信号
 * @param  Type       C++ 类型
 * @param  name       属性名 (camelCase)
 * @param  Name       Setter 名后缀 (PascalCase)
 * @param  signalName 通知信号名（需在类中另行声明）
 * @param  defaultVal 成员变量默认值
 *
 * @note   Setter 仅声明，需在 .cpp 中实现；信号由类自身统一声明
 */
#define UT_QML_RW_PROPERTY_SIG(Type, name, Name, signalName, defaultVal)                                                         \
    Q_PROPERTY(Type name READ name WRITE set##Name NOTIFY signalName)                                                            \
public:                                                                                                                          \
    Type name() const                                                                                                            \
    {                                                                                                                            \
        return m_##name;                                                                                                         \
    }                                                                                                                            \
    void set##Name(const Type &value);                                                                                           \
                                                                                                                                 \
private:                                                                                                                         \
    Type m_##name{defaultVal};
