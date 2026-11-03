#include "ProcessMonitor.h"

#include <algorithm>

namespace colibry::tm {

namespace {

/// current - previous, or 0 when a counter went backwards (reset, wrap, or a replaced process).
quint64 delta(quint64 current, quint64 previous)
{
    return current >= previous ? current - previous : 0;
}

} // namespace

void ProcessMonitor::update(const RawSnapshot &snapshot)
{
    const RawSystem &raw = snapshot.system;

    const quint64 totalDelta = m_havePrevious ? delta(raw.totalJiffies, m_prevTotal) : 0;
    const quint64 idleDelta = m_havePrevious ? delta(raw.idleJiffies, m_prevIdle) : 0;

    SystemInfo sys;
    sys.cpuCount = qMax(1, raw.cpuCount);
    if (totalDelta > 0) {
        const quint64 busy = totalDelta > idleDelta ? totalDelta - idleDelta : 0;
        sys.cpuPercent = qBound(0.0, 100.0 * double(busy) / double(totalDelta), 100.0);
    }
    sys.memTotal = raw.memTotal;
    sys.memUsed = qMax<qint64>(0, raw.memTotal - raw.memAvailable);
    sys.swapTotal = raw.swapTotal;
    sys.swapUsed = qMax<qint64>(0, raw.swapTotal - raw.swapFree);
    sys.uptimeSeconds = raw.uptimeSeconds;
    sys.load1 = raw.load1;
    sys.load5 = raw.load5;
    sys.load15 = raw.load15;

    QVector<ProcessInfo> processes;
    processes.reserve(snapshot.processes.size());
    QHash<qint64, Previous> current;
    current.reserve(snapshot.processes.size());

    int threads = 0;
    for (const RawProcess &r : snapshot.processes) {
        ProcessInfo info;
        info.pid = r.pid;
        info.ppid = r.ppid;
        info.name = r.name;
        info.user = r.user;
        info.uid = r.uid;
        info.state = r.state;
        info.memory = r.rssBytes;
        info.threads = r.threads;
        info.command = r.command;

        // Percent of one core: the process share of all CPU time, times the number of cores.
        const auto before = m_previous.constFind(r.pid);
        if (totalDelta > 0 && before != m_previous.constEnd() && before->startTime == r.startTime) {
            const quint64 used = delta(r.cpuJiffies, before->jiffies);
            info.cpu = qBound(0.0, 100.0 * double(used) / double(totalDelta) * sys.cpuCount,
                              100.0 * sys.cpuCount);
        }

        current.insert(r.pid, {r.startTime, r.cpuJiffies});
        threads += r.threads;
        processes.append(info);
    }

    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo &a, const ProcessInfo &b) { return a.pid < b.pid; });

    sys.processCount = static_cast<int>(processes.size());
    sys.threadCount = threads;

    m_processes = std::move(processes);
    m_system = sys;
    m_previous = std::move(current);
    m_prevTotal = raw.totalJiffies;
    m_prevIdle = raw.idleJiffies;
    m_havePrevious = true;
}

} // namespace colibry::tm
