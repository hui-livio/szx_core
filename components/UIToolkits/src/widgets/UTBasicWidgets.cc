#include "UIToolkits/UTCard.h"
#include "UIToolkits/UTCheckBox.h"
#include "UIToolkits/UTComboBox.h"
#include "UIToolkits/UTDialog.h"
#include "UIToolkits/UTLabel.h"
#include "UIToolkits/UTProgressBar.h"
#include "UIToolkits/UTSlider.h"
#include "UIToolkits/UTSpinBox.h"
#include "UIToolkits/UTSwitch.h"

namespace UIToolkits
{
UTLabel::UTLabel(QWidget *parent) : QLabel(parent)
{
    setObjectName(QStringLiteral("UTLabel"));
}

UTCheckBox::UTCheckBox(QWidget *parent) : QCheckBox(parent)
{
    setObjectName(QStringLiteral("UTCheckBox"));
}

UTSwitch::UTSwitch(QWidget *parent) : QCheckBox(parent)
{
    setObjectName(QStringLiteral("UTSwitch"));
    setCursor(Qt::PointingHandCursor);
}

UTComboBox::UTComboBox(QWidget *parent) : QComboBox(parent)
{
    setObjectName(QStringLiteral("UTComboBox"));
    setMinimumHeight(36);
}

UTSpinBox::UTSpinBox(QWidget *parent) : QSpinBox(parent)
{
    setObjectName(QStringLiteral("UTSpinBox"));
    setMinimumHeight(36);
}

UTSlider::UTSlider(Qt::Orientation orientation, QWidget *parent) : QSlider(orientation, parent)
{
    setObjectName(QStringLiteral("UTSlider"));
}

UTProgressBar::UTProgressBar(QWidget *parent) : QProgressBar(parent)
{
    setObjectName(QStringLiteral("UTProgressBar"));
    setTextVisible(false);
    setMinimumHeight(8);
}

UTCard::UTCard(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("UTCard"));
    setFrameShape(QFrame::StyledPanel);
}

UTDialog::UTDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("UTDialog"));
    setModal(true);
}
} // namespace UIToolkits
