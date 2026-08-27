#include "UIToolkits/UTAppWindow.h"
#include "UIToolkits/UTBadge.h"
#include "UIToolkits/UTCalendar.h"
#include "UIToolkits/UTFlickable.h"
#include "UIToolkits/UTFootBar.h"
#include "UIToolkits/UTImageAvatar.h"
#include "UIToolkits/UTImageViewer.h"
#include "UIToolkits/UTKeyValueGrid.h"
#include "UIToolkits/UTLoading.h"
#include "UIToolkits/UTMarqueeLabel.h"
#include "UIToolkits/UTMessageDialog.h"
#include "UIToolkits/UTScrollView.h"
#include "UIToolkits/UTSliderValue.h"
#include "UIToolkits/UTStatCard.h"
#include "UIToolkits/UTTabButton.h"
#include "UIToolkits/UTTitleBar.h"
#include "UIToolkits/UTTitleBarButtonGroup.h"
#include "UIToolkits/UTToast.h"
#include "UIToolkits/UTTooltip.h"
#include <QApplication>
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace UIToolkits
{
UTAppWindow::UTAppWindow(QWidget *parent) : QMainWindow(parent) {}
UTTitleBar::UTTitleBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(40);
    setObjectName("utTitleBar");
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 6, 0);
    auto *label = new QLabel(this);
    label->setObjectName("titleLabel");
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(label);
    auto *close = new QToolButton(this);
    close->setText(QStringLiteral("×"));
    close->setAutoRaise(true);
    close->setFixedSize(30, 30);
    layout->addWidget(close);
    connect(close, &QToolButton::clicked, this, &UTTitleBar::closeRequested);
}
void UTTitleBar::setTitle(const QString &title) { if (auto *label = findChild<QLabel *>(QStringLiteral("titleLabel"))) label->setText(title); }
QString UTTitleBar::title() const { if (auto *label = findChild<QLabel *>(QStringLiteral("titleLabel"))) return label->text(); return {}; }
void UTTitleBar::mousePressEvent(QMouseEvent *event) { if (event->button() == Qt::LeftButton) setProperty("utDragPos", event->globalPosition().toPoint()); QWidget::mousePressEvent(event); }
void UTTitleBar::mouseMoveEvent(QMouseEvent *event) { const auto pos = property("utDragPos"); if (pos.isValid() && (event->buttons() & Qt::LeftButton)) { const auto delta = event->globalPosition().toPoint() - pos.toPoint(); if (window()) window()->move(window()->pos() + delta); setProperty("utDragPos", event->globalPosition().toPoint()); } QWidget::mouseMoveEvent(event); }
UTTitleBarButtonGroup::UTTitleBarButtonGroup(QWidget *parent) : QWidget(parent) {}
UTFootBar::UTFootBar(QWidget *parent) : QWidget(parent) {}
UTBadge::UTBadge(QWidget *parent) : QLabel(parent) { setTone(Tone::Neutral); setAlignment(Qt::AlignCenter); setMinimumHeight(22); setContentsMargins(8, 1, 8, 1); }
void UTBadge::setTone(Tone tone) { setProperty("tone", static_cast<int>(tone)); const char *colors[] = {"#64748b", "#16a34a", "#d97706", "#dc2626", "#2563eb"}; setStyleSheet(QStringLiteral("QLabel { color: white; background: %1; border-radius: 11px; }").arg(colors[static_cast<int>(tone)])); }
UTBadge::Tone UTBadge::tone() const { return static_cast<Tone>(property("tone").toInt()); }
UTStatCard::UTStatCard(QWidget *parent) : QFrame(parent) {}
UTKeyValueGrid::UTKeyValueGrid(QWidget *parent) : QWidget(parent) {}
UTScrollView::UTScrollView(QWidget *parent) : QScrollArea(parent) {}
UTFlickable::UTFlickable(QWidget *parent) : QScrollArea(parent) {}
UTImageAvatar::UTImageAvatar(QWidget *parent) : QLabel(parent) { setAlignment(Qt::AlignCenter); setMinimumSize(32, 32); setScaledContents(false); }
void UTImageAvatar::paintEvent(QPaintEvent *) { QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); QPainterPath clip; clip.addEllipse(rect()); painter.setClipPath(clip); if (!pixmap().isNull()) { const auto scaled = pixmap().scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation); painter.drawPixmap((width()-scaled.width())/2, (height()-scaled.height())/2, scaled); } else { painter.fillRect(rect(), palette().mid()); painter.setPen(palette().brightText().color()); painter.drawText(rect(), Qt::AlignCenter, text().left(1).toUpper()); } }
UTImageViewer::UTImageViewer(QWidget *parent) : QScrollArea(parent) {}
UTLoading::UTLoading(QWidget *parent) : QProgressBar(parent) { setRange(0, 0); setTextVisible(false); setFixedHeight(4); stop(); }
void UTLoading::start() { setProperty("active", true); show(); }
void UTLoading::stop() { setProperty("active", false); hide(); }
bool UTLoading::isActive() const { return property("active").toBool(); }
UTToast::UTToast(QWidget *parent) : QLabel(parent) { setAlignment(Qt::AlignCenter); setWordWrap(true); setMargin(10); setStyleSheet(QStringLiteral("QLabel { color: white; background: #334155; border-radius: 8px; }") ); hide(); }
void UTToast::showMessage(const QString &message, int durationMs) { setText(message); adjustSize(); if (parentWidget()) move((parentWidget()->width()-width())/2, parentWidget()->height()-height()-24); show(); raise(); auto *timer = findChild<QTimer *>(QStringLiteral("hideTimer")); if (!timer) { timer = new QTimer(this); timer->setObjectName(QStringLiteral("hideTimer")); timer->setSingleShot(true); connect(timer, &QTimer::timeout, this, &QWidget::hide); } timer->start(qMax(0, durationMs)); }
UTTooltip::UTTooltip(QWidget *parent) : QLabel(parent) {}
UTMessageDialog::UTMessageDialog(QWidget *parent) : QDialog(parent) {}
UTCalendar::UTCalendar(QWidget *parent) : QCalendarWidget(parent) {}
UTMarqueeLabel::UTMarqueeLabel(QWidget *parent) : QLabel(parent) { setProperty("offset", 0); setProperty("speed", 1); setActive(true); }
void UTMarqueeLabel::setActive(bool active) { setProperty("active", active); auto *timer = findChild<QTimer *>(QStringLiteral("marqueeTimer")); if (!timer) { timer = new QTimer(this); timer->setObjectName(QStringLiteral("marqueeTimer")); connect(timer, &QTimer::timeout, this, [this] { setProperty("offset", property("offset").toInt() + property("speed").toInt()); update(); }); } active ? timer->start(30) : timer->stop(); }
bool UTMarqueeLabel::isActive() const { return property("active").toBool(); }
void UTMarqueeLabel::setSpeed(int pixelsPerTick) { setProperty("speed", qMax(1, pixelsPerTick)); }
void UTMarqueeLabel::paintEvent(QPaintEvent *event) { if (text().isEmpty() || !isActive()) { QLabel::paintEvent(event); return; } QPainter p(this); p.setPen(palette().windowText().color()); const int textWidth = fontMetrics().horizontalAdvance(text()); const int offset = textWidth > width() ? property("offset").toInt() % (textWidth + width()) : 0; p.drawText(-offset, 0, textWidth, height(), alignment(), text()); p.drawText(-offset + textWidth + width(), 0, textWidth, height(), alignment(), text()); }
UTTabButton::UTTabButton(QWidget *parent) : QPushButton(parent) {}
UTSliderValue::UTSliderValue(Qt::Orientation orientation, QWidget *parent) : QSlider(orientation, parent) {}
} // namespace UIToolkits
