/**
 * @file DemoLauncher.cc
 * @brief QML 与 QWidget 双主界面选择器实现。
 */

#include "DemoLauncher.h"

#include "WidgetMainWindow.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

DemoLauncher::DemoLauncher(QWindow *qmlWindow, WidgetMainWindow *widgetWindow, QWidget *parent) :
    QWidget(parent),
    m_qmlWindow(qmlWindow),
    m_widgetWindow(widgetWindow)
{
    setWindowTitle(QStringLiteral("UIToolkits Demo"));
    setFixedSize(420, 260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("UIToolkits Demo Gallery"), this);
    title->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600;"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("请选择要展示的 UI 实现"), this);
    layout->addWidget(hint);

    auto *qmlButton    = new QPushButton(QStringLiteral("打开 QML 主界面"), this);
    auto *widgetButton = new QPushButton(QStringLiteral("打开 QWidget 主界面"), this);
    layout->addWidget(qmlButton);
    layout->addWidget(widgetButton);
    layout->addStretch();

    connect(qmlButton, &QPushButton::clicked, this,
            [this]()
            {
                if (m_qmlWindow)
                {
                    m_qmlWindow->show();
                    m_qmlWindow->raise();
                    m_qmlWindow->requestActivate();
                }
            });
    connect(widgetButton, &QPushButton::clicked, this,
            [this]()
            {
                if (m_widgetWindow)
                {
                    m_widgetWindow->show();
                    m_widgetWindow->raise();
                    m_widgetWindow->activateWindow();
                }
            });
}
