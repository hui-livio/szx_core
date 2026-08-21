/**
 * @file UTTheme.cc
 * @brief QML 主题单例实现 — 管理主题数据并提供强类型属性
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2025-06-24
 * @version 1.0
 */

#include "UTTheme.h"

#include <QCoreApplication>
#include <QDebug>

// ========================================================================
// 单例管理
// ========================================================================

UTTheme *UTTheme::s_instance = nullptr;

UTTheme::UTTheme(QObject *parent) :
    QObject(parent)
{
    initBuiltinPalettes();
}

UTTheme::~UTTheme()
{
    s_instance = nullptr;
}

UTTheme *UTTheme::create(QQmlEngine * /*engine*/, QJSEngine * /*scriptEngine*/)
{
    if (!s_instance)
    {
        s_instance = new UTTheme(QCoreApplication::instance());
    }
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString UTTheme::fontFamily() const
{
    return m_fontFamily;
}

void UTTheme::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    emit fontFamilyChanged();
}

void UTTheme::initBuiltinPalettes()
{
    m_keys    = {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("warm"), QStringLiteral("forest")};
    m_current = QStringLiteral("dark");

    const auto color = [](const char *hex)
    {
        return QVariant::fromValue(QColor(hex));
    };
    const auto setColor = [&](QVariantMap &palette, const QString &key, const QString &hex)
    {
        palette[key] = color(hex.toUtf8().constData());
    };
    const auto registerPalette = [this](const QString &key, QVariantMap palette)
    {
        m_palettes.insert(key, palette);
#ifdef QT_DEBUG
        qDebug().noquote() << "[UTTheme] palette loaded:" << key << "name=" << palette.value("name").toString()
                           << "isDark=" << palette.value("isDark").toBool();
#endif
    };

    QVariantMap light;
    light["name"] = QStringLiteral("Light");
    setColor(light, QStringLiteral("backgroundColor"), QStringLiteral("#f5f7fa"));
    setColor(light, QStringLiteral("primaryColor"), QStringLiteral("#3a6df0"));
    setColor(light, QStringLiteral("focusColor"), QStringLiteral("#3a6df0"));
    setColor(light, QStringLiteral("successColor"), QStringLiteral("#16a34a"));
    setColor(light, QStringLiteral("errorColor"), QStringLiteral("#dc2626"));
    setColor(light, QStringLiteral("warningColor"), QStringLiteral("#d97706"));
    setColor(light, QStringLiteral("infoColor"), QStringLiteral("#0284c7"));
    setColor(light, QStringLiteral("primaryTextColor"), QStringLiteral("#181d27"));
    setColor(light, QStringLiteral("secondaryTextColor"), QStringLiteral("#5a6577"));
    setColor(light, QStringLiteral("surfaceColor"), QStringLiteral("#e8ecf2"));
    setColor(light, QStringLiteral("borderColor"), QStringLiteral("#d5dbe3"));
    setColor(light, QStringLiteral("shadowColor"), QStringLiteral("#18000000"));
    light["radius"] = 8;
    light["isDark"] = false;
    setColor(light, QStringLiteral("titleBarColor"), QStringLiteral("#e8ecf2"));
    setColor(light, QStringLiteral("surfaceButtonHoverColor"), QStringLiteral("#14000000"));
    setColor(light, QStringLiteral("surfaceButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(light, QStringLiteral("filledButtonHoverColor"), QStringLiteral("#24ffffff"));
    setColor(light, QStringLiteral("filledButtonPressedColor"), QStringLiteral("#26000000"));
    setColor(light, QStringLiteral("tonalButtonHoverColor"), QStringLiteral("#1affffff"));
    setColor(light, QStringLiteral("tonalButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(light, QStringLiteral("textButtonHoverColor"), QStringLiteral("#143a6df0"));
    setColor(light, QStringLiteral("textButtonPressedColor"), QStringLiteral("#1f3a6df0"));
    setColor(light, QStringLiteral("surfaceButtonHoverBorderColor"), QStringLiteral("#663a6df0"));
    setColor(light, QStringLiteral("scrollbar.trackColor"), QStringLiteral("#e8ecf1"));
    setColor(light, QStringLiteral("scrollbar.thumbNormalColor"), QStringLiteral("#c0c6cf"));
    setColor(light, QStringLiteral("scrollbar.thumbHoverColor"), QStringLiteral("#8b949e"));
    setColor(light, QStringLiteral("scrollbar.thumbPressedColor"), QStringLiteral("#57606a"));
    registerPalette(QStringLiteral("light"), light);

    QVariantMap dark;
    dark["name"] = QStringLiteral("Dark");
    setColor(dark, QStringLiteral("backgroundColor"), QStringLiteral("#0f1217"));
    setColor(dark, QStringLiteral("primaryColor"), QStringLiteral("#6d9fff"));
    setColor(dark, QStringLiteral("focusColor"), QStringLiteral("#6d9fff"));
    setColor(dark, QStringLiteral("successColor"), QStringLiteral("#22c55e"));
    setColor(dark, QStringLiteral("errorColor"), QStringLiteral("#ef4444"));
    setColor(dark, QStringLiteral("warningColor"), QStringLiteral("#f59e0b"));
    setColor(dark, QStringLiteral("infoColor"), QStringLiteral("#38bdf8"));
    setColor(dark, QStringLiteral("primaryTextColor"), QStringLiteral("#e6edf5"));
    setColor(dark, QStringLiteral("secondaryTextColor"), QStringLiteral("#8b949e"));
    setColor(dark, QStringLiteral("surfaceColor"), QStringLiteral("#1a1e25"));
    setColor(dark, QStringLiteral("borderColor"), QStringLiteral("#404854"));
    setColor(dark, QStringLiteral("shadowColor"), QStringLiteral("#80000000"));
    dark["radius"] = 8;
    dark["isDark"] = true;
    setColor(dark, QStringLiteral("titleBarColor"), QStringLiteral("#161a22"));
    setColor(dark, QStringLiteral("surfaceButtonHoverColor"), QStringLiteral("#1fffffff"));
    setColor(dark, QStringLiteral("surfaceButtonPressedColor"), QStringLiteral("#2effffff"));
    setColor(dark, QStringLiteral("filledButtonHoverColor"), QStringLiteral("#24ffffff"));
    setColor(dark, QStringLiteral("filledButtonPressedColor"), QStringLiteral("#26000000"));
    setColor(dark, QStringLiteral("tonalButtonHoverColor"), QStringLiteral("#1affffff"));
    setColor(dark, QStringLiteral("tonalButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(dark, QStringLiteral("textButtonHoverColor"), QStringLiteral("#146d9fff"));
    setColor(dark, QStringLiteral("textButtonPressedColor"), QStringLiteral("#1f6d9fff"));
    setColor(dark, QStringLiteral("surfaceButtonHoverBorderColor"), QStringLiteral("#59ffffff"));
    setColor(dark, QStringLiteral("scrollbar.trackColor"), QStringLiteral("#1e2229"));
    setColor(dark, QStringLiteral("scrollbar.thumbNormalColor"), QStringLiteral("#4a5058"));
    setColor(dark, QStringLiteral("scrollbar.thumbHoverColor"), QStringLiteral("#6a7078"));
    setColor(dark, QStringLiteral("scrollbar.thumbPressedColor"), QStringLiteral("#8a9098"));
    registerPalette(QStringLiteral("dark"), dark);

    QVariantMap warm;
    warm["name"] = QStringLiteral("Warm");
    setColor(warm, QStringLiteral("backgroundColor"), QStringLiteral("#fcf9f5"));
    setColor(warm, QStringLiteral("primaryColor"), QStringLiteral("#e0732f"));
    setColor(warm, QStringLiteral("focusColor"), QStringLiteral("#e0732f"));
    setColor(warm, QStringLiteral("successColor"), QStringLiteral("#2f9e44"));
    setColor(warm, QStringLiteral("errorColor"), QStringLiteral("#dc2626"));
    setColor(warm, QStringLiteral("warningColor"), QStringLiteral("#e0732f"));
    setColor(warm, QStringLiteral("infoColor"), QStringLiteral("#0e7490"));
    setColor(warm, QStringLiteral("primaryTextColor"), QStringLiteral("#231c16"));
    setColor(warm, QStringLiteral("secondaryTextColor"), QStringLiteral("#6b5543"));
    setColor(warm, QStringLiteral("surfaceColor"), QStringLiteral("#f5ede3"));
    setColor(warm, QStringLiteral("borderColor"), QStringLiteral("#dfd3c5"));
    setColor(warm, QStringLiteral("shadowColor"), QStringLiteral("#18000000"));
    warm["radius"] = 8;
    warm["isDark"] = false;
    setColor(warm, QStringLiteral("titleBarColor"), QStringLiteral("#f2e8db"));
    setColor(warm, QStringLiteral("surfaceButtonHoverColor"), QStringLiteral("#14000000"));
    setColor(warm, QStringLiteral("surfaceButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(warm, QStringLiteral("filledButtonHoverColor"), QStringLiteral("#24ffffff"));
    setColor(warm, QStringLiteral("filledButtonPressedColor"), QStringLiteral("#26000000"));
    setColor(warm, QStringLiteral("tonalButtonHoverColor"), QStringLiteral("#1affffff"));
    setColor(warm, QStringLiteral("tonalButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(warm, QStringLiteral("textButtonHoverColor"), QStringLiteral("#14e0732f"));
    setColor(warm, QStringLiteral("textButtonPressedColor"), QStringLiteral("#1fe0732f"));
    setColor(warm, QStringLiteral("surfaceButtonHoverBorderColor"), QStringLiteral("#73b45a1e"));
    setColor(warm, QStringLiteral("scrollbar.trackColor"), QStringLiteral("#e8ddd0"));
    setColor(warm, QStringLiteral("scrollbar.thumbNormalColor"), QStringLiteral("#c0b098"));
    setColor(warm, QStringLiteral("scrollbar.thumbHoverColor"), QStringLiteral("#8b7a68"));
    setColor(warm, QStringLiteral("scrollbar.thumbPressedColor"), QStringLiteral("#6b5a48"));
    registerPalette(QStringLiteral("warm"), warm);

    QVariantMap forest;
    forest["name"] = QStringLiteral("Forest");
    setColor(forest, QStringLiteral("backgroundColor"), QStringLiteral("#f2f7f3"));
    setColor(forest, QStringLiteral("primaryColor"), QStringLiteral("#2e9e5b"));
    setColor(forest, QStringLiteral("focusColor"), QStringLiteral("#2e9e5b"));
    setColor(forest, QStringLiteral("successColor"), QStringLiteral("#2e9e5b"));
    setColor(forest, QStringLiteral("errorColor"), QStringLiteral("#dc2626"));
    setColor(forest, QStringLiteral("warningColor"), QStringLiteral("#ca8a04"));
    setColor(forest, QStringLiteral("infoColor"), QStringLiteral("#0891b2"));
    setColor(forest, QStringLiteral("primaryTextColor"), QStringLiteral("#17241c"));
    setColor(forest, QStringLiteral("secondaryTextColor"), QStringLiteral("#4d6654"));
    setColor(forest, QStringLiteral("surfaceColor"), QStringLiteral("#e3ede6"));
    setColor(forest, QStringLiteral("borderColor"), QStringLiteral("#ced9d0"));
    setColor(forest, QStringLiteral("shadowColor"), QStringLiteral("#18000000"));
    forest["radius"] = 8;
    forest["isDark"] = false;
    setColor(forest, QStringLiteral("titleBarColor"), QStringLiteral("#dce8df"));
    setColor(forest, QStringLiteral("surfaceButtonHoverColor"), QStringLiteral("#14000000"));
    setColor(forest, QStringLiteral("surfaceButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(forest, QStringLiteral("filledButtonHoverColor"), QStringLiteral("#24ffffff"));
    setColor(forest, QStringLiteral("filledButtonPressedColor"), QStringLiteral("#26000000"));
    setColor(forest, QStringLiteral("tonalButtonHoverColor"), QStringLiteral("#1affffff"));
    setColor(forest, QStringLiteral("tonalButtonPressedColor"), QStringLiteral("#1f000000"));
    setColor(forest, QStringLiteral("textButtonHoverColor"), QStringLiteral("#142e9e5b"));
    setColor(forest, QStringLiteral("textButtonPressedColor"), QStringLiteral("#1f2e9e5b"));
    setColor(forest, QStringLiteral("surfaceButtonHoverBorderColor"), QStringLiteral("#7326824b"));
    setColor(forest, QStringLiteral("scrollbar.trackColor"), QStringLiteral("#d8e5da"));
    setColor(forest, QStringLiteral("scrollbar.thumbNormalColor"), QStringLiteral("#b0c8b4"));
    setColor(forest, QStringLiteral("scrollbar.thumbHoverColor"), QStringLiteral("#7a9a80"));
    setColor(forest, QStringLiteral("scrollbar.thumbPressedColor"), QStringLiteral("#507058"));
    registerPalette(QStringLiteral("forest"), forest);
}

void UTTheme::notifyThemeChanges(const QVariantMap &previous, const QVariantMap &current)
{
    const auto notifyIfChanged = [this, &previous, &current](const char *key, void (UTTheme::*signal)())
    {
        if (previous.value(key) != current.value(key))
            (this->*signal)();
    };

    notifyIfChanged("isDark", &UTTheme::isDarkChanged);
#define UT_NOTIFY_THEME_COLOR(name, key, fallbackKey, fallback)                                                                  \
    if (colorValue(previous, key, fallbackKey, QColor(fallback)) != colorValue(current, key, fallbackKey, QColor(fallback)))     \
        emit name##Changed();
    UT_THEME_COLOR_PROPERTIES(UT_NOTIFY_THEME_COLOR)
#undef UT_NOTIFY_THEME_COLOR
    notifyIfChanged("radius", &UTTheme::radiusChanged);

    if (previous != current)
        emit themeChanged();
}

QColor UTTheme::colorValue(const QVariantMap &palette, const char *key, const char *fallbackKey, const QColor &fallback) const
{
    const QVariant value = palette.value(key);
    if (value.isValid())
        return value.value<QColor>();

    if (*fallbackKey != '\0')
    {
        const QVariant fallbackValue = palette.value(fallbackKey);
        if (fallbackValue.isValid())
            return fallbackValue.value<QColor>();
    }

    return fallback;
}

const QVariantMap &UTTheme::currentTheme() const
{
    static const QVariantMap emptyTheme;
    const auto it = m_palettes.constFind(m_current);
    return it == m_palettes.cend() ? emptyTheme : it.value();
}

// ========================================================================
// 便捷属性访问（含 fallback 默认值，与原 UTTheme.qml 行为一致）
// ========================================================================

QVariantMap UTTheme::theme() const
{
    return currentTheme();
}

QStringList UTTheme::availableThemes() const
{
    return m_keys;
}

bool UTTheme::isDark() const
{
    return currentTheme().value("isDark", false).toBool();
}

#define UT_DEFINE_THEME_COLOR_GETTER(name, key, fallbackKey, fallback)                                                           \
    QColor UTTheme::name() const                                                                                                 \
    {                                                                                                                            \
        return colorValue(currentTheme(), key, fallbackKey, QColor(fallback));                                                   \
    }
UT_THEME_COLOR_PROPERTIES(UT_DEFINE_THEME_COLOR_GETTER)
#undef UT_DEFINE_THEME_COLOR_GETTER

qreal UTTheme::radius() const
{
    const QVariant v = currentTheme().value("radius");
    return v.isValid() ? v.toReal() : 8.0;
}

// ========================================================================
// 主题操作
// ========================================================================

void UTTheme::nextTheme()
{
    if (m_keys.isEmpty())
        return;

    const int currentIndex = m_keys.indexOf(m_current);
    const int nextIndex    = currentIndex < 0 ? 0 : (currentIndex + 1) % m_keys.size();
    setTheme(m_keys.at(nextIndex));
}

void UTTheme::setTheme(const QString &key)
{
    if (key.isEmpty() || !m_palettes.contains(key) || m_current == key)
        return;

    const QVariantMap previous = theme();
    m_current                  = key;
    notifyThemeChanges(previous, theme());
}

void UTTheme::toggleTheme()
{
    setTheme(isDark() ? QStringLiteral("light") : QStringLiteral("dark"));
}

void UTTheme::registerTheme(const QString &key, const QVariantMap &palette)
{
    if (key.isEmpty())
        return;

    const bool isNewTheme      = !m_palettes.contains(key);
    const bool affectsCurrent  = key == m_current;
    const QVariantMap previous = affectsCurrent ? theme() : QVariantMap{};
    m_palettes.insert(key, palette);

    if (isNewTheme)
    {
        m_keys.append(key);
        emit availableThemesChanged();
    }

    if (affectsCurrent)
        notifyThemeChanges(previous, palette);
}
