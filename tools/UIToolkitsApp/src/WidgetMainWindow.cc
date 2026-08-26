/**
 * @file WidgetMainWindow.cc
 * @brief QWidget 主界面 demo 实现。
 */

#include "WidgetMainWindow.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <UIToolkits/UTButton.h>
#include <UIToolkits/UTLineEdit.h>

namespace
{
QWidget *makeButtonsPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Buttons"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *description = new QLabel(QStringLiteral("QWidget 版本的基础按钮组件"), page);
    layout->addWidget(description);

    auto *primary = new UIToolkits::UTButton(page);
    primary->setText(QStringLiteral("Primary Button"));
    layout->addWidget(primary, 0, Qt::AlignLeft);

    auto *disabled = new UIToolkits::UTButton(page);
    disabled->setText(QStringLiteral("Disabled Button"));
    disabled->setEnabled(false);
    layout->addWidget(disabled, 0, Qt::AlignLeft);

    layout->addStretch();
    return page;
}

QWidget *makeInputsPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QFormLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Inputs"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addRow(title);

    auto *lineEdit = new UIToolkits::UTLineEdit(page);
    lineEdit->setPlaceholderText(QStringLiteral("请输入内容"));
    layout->addRow(QStringLiteral("LineEdit"), lineEdit);

    auto *textEdit = new QPlainTextEdit(page);
    textEdit->setPlaceholderText(QStringLiteral("多行文本输入"));
    layout->addRow(QStringLiteral("TextEdit"), textEdit);
    return page;
}
} // namespace

WidgetMainWindow::WidgetMainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_navigation(new QListWidget(this)),
    m_pages(new QStackedWidget(this))
{
    setWindowTitle(QStringLiteral("UIToolkits · QWidget Demo"));
    resize(1000, 680);

    m_navigation->addItems({QStringLiteral("Buttons"), QStringLiteral("Inputs")});
    m_navigation->setFixedWidth(200);
    m_navigation->setCurrentRow(0);

    m_pages->addWidget(makeButtonsPage(m_pages));
    m_pages->addWidget(makeInputsPage(m_pages));

    auto *central = new QWidget(this);
    auto *layout  = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_navigation);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);

    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
}
