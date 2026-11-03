#pragma once

#include <QChar>
#include <QString>
#include <QMetaType>
#include <QVector>
#include <QtGlobal>

namespace colibry::tm {

/// What the operating system reports about one process, before any calculation.
struct RawProcess
{
    qint64 pid = 0;
    qint64 ppid = 0;
    QString name;
    QString user;
    quint32 uid = 0;
    QChar state = QLatin1Char('?'); ///< R, S, D, Z, T, I ... as in /proc
    quint64 cpuJiffies = 0;         ///< user + system time used so far, in clock ticks
    quint64 startTime = 0;          ///< when it started; tells a reused pid from the old process
    qint64 rssBytes = 0;
    int threads = 1;
    QString command; ///< full command line, or "[name]" for kernel threads
};

/// Machine-wide numbers from the operating system.
struct RawSystem
{
    quint64 totalJiffies = 0; ///< all CPUs, all states
    quint64 idleJiffies = 0;  ///< idle + waiting for I/O
    int cpuCount = 1;
    qint64 memTotal = 0; ///< bytes
    qint64 memAvailable = 0;
    qint64 swapTotal = 0;
    qint64 swapFree = 0;
    double uptimeSeconds = 0;
    double load1 = 0;
    double load5 = 0;
    double load15 = 0;
};

struct RawSnapshot
{
    QVector<RawProcess> processes;
    RawSystem system;
};

/// One row of the process list, with CPU usage worked out.
struct ProcessInfo
{
    qint64 pid = 0;
    qint64 ppid = 0;
    QString name;
    QString user;
    quint32 uid = 0;
    QChar state = QLatin1Char('?');
    double cpu = 0; ///< percent of ONE core: 100 means one core fully used, 400 means four
    qint64 memory = 0;
    int threads = 1;
    QString command;

    bool operator==(const ProcessInfo &o) const
    {
        return pid == o.pid && ppid == o.ppid && name == o.name && user == o.user && uid == o.uid
               && state == o.state && cpu == o.cpu && memory == o.memory && threads == o.threads
               && command == o.command;
    }
    bool operator!=(const ProcessInfo &o) const { return !(*this == o); }
};

struct SystemInfo
{
    double cpuPercent = 0; ///< whole machine, 0..100
    int cpuCount = 1;
    qint64 memTotal = 0;
    qint64 memUsed = 0; ///< total minus available
    qint64 swapTotal = 0;
    qint64 swapUsed = 0;
    double uptimeSeconds = 0;
    double load1 = 0;
    double load5 = 0;
    double load15 = 0;
    int processCount = 0;
    int threadCount = 0;
};

} // namespace colibry::tm

Q_DECLARE_METATYPE(colibry::tm::ProcessInfo)
