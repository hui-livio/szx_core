/**
 * @file WidgetMainWindow.cc
 * @brief QWidget 主界面 demo 实现。
 */

#include "WidgetMainWindow.h"

#include <QApplication>
#include <QColor>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <UIToolkits/UTButton.h>
#include <UIToolkits/UTCard.h>
#include <UIToolkits/UTCheckBox.h>
#include <UIToolkits/UTComboBox.h>
#include <UIToolkits/UTLineEdit.h>
#include <UIToolkits/UTLabel.h>
#include <UIToolkits/UTProgressBar.h>
#include <UIToolkits/UTBadge.h>
#include <UIToolkits/UTLoading.h>
#include <UIToolkits/UTMarqueeLabel.h>
#include <UIToolkits/UTToast.h>
#include <UIToolkits/UTImageAvatar.h>
#include <UIToolkits/UTStatCard.h>
#include <UIToolkits/UTKeyValueGrid.h>
#include <UIToolkits/UTImageViewer.h>
#include <UIToolkits/UTTabButton.h>
#include <UIToolkits/UTSliderValue.h>
#include <UIToolkits/UTMessageDialog.h>
#include <UIToolkits/UTTitleBar.h>
#include <UIToolkits/UTSlider.h>
#include <UIToolkits/UTSpinBox.h>
#include <UIToolkits/UTSwitch.h>
#include <UIToolkits/UTWidgetTheme.h>

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

    auto *description = new UIToolkits::UTLabel(page);
    description->setText(QStringLiteral("QWidget 版本的基础按钮组件"));
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

QWidget *makeSelectionsPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QFormLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Selections"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addRow(title);

    auto *checkBox = new UIToolkits::UTCheckBox(page);
    checkBox->setText(QStringLiteral("启用自动检测"));
    checkBox->setChecked(true);
    layout->addRow(QStringLiteral("CheckBox"), checkBox);

    auto *switchBox = new UIToolkits::UTSwitch(page);
    switchBox->setText(QStringLiteral("实时监控"));
    switchBox->setChecked(true);
    layout->addRow(QStringLiteral("Switch"), switchBox);

    auto *comboBox = new UIToolkits::UTComboBox(page);
    comboBox->addItems({QStringLiteral("普通检测"), QStringLiteral("深度检测"), QStringLiteral("快速检测")});
    layout->addRow(QStringLiteral("ComboBox"), comboBox);

    auto *slider = new UIToolkits::UTSlider(Qt::Horizontal, page);
    slider->setRange(0, 100);
    slider->setValue(60);
    layout->addRow(QStringLiteral("Slider"), slider);

    auto *spinBox = new UIToolkits::UTSpinBox(page);
    spinBox->setRange(0, 999);
    spinBox->setValue(20);
    layout->addRow(QStringLiteral("SpinBox"), spinBox);
    return page;
}

QWidget *makeDisplayPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Display"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *card = new UIToolkits::UTCard(page);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->addWidget(new QLabel(QStringLiteral("当前状态：运行中"), card));
    auto *progress = new UIToolkits::UTProgressBar(card);
    progress->setRange(0, 100);
    progress->setValue(72);
    cardLayout->addWidget(progress);
    layout->addWidget(card);
    auto *stat = new UIToolkits::UTStatCard(page);
    stat->setTitle(QStringLiteral("检测通过率"));
    stat->setValue(QStringLiteral("98.6%"));
    stat->setMaximumWidth(220);
    layout->addWidget(stat, 0, Qt::AlignLeft);
    auto *grid = new UIToolkits::UTKeyValueGrid(page);
    grid->addRow(QStringLiteral("设备"), QStringLiteral("AOI-01"));
    grid->addRow(QStringLiteral("耗时"), QStringLiteral("12.4 ms"));
    layout->addWidget(grid, 0, Qt::AlignLeft);
    auto *viewer = new UIToolkits::UTImageViewer(page);
    QPixmap preview(160, 80); preview.fill(QColor(QStringLiteral("#dbeafe")));
    viewer->setFixedSize(180, 100); viewer->setPixmap(preview);
    layout->addWidget(viewer, 0, Qt::AlignLeft);

    auto *badge = new UIToolkits::UTBadge(page);
    badge->setText(QStringLiteral("SUCCESS"));
    badge->setTone(UIToolkits::UTBadge::Tone::Success);
    layout->addWidget(badge, 0, Qt::AlignLeft);
    auto *avatar = new UIToolkits::UTImageAvatar(page);
    avatar->setFixedSize(48, 48);
    avatar->setText(QStringLiteral("U"));
    layout->addWidget(avatar, 0, Qt::AlignLeft);
    auto *marquee = new UIToolkits::UTMarqueeLabel(page);
    marquee->setText(QStringLiteral("UIToolkits QWidget 控件正在运行 · 默认 Qt 样式 · 与 QML 共用 DLL"));
    marquee->setMinimumWidth(360);
    layout->addWidget(marquee);
    layout->addStretch();
    return page;
}

QWidget *makeFeedbackPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Feedback"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *status = new QLabel(QStringLiteral("Ready"), page);
    layout->addWidget(status);
    auto *loading = new UIToolkits::UTLoading(page);
    layout->addWidget(loading);
    auto *toast = new UIToolkits::UTToast(page);

    auto *toastButton = new UIToolkits::UTButton(page);
    toastButton->setText(QStringLiteral("Show feedback"));
    layout->addWidget(toastButton, 0, Qt::AlignLeft);
    QObject::connect(toastButton, &QPushButton::clicked, page, [status, toast, loading]()
    {
        status->setText(QStringLiteral("操作已完成"));
        loading->start();
        toast->showMessage(QStringLiteral("操作已完成"));
        QTimer::singleShot(1500, status, [status, loading]() { status->setText(QStringLiteral("Ready")); loading->stop(); });
    });

    auto *messageButton = new QPushButton(QStringLiteral("Show message box"), page);
    layout->addWidget(messageButton, 0, Qt::AlignLeft);
    QObject::connect(messageButton, &QPushButton::clicked, page, [page]()
    {
        QMessageBox::information(page, QStringLiteral("UIToolkits"), QStringLiteral("这是一条 QWidget feedback 消息。"));
    });
    auto *dialog = new UIToolkits::UTMessageDialog(page);
    dialog->setWindowTitle(QStringLiteral("UTMessageDialog"));
    dialog->setMessage(QStringLiteral("这是 UIToolkits QWidget 消息对话框示例。"));
    auto *dialogButton = new UIToolkits::UTButton(page);
    dialogButton->setText(QStringLiteral("Show UTMessageDialog"));
    layout->addWidget(dialogButton, 0, Qt::AlignLeft);
    QObject::connect(dialogButton, &QPushButton::clicked, dialog, [dialog]() { dialog->exec(); });
    auto *tab = new UIToolkits::UTTabButton(page);
    tab->setText(QStringLiteral("运行模式"));
    tab->setActive(true);
    layout->addWidget(tab, 0, Qt::AlignLeft);
    auto *slider = new UIToolkits::UTSliderValue(Qt::Horizontal, page);
    slider->setRange(0, 100); slider->setValue(65); slider->setFixedWidth(240);
    layout->addWidget(slider, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

QWidget *makeDialogsPage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Dialogs"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *dialogButton = new UIToolkits::UTButton(page);
    dialogButton->setText(QStringLiteral("Open dialog"));
    layout->addWidget(dialogButton, 0, Qt::AlignLeft);
    QObject::connect(dialogButton, &QPushButton::clicked, page, [page]()
    {
        QDialog dialog(page);
        dialog.setWindowTitle(QStringLiteral("UTDialog"));
        auto *dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->addWidget(new QLabel(QStringLiteral("QWidget 对话框示例"), &dialog));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        dialogLayout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.exec();
    });
    layout->addStretch();
    return page;
}

QWidget *makeThemePage(QWidget *parent)
{
    auto *page   = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Theme"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("Widget demo 的主题切换会更新应用级调色板。"), page);
    layout->addWidget(hint);

    auto *toggleButton = new UIToolkits::UTButton(page);
    toggleButton->setText(QStringLiteral("Toggle light / dark palette"));
    layout->addWidget(toggleButton, 0, Qt::AlignLeft);
    QObject::connect(toggleButton, &QPushButton::clicked, page, []()
    {
        static auto theme = UIToolkits::UTWidgetThemeKind::Dark;
        theme = theme == UIToolkits::UTWidgetThemeKind::Dark ? UIToolkits::UTWidgetThemeKind::Light
                                                              : UIToolkits::UTWidgetThemeKind::Dark;
        if (auto *application = qobject_cast<QApplication *>(QCoreApplication::instance()))
            UIToolkits::UTWidgetTheme::apply(*application, theme);
    });
    layout->addStretch();
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

    m_navigation->addItems({QStringLiteral("Buttons"),
                            QStringLiteral("Inputs"),
                            QStringLiteral("Selections"),
                            QStringLiteral("Display"),
                            QStringLiteral("Feedback"),
                            QStringLiteral("Dialogs"),
                            QStringLiteral("Theme")});
    m_navigation->setFixedWidth(200);
    m_navigation->setCurrentRow(0);

    m_pages->addWidget(makeButtonsPage(m_pages));
    m_pages->addWidget(makeInputsPage(m_pages));
    m_pages->addWidget(makeSelectionsPage(m_pages));
    m_pages->addWidget(makeDisplayPage(m_pages));
    m_pages->addWidget(makeFeedbackPage(m_pages));
    m_pages->addWidget(makeDialogsPage(m_pages));
    m_pages->addWidget(makeThemePage(m_pages));

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    auto *titleBar = new UIToolkits::UTTitleBar(central);
    titleBar->setTitle(QStringLiteral("UIToolkits · QWidget Demo"));
    rootLayout->addWidget(titleBar);
    auto *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addWidget(m_navigation);
    contentLayout->addWidget(m_pages, 1);
    rootLayout->addLayout(contentLayout, 1);
    setCentralWidget(central);

    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(titleBar, &UIToolkits::UTTitleBar::closeRequested, this, &QWidget::close);
}
