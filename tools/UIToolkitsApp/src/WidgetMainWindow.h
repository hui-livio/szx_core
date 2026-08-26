/**
 * @file WidgetMainWindow.h
 * @brief QWidget 主界面 demo。
 */

#pragma once

#include <QMainWindow>

class QStackedWidget;
class QListWidget;

class WidgetMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit WidgetMainWindow(QWidget *parent = nullptr);

private:
    QListWidget *m_navigation = nullptr;
    QStackedWidget *m_pages   = nullptr;
};
