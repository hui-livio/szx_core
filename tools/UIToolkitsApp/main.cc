/**
 * @file main.cc
 * @brief UI 工具集示例入口 — 启动 UIToolkits QML 演示应用。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>
#include <memory>
#include "NativeWindow.h"
#include "src/model/NavTreeFilterModel.h"
#include "src/model/TreeModel.h"
#include "src/model/TreeNode.h"
#include "DemoLauncher.h"
#include "WidgetMainWindow.h"
#include <UIToolkits/UTWidgetTheme.h>

class ApplicationIconProvider : public QQuickImageProvider
{
public:
    ApplicationIconProvider() :
        QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString & /*id*/, QSize *size, const QSize &requestedSize) override
    {
        const QSize iconSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
        QPixmap pixmap       = QApplication::windowIcon().pixmap(iconSize);
        if (pixmap.isNull())
        {
            pixmap = QPixmap(iconSize);
            pixmap.fill(Qt::transparent);
        }

        if (size)
            *size = pixmap.size();
        return pixmap;
    }
};

static void centerWindowOnCurrentScreen(QWindow *window)
{
    if (!window)
        return;

    QScreen *screen = window->screen();
    if (!screen)
        screen = QApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const QSize size      = window->size();
    window->setPosition(available.center() - QPoint(size.width() / 2, size.height() / 2));
}

int main(int argc, char *argv[])
{
    /**
 * @brief 应用初始化
 */
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/UIToolkitsApp/resource/app.ico")));
    UIToolkits::UTWidgetTheme::apply(app, UIToolkits::UTWidgetThemeKind::Dark);

    // ========================================================================
    // 构建演示数据
    // ========================================================================
    QList<TreeNode> treeData;

    auto addNode =
        [&](const QString &title, const QString &icon, const QStringList &childTitles, const QStringList &childIcons = {})
    {
        TreeNode node;
        node.title = title;
        node.icon  = icon;
        for (int i = 0; i < childTitles.size(); ++i)
        {
            TreeNode child;
            child.title = childTitles.at(i);
            child.icon  = childIcons.size() > i ? childIcons.at(i) : QString();
            node.children.append(child);
        }
        treeData.append(node);
    };

    addNode("检测配置", "settings", {"普通检测", "深度检测", "快速检测", "自定义检测"});
    addNode("缺陷管理", "bug", {"连锡", "缺件", "偏移", "极反", "虚焊"});
    addNode("模板管理", "file", {"新建模板", "编辑模板", "删除模板", "导入模板", "导出模板"});
    addNode("报告导出", "chart", {"PDF报告", "Excel报告", "图片报告", "CSV数据"});
    addNode("系统设置", "gear", {"通用设置", "网络设置", "权限管理", "日志配置"});
    addNode("用户管理", "user", {"添加用户", "编辑用户", "删除用户", "角色分配"});
    addNode("日志查看", "list", {"操作日志", "错误日志", "检测日志", "系统日志"});

    TreeModel treeModel;
    treeModel.setTreeData(treeData);

    NavTreeFilterModel filterModel;
    filterModel.setSourceModel(&treeModel);

    /**
 * @brief
 * @param QML  引擎与窗口
 */
    installNativeWindowFilter();

    QQmlApplicationEngine engine;
    auto iconProvider = std::make_unique<ApplicationIconProvider>();
    engine.addImageProvider(QStringLiteral("application"), iconProvider.release());
    engine.rootContext()->setContextProperty(QStringLiteral("appIconSource"), QStringLiteral("image://application/icon"));
    engine.rootContext()->setContextProperty("treeModel", &treeModel);
    engine.rootContext()->setContextProperty("filterTreeModel", &filterModel);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("UIToolkitsApp", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    QWindow *mainWindow = qobject_cast<QWindow *>(engine.rootObjects().first());
    if (!mainWindow)
        return -1;

    mainWindow->setIcon(QApplication::windowIcon());
    mainWindow->setVisibility(QWindow::Hidden);
    setupNativeWindow(mainWindow);
    centerWindowOnCurrentScreen(mainWindow);

    WidgetMainWindow widgetWindow;
    widgetWindow.setWindowIcon(QApplication::windowIcon());
    // QWidget exposes its native QWindow after winId() creates the HWND.
    // Configure the same frameless/DWM policy used by the QML window before
    // the launcher can show it.
    widgetWindow.winId();
    setupNativeWindow(widgetWindow.windowHandle());
    DemoLauncher launcher(mainWindow, &widgetWindow);
    launcher.show();

    /**
 * @brief 事件循环
 */
    return app.exec();
}
