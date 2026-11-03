#include "ProcFsProvider.h"

#include <QDir>
#include <QFile>

#ifdef Q_OS_UNIX
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace colibry::tm {

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

/// Value of "Key:   123 kB" lines in kilobytes, converted to bytes. -1 when the key is missing.
qint64 kilobytesOf(const QList<QByteArray> &lines, const QByteArray &key)
{
    for (const QByteArray &line : lines) {
        if (!line.startsWith(key))
            continue;
        const QList<QByteArray> parts = line.mid(key.size()).simplified().split(' ');
        if (!parts.isEmpty()) {
            bool ok = false;
            const qint64 kb = parts.first().toLongLong(&ok);
            if (ok)
                return kb * 1024;
        }
    }
    return -1;
}

} // namespace

ProcFsProvider::ProcFsProvider(QString procRoot, QString passwdFile, long pageSize)
    : m_root(std::move(procRoot))
    , m_passwd(std::move(passwdFile))
    , m_pageSize(pageSize)
{
#ifdef Q_OS_UNIX
    if (m_pageSize <= 0)
        m_pageSize = ::sysconf(_SC_PAGESIZE);
#endif
    if (m_pageSize <= 0)
        m_pageSize = 4096;
}

bool ProcFsProvider::parseStat(const QByteArray &stat, RawProcess *out, long pageSize)
{
    const int open = static_cast<int>(stat.indexOf('('));
    const int close = static_cast<int>(stat.lastIndexOf(')'));
    if (open < 0 || close < open)
        return false;

    bool ok = false;
    const qint64 pid = stat.left(open).trimmed().toLongLong(&ok);
    if (!ok)
        return false;

    // Fields after the name: state ppid ... utime(11) stime(12) ... threads(17) ... start(19) ... rss(21)
    const QList<QByteArray> f = stat.mid(close + 1).simplified().split(' ');
    if (f.size() < 22 || f.at(0).isEmpty())
        return false;

    out->pid = pid;
    out->name = QString::fromUtf8(stat.mid(open + 1, close - open - 1));
    out->state = QLatin1Char(f.at(0).at(0));
    out->ppid = f.at(1).toLongLong();
    out->cpuJiffies = f.at(11).toULongLong() + f.at(12).toULongLong();
    out->threads = qMax(1, f.at(17).toInt());
    out->startTime = f.at(19).toULongLong();
    out->rssBytes = qMax<qint64>(0, f.at(21).toLongLong()) * pageSize;
    return true;
}

QString ProcFsProvider::userName(quint32 uid)
{
    if (!m_usersLoaded) {
        m_usersLoaded = true;
        const QList<QByteArray> lines = readAll(m_passwd).split('\n');
        for (const QByteArray &line : lines) {
            const QList<QByteArray> parts = line.split(':');
            if (parts.size() < 3)
                continue;
            bool ok = false;
            const quint32 id = parts.at(2).toUInt(&ok);
            if (ok && !m_users.contains(id))
                m_users.insert(id, QString::fromUtf8(parts.at(0)));
        }
    }
    const auto it = m_users.constFind(uid);
    return it != m_users.constEnd() ? it.value() : QString::number(uid);
}

bool ProcFsProvider::readProcess(qint64 pid, RawProcess *out)
{
    const QString dir = m_root + QLatin1Char('/') + QString::number(pid);

    const QByteArray stat = readAll(dir + QStringLiteral("/stat"));
    if (stat.isEmpty() || !parseStat(stat, out, m_pageSize))
        return false; // gone, or not readable

    // Owner: first number of the "Uid:" line.
    const QList<QByteArray> status = readAll(dir + QStringLiteral("/status")).split('\n');
    out->uid = 0;
    for (const QByteArray &line : status) {
        if (line.startsWith("Uid:")) {
            const QList<QByteArray> parts = line.mid(4).simplified().split(' ');
            if (!parts.isEmpty())
                out->uid = parts.first().toUInt();
            break;
        }
    }
    out->user = userName(out->uid);

    QByteArray cmd = readAll(dir + QStringLiteral("/cmdline"));
    cmd.replace('\0', ' ');
    // Arguments may contain newlines or tabs (think of `sh -c "multi-line script"`). A table row
    // must stay one line, so every run of whitespace becomes a single space.
    const QString command = QString::fromUtf8(cmd.left(4096)).simplified();
    out->command = command.isEmpty() ? QStringLiteral("[%1]").arg(out->name) : command;
    return true;
}

RawSystem ProcFsProvider::readSystem() const
{
    RawSystem sys;

    // /proc/stat: "cpu  user nice system idle iowait irq softirq steal guest guest_nice"
    const QList<QByteArray> stat = readAll(m_root + QStringLiteral("/stat")).split('\n');
    int cpus = 0;
    for (const QByteArray &line : stat) {
        if (line.startsWith("cpu ")) {
            const QList<QByteArray> p = line.simplified().split(' ');
            // p[0] is "cpu"; guest time is already inside user time, so it is left out.
            quint64 total = 0;
            for (int i = 1; i <= 8 && i < p.size(); ++i)
                total += p.at(i).toULongLong();
            sys.totalJiffies = total;
            sys.idleJiffies = (p.size() > 4 ? p.at(4).toULongLong() : 0) + (p.size() > 5 ? p.at(5).toULongLong() : 0);
        } else if (line.startsWith("cpu") && line.size() > 3 && QChar(QLatin1Char(line.at(3))).isDigit()) {
            ++cpus;
        }
    }
    sys.cpuCount = qMax(1, cpus);

    const QList<QByteArray> mem = readAll(m_root + QStringLiteral("/meminfo")).split('\n');
    sys.memTotal = qMax<qint64>(0, kilobytesOf(mem, "MemTotal:"));
    qint64 available = kilobytesOf(mem, "MemAvailable:");
    if (available < 0) // very old kernels
        available = qMax<qint64>(0, kilobytesOf(mem, "MemFree:")) + qMax<qint64>(0, kilobytesOf(mem, "Cached:"));
    sys.memAvailable = qMin(available, sys.memTotal);
    sys.swapTotal = qMax<qint64>(0, kilobytesOf(mem, "SwapTotal:"));
    sys.swapFree = qMin(qMax<qint64>(0, kilobytesOf(mem, "SwapFree:")), sys.swapTotal);

    sys.uptimeSeconds = readAll(m_root + QStringLiteral("/uptime")).simplified().split(' ').value(0).toDouble();

    const QList<QByteArray> load = readAll(m_root + QStringLiteral("/loadavg")).simplified().split(' ');
    sys.load1 = load.value(0).toDouble();
    sys.load5 = load.value(1).toDouble();
    sys.load15 = load.value(2).toDouble();
    return sys;
}

RawSnapshot ProcFsProvider::read()
{
    RawSnapshot snapshot;
    snapshot.system = readSystem();

    const QStringList entries = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    snapshot.processes.reserve(entries.size());
    for (const QString &entry : entries) {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (!ok || pid <= 0)
            continue;
        RawProcess process;
        if (readProcess(pid, &process))
            snapshot.processes.append(process);
    }
    return snapshot;
}

ProcessProvider::Result ProcFsProvider::terminate(qint64 pid, bool force)
{
#ifdef Q_OS_UNIX
    if (pid <= 0)
        return {false, QStringLiteral("Invalid process id.")};
    if (::kill(static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) == 0)
        return {true, {}};

    switch (errno) {
    case EPERM:
        return {false, QStringLiteral("Permission denied. This process belongs to another user.")};
    case ESRCH:
        return {false, QStringLiteral("The process no longer exists.")};
    default:
        return {false, QString::fromLocal8Bit(std::strerror(errno))};
    }
#else
    Q_UNUSED(pid)
    Q_UNUSED(force)
    return {false, QStringLiteral("Ending processes is not supported on this system.")};
#endif
}

} // namespace colibry::tm
