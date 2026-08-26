/**
 * @file DemoLauncher.h
 * @brief QML 与 QWidget 双主界面选择器。
 */

#pragma once

#include <QWidget>

class QWindow;
class WidgetMainWindow;

class DemoLauncher final : public QWidget
{
    Q_OBJECT

public:
    DemoLauncher(QWindow *qmlWindow, WidgetMainWindow *widgetWindow, QWidget *parent = nullptr);

private:
    QWindow *m_qmlWindow             = nullptr;
    WidgetMainWindow *m_widgetWindow = nullptr;
};
