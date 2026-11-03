#pragma once

#include "ProcessData.h"

#include <QHash>

namespace colibry::tm {

/// Turns raw snapshots into usage figures. CPU usage is a difference between two snapshots,
/// so the very first update reports 0 % for everything.
class ProcessMonitor
{
public:
    void update(const RawSnapshot &snapshot);

    /// Sorted by pid.
    const QVector<ProcessInfo> &processes() const { return m_processes; }
    const SystemInfo &system() const { return m_system; }

private:
    struct Previous
    {
        quint64 startTime = 0;
        quint64 jiffies = 0;
    };

    QVector<ProcessInfo> m_processes;
    SystemInfo m_system;
    QHash<qint64, Previous> m_previous;
    quint64 m_prevTotal = 0;
    quint64 m_prevIdle = 0;
    bool m_havePrevious = false;
};

} // namespace colibry::tm
