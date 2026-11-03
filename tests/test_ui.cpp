#include "HistoryGraph.h"
#include "TaskWindow.h"
#include "core/ProcFsProvider.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace colibry;
using namespace colibry::tm;

namespace {

/// A pretend operating system whose processes the test controls.
class FakeProvider : public ProcessProvider
{
public:
    RawSnapshot read() override
    {
        ++reads;
        RawSnapshot s;
        s.system.cpuCount = 2;
        s.system.totalJiffies = ticks;
        s.system.idleJiffies = ticks / 2;
        s.system.memTotal = 1000;
        s.system.memAvailable = 400;
        s.system.uptimeSeconds = 90061;
        s.system.load1 = 0.5;
        for (RawProcess p : processes) {
            p.cpuJiffies = jiffies.value(p.pid, 0);
            s.processes.append(p);
        }
        return s;
    }

    Result terminate(qint64 pid, bool force) override
    {
        calls.append({pid, force});
        if (!terminateResult.ok)
            return terminateResult;
        for (int i = 0; i < processes.size(); ++i) {
            if (processes.at(i).pid == pid) {
                processes.removeAt(i);
                break;
            }
        }
        return {true, {}};
    }

    void add(qint64 pid, const QString &name, const QString &user = QStringLiteral("alice"), quint32 uid = 1000)
    {
        RawProcess p;
        p.pid = pid;
        p.name = name;
        p.user = user;
        p.uid = uid;
        p.command = QStringLiteral("/usr/bin/") + name;
        p.rssBytes = pid * 1000;
        p.startTime = 1;
        processes.append(p);
    }

    QVector<RawProcess> processes;
    QHash<qint64, quint64> jiffies;
    quint64 ticks = 1000;
    int reads = 0;
    QVector<QPair<qint64, bool>> calls;
    Result terminateResult{true, {}};
};

} // namespace

class TaskUiTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_store = std::make_unique<SettingsStore>(m_dir.filePath(QStringLiteral("t-%1.ini").arg(++m_n)));
    }

private:
    struct Fixture
    {
        std::unique_ptr<TaskWindow> window;
        FakeProvider *fake = nullptr;
    };

    Fixture make(bool show = true)
    {
        auto fake = std::make_unique<FakeProvider>();
        FakeProvider *raw = fake.get();
        raw->add(10, QStringLiteral("alpha"));
        raw->add(20, QStringLiteral("beta"), QStringLiteral("root"), 0);
        raw->add(30, QStringLiteral("gamma"));
        raw->add(QCoreApplication::applicationPid(), QStringLiteral("taskmanager"));

        Fixture f;
        f.fake = raw;
        f.window = std::make_unique<TaskWindow>(std::move(fake), *m_store);
        f.window->setInterval(0); // tests refresh by hand unless they say otherwise
        if (show) {
            f.window->show();
            if (!QTest::qWaitForWindowExposed(f.window.get()))
                qFatal("window was not exposed");
        }
        return f;
    }

private slots:
    // ---- listing --------------------------------------------------------
    void listsProcessesAtStartup()
    {
        auto f = make();
        QCOMPARE(f.window->visibleCount(), 4);
        QCOMPARE(f.window->systemInfo().processCount, 4);
        QVERIFY(f.window->summaryText().contains(QStringLiteral("4 processes")));
    }

    void cpuUsageAppearsAfterSecondRefresh()
    {
        auto f = make();
        f.fake->ticks = 1200; // 200 ticks on 2 cores
        f.fake->jiffies[10] = 100; // alpha used 100 of them = half the machine = one full core
        f.window->refreshNow();
        const int row = f.window->model()->rowOfPid(10);
        QCOMPARE(f.window->model()->processAt(row)->cpu, 100.0);
        QCOMPARE(f.window->model()->index(row, ProcessModel::Cpu).data().toString(), QStringLiteral("100.0%"));
        QVERIFY(f.window->summaryText().contains(QStringLiteral("CPU 50.0%")));
    }

    void defaultSortIsCpuDescending()
    {
        auto f = make();
        QCOMPARE(f.window->sortColumn(), int(ProcessModel::Cpu));
        QCOMPARE(f.window->sortOrder(), Qt::DescendingOrder);

        f.fake->ticks = 2000;
        f.fake->jiffies[30] = 800;
        f.fake->jiffies[10] = 100;
        f.window->refreshNow();
        QCOMPARE(f.window->proxy()->index(0, ProcessModel::Pid).data().toLongLong(), 30);
        QCOMPARE(f.window->proxy()->index(1, ProcessModel::Pid).data().toLongLong(), 10);
    }

    void sortByNameAndPid()
    {
        auto f = make();
        f.window->sortBy(ProcessModel::Name, Qt::AscendingOrder);
        QCOMPARE(f.window->proxy()->index(0, ProcessModel::Name).data().toString(), QStringLiteral("alpha"));
        f.window->sortBy(ProcessModel::Pid, Qt::DescendingOrder);
        QVERIFY(f.window->proxy()->index(0, ProcessModel::Pid).data().toLongLong()
                > f.window->proxy()->index(1, ProcessModel::Pid).data().toLongLong());
    }

    void newAndFinishedProcessesAppearAndDisappear()
    {
        auto f = make();
        f.fake->add(40, QStringLiteral("delta"));
        f.window->refreshNow();
        QCOMPARE(f.window->visibleCount(), 5);
        f.fake->processes.removeIf([](const RawProcess &p) { return p.pid == 10; });
        f.window->refreshNow();
        QCOMPARE(f.window->visibleCount(), 4);
        QCOMPARE(f.window->model()->rowOfPid(10), -1);
    }

    // ---- filter ---------------------------------------------------------
    void filterNarrowsListAndSummaryMentionsIt()
    {
        auto f = make();
        f.window->setFilterText(QStringLiteral("ALP"));
        QCOMPARE(f.window->visibleCount(), 1);
        QVERIFY(f.window->summaryText().contains(QStringLiteral("(1 shown)")));
        f.window->setFilterText(QString());
        QCOMPARE(f.window->visibleCount(), 4);
        QVERIFY(!f.window->summaryText().contains(QStringLiteral("shown")));
    }

    void onlyMineHidesOtherUsers()
    {
        auto f = make();
        QCOMPARE(f.window->currentUser(), QStringLiteral("alice")); // taken from this very process
        f.window->setOnlyMine(true);
        QVERIFY(f.window->onlyMine());
        QCOMPARE(f.window->visibleCount(), 3); // beta belongs to root
        f.window->setOnlyMine(false);
        QCOMPARE(f.window->visibleCount(), 4);
    }

    // ---- selection ------------------------------------------------------
    void selectionSurvivesRefreshesAndResorting()
    {
        auto f = make();
        QVERIFY(f.window->selectPid(20));
        QCOMPARE(f.window->selectedPid(), 20);

        f.fake->ticks = 5000;
        f.fake->jiffies[30] = 900; // reorders the list
        f.window->refreshNow();
        QCOMPARE(f.window->selectedPid(), 20);

        f.window->sortBy(ProcessModel::Name, Qt::DescendingOrder);
        QCOMPARE(f.window->selectedPid(), 20);
    }

    void selectionClearsWhenTheProcessEnds()
    {
        auto f = make();
        QVERIFY(f.window->selectPid(30));
        f.fake->processes.removeIf([](const RawProcess &p) { return p.pid == 30; });
        f.window->refreshNow();
        QCOMPARE(f.window->selectedPid(), -1); // not the neighbouring process
        // ...so pressing "End Process" now cannot hit an unrelated program.
        QVERIFY(!f.window->endSelected(false).ok);
        QVERIFY(f.fake->calls.isEmpty());
    }

    void cannotSelectHiddenOrUnknownProcess()
    {
        auto f = make();
        QVERIFY(!f.window->selectPid(999));
        f.window->setFilterText(QStringLiteral("alpha"));
        QVERIFY(!f.window->selectPid(30)); // hidden by the filter
    }

    // ---- ending processes -----------------------------------------------
    void endProcessCallsProviderAndRefreshes()
    {
        auto f = make();
        const auto r = f.window->endProcess(30, false);
        QVERIFY(r.ok);
        QVERIFY(r.message.contains(QStringLiteral("gamma")));
        QCOMPARE(f.fake->calls.size(), 1);
        QCOMPARE(f.fake->calls.first().first, 30);
        QCOMPARE(f.fake->calls.first().second, false);
        QCOMPARE(f.window->model()->rowOfPid(30), -1); // list refreshed
    }

    void forceEndPassesTheForceFlag()
    {
        auto f = make();
        QVERIFY(f.window->selectPid(10));
        QVERIFY(f.window->endSelected(true).ok);
        QCOMPARE(f.fake->calls.first().second, true);
    }

    void refusesToEndPidOneAndItself()
    {
        auto f = make();
        const auto init = f.window->endProcess(1, true);
        QVERIFY(!init.ok);
        QVERIFY(init.message.contains(QStringLiteral("PID 1")));

        const auto self = f.window->endProcess(QCoreApplication::applicationPid(), true);
        QVERIFY(!self.ok);
        QVERIFY(self.message.contains(QStringLiteral("itself")));

        QVERIFY(!f.window->endProcess(0, false).ok);
        QVERIFY(!f.window->endProcess(-4, false).ok);
        QVERIFY(f.fake->calls.isEmpty()); // the provider was never asked
    }

    void endSelectedWithoutSelectionDoesNothing()
    {
        auto f = make();
        QVERIFY(!f.window->endSelected(false).ok);
        QVERIFY(f.fake->calls.isEmpty());
    }

    void providerErrorsReachTheUser()
    {
        auto f = make();
        f.fake->terminateResult = {false, QStringLiteral("Permission denied.")};
        const auto r = f.window->endProcess(20, false);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains(QStringLiteral("Permission denied.")));
        QVERIFY(r.message.contains(QStringLiteral("beta")));
        QVERIFY(f.window->model()->rowOfPid(20) >= 0); // still listed
    }

    // ---- refresh timer --------------------------------------------------
    void timerRefreshesAndPauseStopsIt()
    {
        auto f = make();
        f.window->setInterval(40);
        QVERIFY(f.window->isTimerRunning());
        const int before = f.fake->reads;
        QTRY_VERIFY_WITH_TIMEOUT(f.fake->reads >= before + 3, 3000);

        f.window->setInterval(0);
        QVERIFY(f.window->isPaused());
        QVERIFY(!f.window->isTimerRunning());
        const int paused = f.fake->reads;
        QTest::qWait(200);
        QCOMPARE(f.fake->reads, paused);
    }

    void hiddenWindowDoesNotPoll()
    {
        auto f = make();
        f.window->setInterval(40);
        QVERIFY(f.window->isTimerRunning());
        f.window->hide();
        QVERIFY(!f.window->isTimerRunning());
        const int count = f.fake->reads;
        QTest::qWait(200);
        QCOMPARE(f.fake->reads, count);
        f.window->show();
        QVERIFY(f.window->isTimerRunning());
    }

    // ---- performance ----------------------------------------------------
    void graphsCollectSamples()
    {
        auto f = make();
        QCOMPARE(f.window->cpuGraph()->samples().size(), 1); // the refresh at startup
        f.fake->ticks = 1400;
        f.window->refreshNow();
        f.window->refreshNow();
        QCOMPARE(f.window->cpuGraph()->samples().size(), 3);
        QCOMPARE(f.window->memoryGraph()->samples().last(), 0.6); // 600 of 1000 used
        for (double v : f.window->cpuGraph()->samples()) {
            QVERIFY(v >= 0.0);
            QVERIFY(v <= 1.0);
        }
    }

    void performanceTabShowsSystemNumbers()
    {
        auto f = make();
        f.window->showPerformance(true);
        QVERIFY(f.window->isPerformanceShown());
        QCOMPARE(f.window->performanceText(QStringLiteral("cores")), QStringLiteral("2"));
        QCOMPARE(f.window->performanceText(QStringLiteral("processes")), QStringLiteral("4"));
        QCOMPARE(f.window->performanceText(QStringLiteral("uptime")), QStringLiteral("1 d 01:01:01"));
        QVERIFY(f.window->performanceText(QStringLiteral("memory")).contains(QStringLiteral("60%")));
        QCOMPARE(f.window->performanceText(QStringLiteral("swap")), QStringLiteral("none"));
        QVERIFY(f.window->performanceText(QStringLiteral("load")).startsWith(QStringLiteral("0.50")));
        QVERIFY(f.window->performanceText(QStringLiteral("nonsense")).isEmpty());
        f.window->showPerformance(false);
        QVERIFY(!f.window->isPerformanceShown());
    }

    void graphCapsHistoryAndClampsValues()
    {
        HistoryGraph g;
        g.setCapacity(5);
        for (int i = 0; i < 20; ++i)
            g.addSample(i * 0.1);
        QCOMPARE(g.samples().size(), 5);
        QCOMPARE(g.samples().last(), 1.0); // 1.9 clamped
        g.addSample(-3);
        QCOMPARE(g.samples().last(), 0.0);
        g.addSample(std::nan(""));
        QCOMPARE(g.samples().last(), 0.0);
        g.setCapacity(2);
        QCOMPARE(g.samples().size(), 2);
        g.clear();
        QVERIFY(g.samples().isEmpty());
        g.setCapacity(1); // never below 2: a graph needs two points to draw a line
        QCOMPARE(g.capacity(), 2);
    }

    // ---- remembered state -----------------------------------------------
    void settingsAreRemembered()
    {
        {
            auto f = make();
            f.window->sortBy(ProcessModel::Memory, Qt::AscendingOrder);
            f.window->setInterval(3000);
            f.window->setOnlyMine(true);
            f.window->close();
        }
        auto again = make();
        QCOMPARE(again.window->sortColumn(), int(ProcessModel::Memory));
        QCOMPARE(again.window->sortOrder(), Qt::AscendingOrder);
        QVERIFY(!m_store->value(Keys::TaskGeometry).toByteArray().isEmpty());
        QCOMPARE(m_store->value(Keys::TaskOnlyMine).toBool(), true);
    }

    void intervalIsRestored()
    {
        {
            auto f = make();
            f.window->setInterval(500);
        }
        auto fake = std::make_unique<FakeProvider>();
        TaskWindow w(std::move(fake), *m_store);
        QCOMPARE(w.interval(), 500);
    }

    // ---- the real thing ---------------------------------------------------
    void realProcFsShowsThisTestProcess()
    {
        if (!QFileInfo(QStringLiteral("/proc/self/stat")).exists())
            QSKIP("no /proc on this system", SkipSingle);
        TaskWindow w(std::make_unique<ProcFsProvider>(), *m_store);
        w.setInterval(0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.visibleCount() > 1);
        QVERIFY(w.selectPid(QCoreApplication::applicationPid()));
        QVERIFY(w.systemInfo().memTotal > 0);
        QVERIFY(!w.currentUser().isEmpty());

        w.refreshNow();
        QVERIFY(w.systemInfo().cpuPercent >= 0.0 && w.systemInfo().cpuPercent <= 100.0);

        // Safety rules hold with the real provider as well.
        QVERIFY(!w.endProcess(1, true).ok);
        QVERIFY(!w.endProcess(QCoreApplication::applicationPid(), false).ok);
    }

    void endsARealProcessThroughTheWindow()
    {
        if (!QFileInfo(QStringLiteral("/proc/self/stat")).exists())
            QSKIP("no /proc on this system", SkipSingle);
        QProcess child;
        child.start(QStringLiteral("sleep"), {QStringLiteral("60")});
        QVERIFY(child.waitForStarted(5000));

        TaskWindow w(std::make_unique<ProcFsProvider>(), *m_store);
        w.setInterval(0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.selectPid(child.processId()));

        const auto r = w.endSelected(false);
        QVERIFY2(r.ok, qPrintable(r.message));
        QVERIFY(child.waitForFinished(5000));
        w.refreshNow();
        QCOMPARE(w.model()->rowOfPid(child.processId()), -1);
    }

private:
    QTemporaryDir m_dir;
    std::unique_ptr<SettingsStore> m_store;
    int m_n = 0;
};

QTEST_MAIN(TaskUiTest)
#include "test_ui.moc"
