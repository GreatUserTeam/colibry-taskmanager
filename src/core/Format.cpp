#include "Format.h"

#include <QCoreApplication>
#include <QLocale>

#include <cmath>

namespace colibry::tm {

QString formatUptime(double seconds)
{
    if (!(seconds > 0))
        seconds = 0;
    const qint64 total = static_cast<qint64>(seconds);
    const qint64 days = total / 86400;
    const int hours = static_cast<int>((total % 86400) / 3600);
    const int minutes = static_cast<int>((total % 3600) / 60);
    const int secs = static_cast<int>(total % 60);

    const QString clock = QStringLiteral("%1:%2:%3")
                              .arg(hours, 2, 10, QLatin1Char('0'))
                              .arg(minutes, 2, 10, QLatin1Char('0'))
                              .arg(secs, 2, 10, QLatin1Char('0'));
    return days > 0 ? QStringLiteral("%1 d %2").arg(days).arg(clock) : clock;
}

QString stateText(QChar state)
{
    switch (state.toLatin1()) {
    case 'R':
        return QCoreApplication::translate("colibry::tm", "Running");
    case 'S':
        return QCoreApplication::translate("colibry::tm", "Sleeping");
    case 'D':
        return QCoreApplication::translate("colibry::tm", "Waiting for disk");
    case 'Z':
        return QCoreApplication::translate("colibry::tm", "Zombie");
    case 'T':
    case 't':
        return QCoreApplication::translate("colibry::tm", "Stopped");
    case 'I':
        return QCoreApplication::translate("colibry::tm", "Idle");
    case 'X':
    case 'x':
        return QCoreApplication::translate("colibry::tm", "Dead");
    default:
        return QString(state);
    }
}

QString formatBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeIecFormat);
}

QString formatPercent(double percent)
{
    if (!(percent >= 0.05))
        return QStringLiteral("0%");
    return QStringLiteral("%1%").arg(percent, 0, 'f', 1);
}

} // namespace colibry::tm
