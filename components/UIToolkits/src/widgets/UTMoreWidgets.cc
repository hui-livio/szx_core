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

namespace UIToolkits
{
UTAppWindow::UTAppWindow(QWidget *parent) : QMainWindow(parent) {}
UTTitleBar::UTTitleBar(QWidget *parent) : QWidget(parent) {}
UTTitleBarButtonGroup::UTTitleBarButtonGroup(QWidget *parent) : QWidget(parent) {}
UTFootBar::UTFootBar(QWidget *parent) : QWidget(parent) {}
UTBadge::UTBadge(QWidget *parent) : QLabel(parent) {}
UTStatCard::UTStatCard(QWidget *parent) : QFrame(parent) {}
UTKeyValueGrid::UTKeyValueGrid(QWidget *parent) : QWidget(parent) {}
UTScrollView::UTScrollView(QWidget *parent) : QScrollArea(parent) {}
UTFlickable::UTFlickable(QWidget *parent) : QScrollArea(parent) {}
UTImageAvatar::UTImageAvatar(QWidget *parent) : QLabel(parent) {}
UTImageViewer::UTImageViewer(QWidget *parent) : QScrollArea(parent) {}
UTLoading::UTLoading(QWidget *parent) : QProgressBar(parent) { setRange(0, 0); }
UTToast::UTToast(QWidget *parent) : QLabel(parent) {}
UTTooltip::UTTooltip(QWidget *parent) : QLabel(parent) {}
UTMessageDialog::UTMessageDialog(QWidget *parent) : QDialog(parent) {}
UTCalendar::UTCalendar(QWidget *parent) : QCalendarWidget(parent) {}
UTMarqueeLabel::UTMarqueeLabel(QWidget *parent) : QLabel(parent) {}
UTTabButton::UTTabButton(QWidget *parent) : QPushButton(parent) {}
UTSliderValue::UTSliderValue(Qt::Orientation orientation, QWidget *parent) : QSlider(orientation, parent) {}
} // namespace UIToolkits
