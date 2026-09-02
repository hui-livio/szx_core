#pragma once
#include <QLabel>
#include "../UIToolkitsExport.h"
namespace UIToolkits
{
class UITOOLKITS_API UTBadge : public QLabel
{
Q_OBJECT public : enum class Tone
    {
        Neutral,
        Success,
        Warning,
        Error,
        Info
    };
    Q_ENUM(Tone) explicit UTBadge(QWidget *parent = nullptr);
    void setTone(Tone tone);
    Tone tone() const;
};
} // namespace UIToolkits
