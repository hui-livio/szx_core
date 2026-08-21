/**
 * @file UTTheme.h
 * @brief QML 主题单例 — UI 工具箱对外唯一主题入口
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2025-06-24
 * @version 1.0
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/QQmlEngine>

// name, palette key, fallback key, final fallback color
#define UT_THEME_COLOR_PROPERTIES(X)                                                                                             \
    X(primaryColor, "primaryColor", "", "#3a6df0")                                                                               \
    X(focusColor, "focusColor", "primaryColor", "#3a6df0")                                                                       \
    X(successColor, "successColor", "", "#22c55e")                                                                               \
    X(errorColor, "errorColor", "", "#ef4444")                                                                                   \
    X(warningColor, "warningColor", "", "#f59e0b")                                                                               \
    X(infoColor, "infoColor", "", "#38bdf8")                                                                                     \
    X(backgroundColor, "backgroundColor", "", "#f5f7fa")                                                                         \
    X(surfaceColor, "surfaceColor", "", "#ffffff")                                                                               \
    X(primaryTextColor, "primaryTextColor", "", "#181d27")                                                                       \
    X(secondaryTextColor, "secondaryTextColor", "", "#5a6577")                                                                   \
    X(borderColor, "borderColor", "", "#d5dbe3")                                                                                 \
    X(shadowColor, "shadowColor", "", "#18000000")                                                                               \
    X(titleBarColor, "titleBarColor", "backgroundColor", "#f5f7fa")                                                              \
    X(surfaceButtonHoverColor, "surfaceButtonHoverColor", "", "#14000000")                                                       \
    X(surfaceButtonPressedColor, "surfaceButtonPressedColor", "", "#1f000000")                                                   \
    X(filledButtonHoverColor, "filledButtonHoverColor", "", "#24ffffff")                                                         \
    X(filledButtonPressedColor, "filledButtonPressedColor", "", "#26000000")                                                     \
    X(tonalButtonHoverColor, "tonalButtonHoverColor", "", "#1affffff")                                                           \
    X(tonalButtonPressedColor, "tonalButtonPressedColor", "", "#1f000000")                                                       \
    X(textButtonHoverColor, "textButtonHoverColor", "", "#143a6df0")                                                             \
    X(textButtonPressedColor, "textButtonPressedColor", "", "#1f3a6df0")                                                         \
    X(surfaceButtonHoverBorderColor, "surfaceButtonHoverBorderColor", "", "#663a6df0")                                           \
    X(scrollbarTrackColor, "scrollbar.trackColor", "", "#e8ecf1")                                                                \
    X(scrollbarThumbNormalColor, "scrollbar.thumbNormalColor", "", "#c0c6cf")                                                    \
    X(scrollbarThumbHoverColor, "scrollbar.thumbHoverColor", "", "#8b949e")                                                      \
    X(scrollbarThumbPressedColor, "scrollbar.thumbPressedColor", "", "#57606a")

// ========================================================================
// UTTheme — QML 层唯一主题入口
// ========================================================================

/**
 * @class UTTheme
 * @brief  QML 单例主题对象，提供便捷属性与主题切换方法
 *
 * QML 用法:
 *   color: UTTheme.primaryColor
 *   调用 UTTheme.nextTheme() / setTheme("dark") / toggleTheme()
 */
class UTTheme : public QObject
{
    Q_OBJECT
    // =====================================================================
    // QML 属性 — 使用独立通知信号，避免无关绑定刷新
    // =====================================================================
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged FINAL)
    Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged FINAL)
    Q_PROPERTY(bool isDark READ isDark NOTIFY isDarkChanged FINAL)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged FINAL)
#define UT_DECLARE_THEME_COLOR_PROPERTY(name, key, fallbackKey, fallback)                                                        \
    Q_PROPERTY(QColor name READ name NOTIFY name##Changed FINAL)
    UT_THEME_COLOR_PROPERTIES(UT_DECLARE_THEME_COLOR_PROPERTY)
#undef UT_DECLARE_THEME_COLOR_PROPERTY
    Q_PROPERTY(qreal radius READ radius NOTIFY radiusChanged FINAL)
    QML_SINGLETON
    QML_ELEMENT

public:
    /**
     * @brief QML 引擎工厂方法，返回全局唯一实例
     */
    static UTTheme *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    // ── 便捷属性访问（含 fallback 默认值）─────────────────────
    QVariantMap theme() const;
    QStringList availableThemes() const;
    bool isDark() const;
    QString fontFamily() const;
#define UT_DECLARE_THEME_COLOR_GETTER(name, key, fallbackKey, fallback) QColor name() const;
    UT_THEME_COLOR_PROPERTIES(UT_DECLARE_THEME_COLOR_GETTER)
#undef UT_DECLARE_THEME_COLOR_GETTER
    qreal radius() const;

    // ── 主题操作（QML 可直接调用）─────────────────────────────
    Q_INVOKABLE void nextTheme();
    Q_INVOKABLE void setTheme(const QString &key);
    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void registerTheme(const QString &key, const QVariantMap &palette);
    void setFontFamily(const QString &family);

signals:
    // =====================================================================
    // 主题变更 — theme Map 的兼容通知及强类型属性的精细通知
    // =====================================================================
    void themeChanged();
    void availableThemesChanged();
    void isDarkChanged();
#define UT_DECLARE_THEME_COLOR_SIGNAL(name, key, fallbackKey, fallback) void name##Changed();
    UT_THEME_COLOR_PROPERTIES(UT_DECLARE_THEME_COLOR_SIGNAL)
#undef UT_DECLARE_THEME_COLOR_SIGNAL
    void radiusChanged();
    void fontFamilyChanged();

private:
    explicit UTTheme(QObject *parent = nullptr);
    ~UTTheme() override;

    static UTTheme *s_instance; ///< 全局唯一实例

    QString m_current;
    QString m_fontFamily;
    QStringList m_keys;
    QHash<QString, QVariantMap> m_palettes;

    void initBuiltinPalettes();
    void notifyThemeChanges(const QVariantMap &previous, const QVariantMap &current);
    const QVariantMap &currentTheme() const;
    QColor colorValue(const QVariantMap &palette, const char *key, const char *fallbackKey, const QColor &fallback) const;
};
