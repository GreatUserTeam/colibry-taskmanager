#include "core/Format.h"
#include "core/ProcFsProvider.h"
#include "core/ProcessModel.h"
#include "core/ProcessMonitor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace colibry::tm;

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qPrintable(path));
    f.write(data);
}

/// A /proc/<pid>/stat line with the interesting fields filled in.
QByteArray statLine(qint64 pid, const QByteArray &comm, char state, qint64 ppid, quint64 utime, quint64 stime,
                    int threads, quint64 start, qint64 rssPages)
{
    // pid (comm) state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime
    // cutime cstime priority nice threads itrealvalue starttime vsize rss ...
    return QByteArray::number(pid) + " (" + comm + ") " + state + ' ' + QByteArray::number(ppid)
           + " 0 0 0 0 0 0 0 0 0 " + QByteArray::number(utime) + ' ' + QByteArray::number(stime)
           + " 0 0 20 0 " + QByteArray::number(threads) + " 0 " + QByteArray::number(start)
           + " 123456 " + QByteArray::number(rssPages) + " 0 0\n";
}

ProcessInfo info(qint64 pid, const QString &name, double cpu = 0, qint64 mem = 0)
{
    ProcessInfo p;
    p.pid = pid;
    p.name = name;
    p.user = QStringLiteral("alice");
    p.cpu = cpu;
    p.memory = mem;
    p.command = name;
    return p;
}

RawProcess raw(qint64 pid, quint64 jiffies, quint64 start = 1)
{
    RawProcess r;
    r.pid = pid;
    r.name = QStringLiteral("p%1").arg(pid);
    r.cpuJiffies = jiffies;
    r.startTime = start;
    return r;
}

RawSnapshot snapshot(quint64 total, quint64 idle, int cpus, const QVector<RawProcess> &procs)
{
    RawSnapshot s;
    s.system.totalJiffies = total;
    s.system.idleJiffies = idle;
    s.system.cpuCount = cpus;
    s.processes = procs;
    return s;
}

} // namespace

class TaskCoreTest : public QObject
{
    Q_OBJECT

private slots:
    // ---- formatting ---------------------------------------------------
    void formatsUptime()
    {
        QCOMPARE(formatUptime(0), QStringLiteral("00:00:00"));
        QCOMPARE(formatUptime(9.9), QStringLiteral("00:00:09"));
        QCOMPARE(formatUptime(3725), QStringLiteral("01:02:05"));
        QCOMPARE(formatUptime(86400 + 3661), QStringLiteral("1 d 01:01:01"));
        QCOMPARE(formatUptime(-5), QStringLiteral("00:00:00"));
        QCOMPARE(formatUptime(std::nan("")), QStringLiteral("00:00:00"));
    }

    void formatsPercentAndStates()
    {
        QCOMPARE(formatPercent(0), QStringLiteral("0%"));
        QCOMPARE(formatPercent(0.04), QStringLiteral("0%"));
        QCOMPARE(formatPercent(12.34), QStringLiteral("12.3%"));
        QCOMPARE(formatPercent(250), QStringLiteral("250.0%"));
        QCOMPARE(stateText(QLatin1Char('R')), QStringLiteral("Running"));
        QCOMPARE(stateText(QLatin1Char('Z')), QStringLiteral("Zombie"));
        QCOMPARE(stateText(QLatin1Char('?')), QStringLiteral("?"));
    }

    // ---- /proc/<pid>/stat parsing ---------------------------------------
    void parsesPlainStatLine()
    {
        RawProcess p;
        QVERIFY(ProcFsProvider::parseStat(statLine(42, "bash", 'S', 1, 100, 50, 3, 777, 250), &p, 4096));
        QCOMPARE(p.pid, 42);
        QCOMPARE(p.name, QStringLiteral("bash"));
        QCOMPARE(p.state, QLatin1Char('S'));
        QCOMPARE(p.ppid, 1);
        QCOMPARE(p.cpuJiffies, quint64(150));
        QCOMPARE(p.threads, 3);
        QCOMPARE(p.startTime, quint64(777));
        QCOMPARE(p.rssBytes, qint64(250) * 4096);
    }

    void parsesNamesWithSpacesAndParentheses()
    {
        RawProcess p;
        QVERIFY(ProcFsProvider::parseStat(statLine(7, "Web Content (x) y)", 'R', 3, 10, 5, 1, 9, 1), &p, 4096));
        QCOMPARE(p.name, QStringLiteral("Web Content (x) y)"));
        QCOMPARE(p.state, QLatin1Char('R'));
        QCOMPARE(p.ppid, 3);
        QCOMPARE(p.cpuJiffies, quint64(15));
    }

    void parsesNonAsciiNames()
    {
        RawProcess p;
        QVERIFY(ProcFsProvider::parseStat(statLine(8, QStringLiteral("програма").toUtf8(), 'S', 1, 0, 0, 1, 1, 0), &p, 4096));
        QCOMPARE(p.name, QStringLiteral("програма"));
    }

    void rejectsGarbage()
    {
        RawProcess p;
        QVERIFY(!ProcFsProvider::parseStat(QByteArray(), &p, 4096));
        QVERIFY(!ProcFsProvider::parseStat("hello", &p, 4096));
        QVERIFY(!ProcFsProvider::parseStat("12 (x", &p, 4096));
        QVERIFY(!ProcFsProvider::parseStat("abc (x) S 1", &p, 4096));
        QVERIFY(!ProcFsProvider::parseStat("12 (x) S 1 2 3", &p, 4096)); // too few fields
    }

    // ---- provider on a fake /proc ---------------------------------------
    void readsAFakeProcTree()
    {
        QTemporaryDir dir;
        const QString root = dir.path() + QStringLiteral("/proc");
        writeFile(dir.path() + QStringLiteral("/passwd"), "root:x:0:0:root:/root:/bin/sh\nalice:x:1000:1000::/home/alice:/bin/sh\n");

        writeFile(root + QStringLiteral("/stat"),
                  "cpu  100 20 30 400 50 6 7 8 9 10\ncpu0 50 10 15 200 25 3 3 4 0 0\ncpu1 50 10 15 200 25 3 4 4 0 0\nintr 1\n");
        writeFile(root + QStringLiteral("/meminfo"),
                  "MemTotal:       8000000 kB\nMemFree:         100000 kB\nMemAvailable:   3000000 kB\nSwapTotal:      2000000 kB\nSwapFree:       1500000 kB\n");
        writeFile(root + QStringLiteral("/uptime"), "12345.67 99999.00\n");
        writeFile(root + QStringLiteral("/loadavg"), "0.50 1.25 2.00 1/200 4242\n");

        writeFile(root + QStringLiteral("/100/stat"), statLine(100, "firefox", 'S', 1, 200, 100, 12, 5000, 25600));
        writeFile(root + QStringLiteral("/100/status"), "Name:\tfirefox\nUid:\t1000\t1000\t1000\t1000\n");
        writeFile(root + QStringLiteral("/100/cmdline"), QByteArray("/usr/bin/firefox\0--new-window\0", 30));

        writeFile(root + QStringLiteral("/2/stat"), statLine(2, "kthreadd", 'S', 0, 0, 0, 1, 1, 0));
        writeFile(root + QStringLiteral("/2/status"), "Name:\tkthreadd\nUid:\t0\t0\t0\t0\n");
        writeFile(root + QStringLiteral("/2/cmdline"), QByteArray());

        // Not processes: must be ignored
        writeFile(root + QStringLiteral("/self/stat"), "x");
        writeFile(root + QStringLiteral("/sys/stat"), "x");
        // A process that vanished halfway: directory without a stat file
        QDir().mkpath(root + QStringLiteral("/555"));

        ProcFsProvider provider(root, dir.path() + QStringLiteral("/passwd"), 4096);
        const RawSnapshot snap = provider.read();

        QCOMPARE(snap.processes.size(), 2);
        RawProcess ff;
        RawProcess kt;
        for (const RawProcess &p : snap.processes)
            (p.pid == 100 ? ff : kt) = p;

        QCOMPARE(ff.name, QStringLiteral("firefox"));
        QCOMPARE(ff.user, QStringLiteral("alice"));
        QCOMPARE(ff.uid, 1000u);
        QCOMPARE(ff.command, QStringLiteral("/usr/bin/firefox --new-window"));
        QCOMPARE(ff.rssBytes, qint64(25600) * 4096);
        QCOMPARE(ff.threads, 12);

        QCOMPARE(kt.user, QStringLiteral("root"));
        QCOMPARE(kt.command, QStringLiteral("[kthreadd]")); // kernel threads have no command line

        const RawSystem &s = snap.system;
        QCOMPARE(s.cpuCount, 2);
        QCOMPARE(s.totalJiffies, quint64(100 + 20 + 30 + 400 + 50 + 6 + 7 + 8)); // guest columns excluded
        QCOMPARE(s.idleJiffies, quint64(450));                                    // idle + iowait
        QCOMPARE(s.memTotal, qint64(8000000) * 1024);
        QCOMPARE(s.memAvailable, qint64(3000000) * 1024);
        QCOMPARE(s.swapTotal, qint64(2000000) * 1024);
        QCOMPARE(s.swapFree, qint64(1500000) * 1024);
        QCOMPARE(s.uptimeSeconds, 12345.67);
        QCOMPARE(s.load1, 0.5);
        QCOMPARE(s.load5, 1.25);
        QCOMPARE(s.load15, 2.0);
    }

    void commandLinesWithNewlinesStayOnOneLine()
    {
        QTemporaryDir dir;
        const QString root = dir.path() + QStringLiteral("/proc");
        writeFile(root + QStringLiteral("/9/stat"), statLine(9, "bash", 'S', 1, 0, 0, 1, 1, 0));
        writeFile(root + QStringLiteral("/9/status"), "Uid:\t0\t0\t0\t0\n");
        writeFile(root + QStringLiteral("/9/cmdline"), QByteArray("/bin/bash\0-c\0echo one\n\techo   two\r\n\0", 36));
        ProcFsProvider provider(root, QString(), 4096);
        const RawSnapshot snap = provider.read();
        QCOMPARE(snap.processes.size(), 1);
        QCOMPARE(snap.processes.first().command, QStringLiteral("/bin/bash -c echo one echo two"));
        QVERIFY(!snap.processes.first().command.contains(QLatin1Char('\n')));
    }

    void unknownUidFallsBackToNumber()
    {
        QTemporaryDir dir;
        const QString root = dir.path() + QStringLiteral("/proc");
        writeFile(root + QStringLiteral("/9/stat"), statLine(9, "x", 'S', 1, 0, 0, 1, 1, 0));
        writeFile(root + QStringLiteral("/9/status"), "Uid:\t4321\t4321\t4321\t4321\n");
        ProcFsProvider provider(root, dir.path() + QStringLiteral("/no-passwd"), 4096);
        const RawSnapshot snap = provider.read();
        QCOMPARE(snap.processes.size(), 1);
        QCOMPARE(snap.processes.first().user, QStringLiteral("4321"));
    }

    void missingProcTreeGivesEmptySnapshotNotCrash()
    {
        ProcFsProvider provider(QStringLiteral("/nonexistent-proc"), QStringLiteral("/nonexistent-passwd"), 4096);
        const RawSnapshot snap = provider.read();
        QVERIFY(snap.processes.isEmpty());
        QCOMPARE(snap.system.cpuCount, 1);
        QCOMPARE(snap.system.memTotal, qint64(0));
    }

    void oldKernelWithoutMemAvailableStillWorks()
    {
        QTemporaryDir dir;
        const QString root = dir.path() + QStringLiteral("/proc");
        writeFile(root + QStringLiteral("/meminfo"), "MemTotal: 1000 kB\nMemFree: 200 kB\nCached: 300 kB\n");
        ProcFsProvider provider(root, QString(), 4096);
        QCOMPARE(provider.readSystem().memAvailable, qint64(500) * 1024);
    }

    // ---- the real /proc -------------------------------------------------
    void readsTheRealProcFs()
    {
        if (!QFileInfo(QStringLiteral("/proc/self/stat")).exists())
            QSKIP("no /proc on this system", SkipSingle);

        ProcFsProvider provider;
        const RawSnapshot snap = provider.read();
        QVERIFY(snap.processes.size() > 1);

        const qint64 me = QCoreApplication::applicationPid();
        const RawProcess *self = nullptr;
        for (const RawProcess &p : snap.processes) {
            if (p.pid == me)
                self = &p;
        }
        QVERIFY2(self, "own process not found");
        QVERIFY(self->rssBytes > 0);
        QVERIFY(self->threads >= 1);
        QVERIFY(!self->command.isEmpty());
        QVERIFY(!self->user.isEmpty());
        QVERIFY(self->name.contains(QStringLiteral("task"), Qt::CaseInsensitive)
                || self->name.contains(QStringLiteral("tm_"), Qt::CaseInsensitive));
        QCOMPARE(self->ppid, qint64(::getppid()));

        QVERIFY(snap.system.memTotal > 0);
        QVERIFY(snap.system.cpuCount >= 1);
        QVERIFY(snap.system.uptimeSeconds > 0);
        QVERIFY(snap.system.totalJiffies > 0);
    }

    // ---- CPU maths ------------------------------------------------------
    void firstUpdateReportsZeroCpu()
    {
        ProcessMonitor m;
        m.update(snapshot(1000, 500, 4, {raw(1, 100)}));
        QCOMPARE(m.processes().size(), 1);
        QCOMPARE(m.processes().first().cpu, 0.0);
        QCOMPARE(m.system().cpuPercent, 0.0);
    }

    void cpuPercentIsShareOfOneCore()
    {
        ProcessMonitor m;
        m.update(snapshot(1000, 500, 4, {raw(1, 100), raw(2, 0)}));
        // 400 ticks passed in total on 4 cores (= 100 ticks per core). Process 1 used 100 of them:
        // that is exactly one full core = 100 %.
        m.update(snapshot(1400, 800, 4, {raw(1, 200), raw(2, 50)}));
        QCOMPARE(m.processes().at(0).pid, 1);
        QVERIFY(qFuzzyCompare(m.processes().at(0).cpu, 100.0));
        QVERIFY(qFuzzyCompare(m.processes().at(1).cpu, 50.0)); // half a core
        // machine busy: (400 - 300 idle) / 400 = 25 %
        QVERIFY(qFuzzyCompare(m.system().cpuPercent, 25.0));
    }

    void multiThreadedProcessCanExceedOneHundred()
    {
        ProcessMonitor m;
        m.update(snapshot(0, 0, 4, {raw(1, 0)}));
        m.update(snapshot(400, 0, 4, {raw(1, 300)})); // 3 of 4 cores busy with this process
        QVERIFY(qFuzzyCompare(m.processes().first().cpu, 300.0));
        QVERIFY(qFuzzyCompare(m.system().cpuPercent, 100.0));
    }

    void cpuIsClampedWhenCountersMisbehave()
    {
        ProcessMonitor m;
        m.update(snapshot(100, 0, 2, {raw(1, 0)}));
        m.update(snapshot(110, 0, 2, {raw(1, 99999)})); // more CPU time than possible
        QVERIFY(m.processes().first().cpu <= 200.0);
        QVERIFY(m.system().cpuPercent <= 100.0);
    }

    void counterGoingBackwardsGivesZeroNotHugeNumbers()
    {
        ProcessMonitor m;
        m.update(snapshot(1000, 500, 1, {raw(1, 900)}));
        m.update(snapshot(2000, 600, 1, {raw(1, 10)})); // process counter reset
        QCOMPARE(m.processes().first().cpu, 0.0);
        m.update(snapshot(1500, 700, 1, {raw(1, 20)})); // system counter went back
        QCOMPARE(m.processes().first().cpu, 0.0);
        QCOMPARE(m.system().cpuPercent, 0.0);
    }

    void noElapsedTimeDoesNotDivideByZero()
    {
        ProcessMonitor m;
        m.update(snapshot(1000, 500, 2, {raw(1, 10)}));
        m.update(snapshot(1000, 500, 2, {raw(1, 50)}));
        QCOMPARE(m.processes().first().cpu, 0.0);
        QCOMPARE(m.system().cpuPercent, 0.0);
    }

    void reusedPidDoesNotInheritOldCpuTime()
    {
        ProcessMonitor m;
        m.update(snapshot(1000, 0, 1, {raw(5, 900, /*start*/ 10)}));
        // pid 5 is now a different process (other start time) that has used 20 ticks
        m.update(snapshot(1100, 0, 1, {raw(5, 20, /*start*/ 99)}));
        QCOMPARE(m.processes().first().cpu, 0.0); // no valid earlier sample for it
        m.update(snapshot(1200, 0, 1, {raw(5, 70, 99)}));
        QVERIFY(qFuzzyCompare(m.processes().first().cpu, 50.0));
    }

    void newProcessStartsAtZeroAndGoneProcessIsForgotten()
    {
        ProcessMonitor m;
        m.update(snapshot(100, 0, 1, {raw(1, 0)}));
        m.update(snapshot(200, 0, 1, {raw(1, 10), raw(2, 90)}));
        QCOMPARE(m.processes().size(), 2);
        QCOMPARE(m.processes().at(1).cpu, 0.0); // pid 2 has no earlier sample
        m.update(snapshot(300, 0, 1, {raw(2, 100)}));
        QCOMPARE(m.processes().size(), 1);
        QVERIFY(qFuzzyCompare(m.processes().first().cpu, 10.0));
    }

    void summarisesMemoryAndCounts()
    {
        RawSnapshot s = snapshot(1, 0, 1, {raw(3, 0), raw(1, 0)});
        s.processes[0].threads = 4;
        s.processes[1].threads = 6;
        s.system.memTotal = 1000;
        s.system.memAvailable = 250;
        s.system.swapTotal = 400;
        s.system.swapFree = 100;
        ProcessMonitor m;
        m.update(s);
        QCOMPARE(m.system().memUsed, qint64(750));
        QCOMPARE(m.system().swapUsed, qint64(300));
        QCOMPARE(m.system().processCount, 2);
        QCOMPARE(m.system().threadCount, 10);
        QCOMPARE(m.processes().first().pid, 1); // sorted by pid
    }

    // ---- the table model ------------------------------------------------
    void modelInsertsRemovesAndUpdatesInPlace()
    {
        ProcessModel model;
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

        model.setProcesses({info(1, QStringLiteral("a")), info(3, QStringLiteral("c")), info(5, QStringLiteral("e"))});
        QCOMPARE(model.rowCount(), 3);

        // 3 vanishes, 4 appears, 5 changes
        inserted.clear();
        removed.clear();
        changed.clear();
        model.setProcesses({info(1, QStringLiteral("a")), info(4, QStringLiteral("d")), info(5, QStringLiteral("e"), 7.5)});
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.processAt(0)->pid, 1);
        QCOMPARE(model.processAt(1)->pid, 4);
        QCOMPARE(model.processAt(2)->pid, 5);
        QCOMPARE(model.processAt(2)->cpu, 7.5);
        QVERIFY(removed.count() >= 1);
        QVERIFY(inserted.count() >= 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(reset.count(), 0); // never a full reset: the selection must survive
    }

    void modelDoesNotSignalWhenNothingChanged()
    {
        ProcessModel model;
        const QVector<ProcessInfo> list = {info(1, QStringLiteral("a")), info(2, QStringLiteral("b"))};
        model.setProcesses(list);
        QSignalSpy any(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy rows(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy gone(&model, &QAbstractItemModel::rowsRemoved);
        model.setProcesses(list);
        QCOMPARE(any.count() + rows.count() + gone.count(), 0);
    }

    void modelHandlesBulkChangesCorrectly()
    {
        ProcessModel model;
        QVector<ProcessInfo> first;
        for (int i = 1; i <= 50; ++i)
            first << info(i, QStringLiteral("p%1").arg(i));
        model.setProcesses(first);

        // remove 10..20, add 100..110, keep the rest
        QVector<ProcessInfo> second;
        for (int i = 1; i <= 50; ++i) {
            if (i < 10 || i > 20)
                second << info(i, QStringLiteral("p%1").arg(i));
        }
        for (int i = 100; i <= 110; ++i)
            second << info(i, QStringLiteral("p%1").arg(i));
        model.setProcesses(second);

        QCOMPARE(model.rowCount(), second.size());
        for (int row = 0; row < model.rowCount(); ++row)
            QCOMPARE(model.processAt(row)->pid, second.at(row).pid);

        model.setProcesses({});
        QCOMPARE(model.rowCount(), 0);
        model.setProcesses(first);
        QCOMPARE(model.rowCount(), 50);
    }

    void modelRandomisedAgainstReference()
    {
        ProcessModel model;
        QRandomGenerator rng(12345);
        for (int round = 0; round < 200; ++round) {
            QVector<ProcessInfo> list;
            for (int pid = 1; pid <= 60; ++pid) {
                if (rng.bounded(100) < 50)
                    list << info(pid, QStringLiteral("p%1").arg(pid), rng.bounded(3) * 1.0);
            }
            model.setProcesses(list);
            QCOMPARE(model.rowCount(), static_cast<int>(list.size()));
            for (int row = 0; row < list.size(); ++row) {
                QCOMPARE(model.processAt(row)->pid, list.at(row).pid);
                QCOMPARE(model.processAt(row)->cpu, list.at(row).cpu);
            }
        }
    }

    void rowOfPidFindsRows()
    {
        ProcessModel model;
        model.setProcesses({info(2, QStringLiteral("b")), info(8, QStringLiteral("h")), info(20, QStringLiteral("t"))});
        QCOMPARE(model.rowOfPid(2), 0);
        QCOMPARE(model.rowOfPid(8), 1);
        QCOMPARE(model.rowOfPid(20), 2);
        QCOMPARE(model.rowOfPid(3), -1);
        QCOMPARE(model.rowOfPid(0), -1);
    }

    void modelShowsReadableValues()
    {
        ProcessModel model;
        ProcessInfo p = info(77, QStringLiteral("Firefox"), 12.34, 1536LL * 1024 * 1024);
        p.state = QLatin1Char('S');
        model.setProcesses({p});
        QCOMPARE(model.index(0, ProcessModel::Name).data().toString(), QStringLiteral("Firefox"));
        QCOMPARE(model.index(0, ProcessModel::Pid).data().toLongLong(), 77);
        QCOMPARE(model.index(0, ProcessModel::Cpu).data().toString(), QStringLiteral("12.3%"));
        QVERIFY(model.index(0, ProcessModel::Memory).data().toString().contains(QStringLiteral("GiB")));
        QCOMPARE(model.index(0, ProcessModel::Status).data().toString(), QStringLiteral("Sleeping"));
        QCOMPARE(model.index(0, ProcessModel::Pid).data(ProcessModel::PidRole).toLongLong(), 77);
        QVERIFY(!model.index(0, 0).data(Qt::ToolTipRole).toString().isEmpty());
        QVERIFY(!model.headerData(ProcessModel::Cpu, Qt::Horizontal).toString().isEmpty());
        QVERIFY(!model.index(9, 0).data().isValid()); // out of range
    }

    void proxySortsNumbersAsNumbers()
    {
        ProcessModel model;
        ProcessFilterProxy proxy;
        proxy.setSourceModel(&model);
        model.setProcesses({info(9, QStringLiteral("b")), info(10, QStringLiteral("A")), info(100, QStringLiteral("c"))});

        proxy.sort(ProcessModel::Pid, Qt::AscendingOrder);
        QCOMPARE(proxy.index(0, ProcessModel::Pid).data().toLongLong(), 9); // 9 < 10 < 100, not "100" < "9"
        QCOMPARE(proxy.index(2, ProcessModel::Pid).data().toLongLong(), 100);

        proxy.sort(ProcessModel::Name, Qt::AscendingOrder);
        QCOMPARE(proxy.index(0, ProcessModel::Name).data().toString(), QStringLiteral("A")); // case-insensitive
    }

    void proxySortsByMemoryNotByText()
    {
        ProcessModel model;
        ProcessFilterProxy proxy;
        proxy.setSourceModel(&model);
        model.setProcesses({info(1, QStringLiteral("a"), 0, 900), info(2, QStringLiteral("b"), 0, 1024 * 1024 * 2),
                            info(3, QStringLiteral("c"), 0, 20000)});
        proxy.sort(ProcessModel::Memory, Qt::DescendingOrder);
        QCOMPARE(proxy.index(0, ProcessModel::Pid).data().toLongLong(), 2);
        QCOMPARE(proxy.index(1, ProcessModel::Pid).data().toLongLong(), 3);
        QCOMPARE(proxy.index(2, ProcessModel::Pid).data().toLongLong(), 1);
    }

    void proxyFiltersByTextAndUser()
    {
        ProcessModel model;
        ProcessFilterProxy proxy;
        proxy.setSourceModel(&model);
        ProcessInfo a = info(11, QStringLiteral("firefox"));
        a.command = QStringLiteral("/usr/bin/firefox --private");
        ProcessInfo b = info(22, QStringLiteral("bash"));
        b.user = QStringLiteral("root");
        ProcessInfo c = info(333, QStringLiteral("sshd"));
        model.setProcesses({a, b, c});

        QCOMPARE(proxy.rowCount(), 3);
        proxy.setFilterText(QStringLiteral("FIRE"));
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText(QStringLiteral("--private")); // matches the command line
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText(QStringLiteral("33")); // matches a pid
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText(QStringLiteral("root")); // matches a user
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setFilterText(QStringLiteral("  "));
        QCOMPARE(proxy.rowCount(), 3);
        proxy.setFilterText(QStringLiteral("nothing-like-this"));
        QCOMPARE(proxy.rowCount(), 0);

        proxy.setFilterText(QString());
        proxy.setOnlyUser(QStringLiteral("alice"));
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setOnlyUser(QString());
        QCOMPARE(proxy.rowCount(), 3);
    }

    // ---- ending a real process -------------------------------------------
    void terminatesARealChildProcess()
    {
        QProcess child;
        child.start(QStringLiteral("sleep"), {QStringLiteral("60")});
        QVERIFY(child.waitForStarted(5000));

        ProcFsProvider provider;
        const auto result = provider.terminate(child.processId(), false);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(child.waitForFinished(5000));
        QCOMPARE(child.exitStatus(), QProcess::CrashExit); // died from a signal, as expected
    }

    void forceKillsAProcessThatIgnoresTerm()
    {
        QProcess child;
        // The shell ignores SIGTERM, so only SIGKILL can end it.
        child.start(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("trap '' TERM; while true; do sleep 1; done")});
        QVERIFY(child.waitForStarted(5000));
        QTest::qWait(300);

        ProcFsProvider provider;
        QVERIFY(provider.terminate(child.processId(), false).ok);
        QVERIFY(!child.waitForFinished(700)); // still alive after SIGTERM
        QVERIFY(provider.terminate(child.processId(), true).ok);
        QVERIFY(child.waitForFinished(5000));
    }

    void terminateReportsMissingProcessAndBadPid()
    {
        ProcFsProvider provider;
        const auto gone = provider.terminate(2000000000, false);
        QVERIFY(!gone.ok);
        QVERIFY(!gone.error.isEmpty());
        QVERIFY(!provider.terminate(0, false).ok);
        QVERIFY(!provider.terminate(-5, true).ok);
    }

    void terminateReportsPermissionDenied()
    {
        if (::geteuid() == 0)
            QSKIP("running as root: every process can be ended", SkipSingle);
        ProcFsProvider provider;
        const auto r = provider.terminate(1, false);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("Permission"), Qt::CaseInsensitive));
    }
};

QTEST_MAIN(TaskCoreTest)
#include "test_core.moc"
