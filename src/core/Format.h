#pragma once

#include <QChar>
#include <QString>

namespace colibry::tm {

/// "2 d 03:04:05", "03:04:05", "00:00:09"
QString formatUptime(double seconds);

/// "Running", "Sleeping", ... for the state letters of /proc.
QString stateText(QChar state);

/// "1.5 GiB" style size.
QString formatBytes(qint64 bytes);

/// "12.3%" with one decimal; values below 0.05 show as "0%".
QString formatPercent(double percent);

} // namespace colibry::tm
