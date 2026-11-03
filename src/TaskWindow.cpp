#include "TaskWindow.h"

#include "HistoryGraph.h"
#include "core/Format.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

namespace colibry::tm {

namespace {

constexpr int kIntervals[] = {500, 1000, 3000};

QString percentOf(qint64 part, qint64 whole)
{
    if (whole <= 0)
        return QStringLiteral("0%");
    return QStringLiteral("%1%").arg(qRound(100.0 * double(part) / double(whole)));
}

} // namespace

TaskWindow::TaskWindow(std::unique_ptr<ProcessProvider> provider, SettingsStore &store, QWidget *parent)
    : QMainWindow(parent)
    , m_provider(std::move(provider))
    , m_store(store)
{
    setWindowTitle(tr("Task Manager"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-system-monitor")));
    resize(960, 620);
    setMinimumSize(560, 380);

    m_model = new ProcessModel(this);
    m_proxy = new ProcessFilterProxy(this);
    m_proxy->setSourceModel(m_model);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] { refreshNow(); });

    buildUi();
    buildActions();
    buildMenus();

    // Restore the previous state.
    const QByteArray geometry = m_store.value(Keys::TaskGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const int column = m_store.value(Keys::TaskSortColumn).isValid() ? m_store.value(Keys::TaskSortColumn).toInt()
                                                                     : int(ProcessModel::Cpu);
    const int order = m_store.value(Keys::TaskSortOrder).isValid() ? m_store.value(Keys::TaskSortOrder).toInt() : 1;
    sortBy(column >= 0 && column < ProcessModel::ColumnCount ? column : int(ProcessModel::Cpu),
           order == 0 ? Qt::AscendingOrder : Qt::DescendingOrder);

    const int saved = m_store.value(Keys::TaskInterval).isValid() ? m_store.value(Keys::TaskInterval).toInt()
                                                                  : kDefaultInterval;
    m_interval = saved < 0 ? kDefaultInterval : saved;
    setInterval(m_interval);

    refreshNow();
    m_mine->setChecked(m_store.value(Keys::TaskOnlyMine).toBool());
}

TaskWindow::~TaskWindow() = default;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void TaskWindow::buildUi()
{
    m_tabs = new QTabWidget(this);

    // --- Processes ------------------------------------------------------
    auto *processes = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(processes);
    layout->setContentsMargins(8, 8, 8, 8);

    m_filter = new QLineEdit(processes);
    m_filter->setPlaceholderText(tr("Filter by name, user, PID or command (Ctrl+F)"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) { setFilterText(text); });

    m_view = new QTreeView(processes);
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSortingEnabled(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection); // one process at a time is safer
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->header()->setStretchLastSection(true);
    m_view->header()->setSectionsMovable(true);
    m_view->setColumnWidth(ProcessModel::Name, 220);
    m_view->setColumnWidth(ProcessModel::Pid, 70);
    m_view->setColumnWidth(ProcessModel::User, 100);
    m_view->setColumnWidth(ProcessModel::Cpu, 80);
    m_view->setColumnWidth(ProcessModel::Memory, 100);
    m_view->setColumnWidth(ProcessModel::Status, 110);
    m_view->setColumnWidth(ProcessModel::Threads, 70);
    layout->addWidget(m_view, 1);

    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) { showContextMenu(pos); });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    connect(m_view->header(), &QHeaderView::sortIndicatorChanged, this, [this] { updateActions(); });

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *endButton = new QPushButton(tr("End Process"), processes);
    endButton->setObjectName(QStringLiteral("endButton"));
    buttons->addWidget(endButton);
    layout->addLayout(buttons);
    connect(endButton, &QPushButton::clicked, this, [this] { promptEnd(false); });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, endButton,
            [this, endButton] { endButton->setEnabled(selectedPid() >= 0); });
    endButton->setEnabled(false);

    m_tabs->addTab(processes, tr("Processes"));

    // --- Performance ----------------------------------------------------
    auto *performance = new QWidget(m_tabs);
    auto *perfLayout = new QVBoxLayout(performance);
    perfLayout->setContentsMargins(8, 8, 8, 8);

    auto *cpuBox = new QGroupBox(tr("CPU"), performance);
    auto *cpuLayout = new QVBoxLayout(cpuBox);
    m_cpuGraph = new HistoryGraph(cpuBox);
    cpuLayout->addWidget(m_cpuGraph);
    perfLayout->addWidget(cpuBox, 1);

    auto *memBox = new QGroupBox(tr("Memory"), performance);
    auto *memLayout = new QVBoxLayout(memBox);
    m_memGraph = new HistoryGraph(memBox);
    memLayout->addWidget(m_memGraph);
    perfLayout->addWidget(memBox, 1);

    auto *infoBox = new QGroupBox(tr("System"), performance);
    auto *form = new QFormLayout(infoBox);
    const QList<QPair<QString, QString>> rows = {
        {QStringLiteral("cpu"), tr("CPU usage")},         {QStringLiteral("cores"), tr("CPU cores")},
        {QStringLiteral("memory"), tr("Memory")},         {QStringLiteral("swap"), tr("Swap")},
        {QStringLiteral("load"), tr("Load average")},     {QStringLiteral("processes"), tr("Processes")},
        {QStringLiteral("threads"), tr("Threads")},       {QStringLiteral("uptime"), tr("Uptime")},
    };
    for (const auto &row : rows) {
        auto *value = new QLabel(infoBox);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(row.second, value);
        m_values.insert(row.first, value);
    }
    perfLayout->addWidget(infoBox);

    m_tabs->addTab(performance, tr("Performance"));
    setCentralWidget(m_tabs);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);
}

void TaskWindow::buildActions()
{
    auto make = [this](const QString &text, QKeySequence shortcut = {}) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        return action;
    };

    m_end = make(tr("End Process…"), QKeySequence(Qt::Key_Delete));
    m_end->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_forceEnd = make(tr("Force End Process…"), QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    m_forceEnd->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_copyCommand = make(tr("Copy Command Line"));
    m_view->addAction(m_end);
    m_view->addAction(m_forceEnd);

    m_refresh = make(tr("Refresh Now"), QKeySequence::Refresh);
    m_mine = make(tr("Only My Processes"));
    m_mine->setCheckable(true);

    connect(m_end, &QAction::triggered, this, [this] { promptEnd(false); });
    connect(m_forceEnd, &QAction::triggered, this, [this] { promptEnd(true); });
    connect(m_copyCommand, &QAction::triggered, this, [this] { copyCommand(); });
    connect(m_refresh, &QAction::triggered, this, [this] { refreshNow(); });
    connect(m_mine, &QAction::toggled, this, [this](bool on) { setOnlyMine(on); });

    auto *group = new QActionGroup(this);
    const QList<QPair<QString, int>> speeds = {
        {tr("Fast (0.5 s)"), kIntervals[0]}, {tr("Normal (1 s)"), kIntervals[1]},
        {tr("Slow (3 s)"), kIntervals[2]},   {tr("Paused"), 0},
    };
    for (const auto &speed : speeds) {
        QAction *action = make(speed.first);
        action->setCheckable(true);
        action->setData(speed.second);
        group->addAction(action);
        m_speedActions.append(action);
        const int ms = speed.second;
        connect(action, &QAction::triggered, this, [this, ms] { setInterval(ms); });
    }

    auto *focusFilter = make(QString(), QKeySequence::Find);
    connect(focusFilter, &QAction::triggered, this, [this] {
        m_tabs->setCurrentIndex(0);
        m_filter->setFocus();
        m_filter->selectAll();
    });
    addAction(focusFilter);
    addAction(m_refresh);

    updateActions();
}

void TaskWindow::buildMenus()
{
    QMenu *process = menuBar()->addMenu(tr("&Process"));
    process->addAction(m_end);
    process->addAction(m_forceEnd);
    process->addSeparator();
    process->addAction(m_copyCommand);
    process->addSeparator();
    QAction *quit = process->addAction(tr("Close"));
    quit->setShortcut(QKeySequence::Close);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_refresh);
    view->addSeparator();
    view->addAction(m_mine);
    view->addSeparator();
    QMenu *speed = view->addMenu(tr("Update Speed"));
    for (QAction *action : std::as_const(m_speedActions))
        speed->addAction(action);
}

// ---------------------------------------------------------------------------
// Refreshing
// ---------------------------------------------------------------------------

void TaskWindow::refreshNow()
{
    // If the selected process has ended, Qt would quietly select its neighbour, and the next
    // press of Delete would end an unrelated process. Drop the selection instead.
    const qint64 selected = selectedPid();

    m_monitor.update(m_provider->read());
    m_model->setProcesses(m_monitor.processes());

    if (selected >= 0 && m_model->rowOfPid(selected) < 0) {
        m_view->selectionModel()->clearSelection();
        m_view->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }

    const SystemInfo &sys = m_monitor.system();
    m_cpuGraph->addSample(sys.cpuPercent / 100.0);
    m_memGraph->addSample(sys.memTotal > 0 ? double(sys.memUsed) / double(sys.memTotal) : 0.0);
    m_cpuGraph->setLabel(tr("%1").arg(formatPercent(sys.cpuPercent)));
    m_memGraph->setLabel(tr("%1 of %2").arg(formatBytes(sys.memUsed), formatBytes(sys.memTotal)));

    updateSummary();
    updatePerformance();
    updateActions();
}

void TaskWindow::setInterval(int ms)
{
    m_interval = std::max(0, ms);

    for (QAction *action : std::as_const(m_speedActions)) {
        if (action->data().toInt() == m_interval) {
            const QSignalBlocker blocker(action);
            action->setChecked(true);
        }
    }

    if (m_interval > 0 && isVisible())
        m_timer->start(m_interval);
    else
        m_timer->stop();

    m_store.setValue(Keys::TaskInterval, m_interval);
}

bool TaskWindow::isTimerRunning() const
{
    return m_timer->isActive();
}

void TaskWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_interval > 0)
        m_timer->start(m_interval);
}

void TaskWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    m_timer->stop(); // nobody is looking: do not spend CPU on it
}

// ---------------------------------------------------------------------------
// The list
// ---------------------------------------------------------------------------

int TaskWindow::visibleCount() const
{
    return m_proxy->rowCount();
}

void TaskWindow::setFilterText(const QString &text)
{
    m_proxy->setFilterText(text);
    if (m_filter->text() != text) {
        const QSignalBlocker blocker(m_filter);
        m_filter->setText(text);
    }
    updateSummary();
}

QString TaskWindow::currentUser() const
{
    // The user of this very process, as the system reports it.
    const int row = m_model->rowOfPid(QCoreApplication::applicationPid());
    if (const ProcessInfo *self = m_model->processAt(row))
        return self->user;
    return qEnvironmentVariable("USER", qEnvironmentVariable("LOGNAME"));
}

void TaskWindow::setOnlyMine(bool on)
{
    m_proxy->setOnlyUser(on ? currentUser() : QString());
    {
        const QSignalBlocker blocker(m_mine);
        m_mine->setChecked(on);
    }
    m_store.setValue(Keys::TaskOnlyMine, on);
    updateSummary();
}

bool TaskWindow::onlyMine() const
{
    return !m_proxy->onlyUser().isEmpty();
}

void TaskWindow::sortBy(int column, Qt::SortOrder order)
{
    m_view->sortByColumn(column, order);
    m_store.setValue(Keys::TaskSortColumn, column);
    m_store.setValue(Keys::TaskSortOrder, order == Qt::AscendingOrder ? 0 : 1);
}

int TaskWindow::sortColumn() const
{
    return m_view->header()->sortIndicatorSection();
}

Qt::SortOrder TaskWindow::sortOrder() const
{
    return m_view->header()->sortIndicatorOrder();
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

qint64 TaskWindow::selectedPid() const
{
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return -1;
    return rows.first().data(ProcessModel::PidRole).toLongLong();
}

bool TaskWindow::selectPid(qint64 pid)
{
    const int row = m_model->rowOfPid(pid);
    if (row < 0)
        return false;
    const QModelIndex index = m_proxy->mapFromSource(m_model->index(row, 0));
    if (!index.isValid())
        return false; // hidden by the filter
    m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_view->scrollTo(index);
    return true;
}

// ---------------------------------------------------------------------------
// Ending processes
// ---------------------------------------------------------------------------

TaskWindow::Outcome TaskWindow::endProcess(qint64 pid, bool force)
{
    if (pid <= 0)
        return {false, tr("No process selected.")};
    if (pid == 1)
        return {false, tr("This is the system's first process (PID 1). Ending it would stop the whole system.")};
    if (pid == QCoreApplication::applicationPid())
        return {false, tr("Task Manager cannot end itself. Close the window instead.")};

    QString name = QString::number(pid);
    if (const ProcessInfo *info = m_model->processAt(m_model->rowOfPid(pid)))
        name = QStringLiteral("%1 (PID %2)").arg(info->name).arg(pid);

    const ProcessProvider::Result result = m_provider->terminate(pid, force);
    if (!result.ok)
        return {false, tr("Could not end %1: %2").arg(name, result.error)};

    refreshNow();
    return {true, force ? tr("Force-ended %1").arg(name) : tr("Asked %1 to quit").arg(name)};
}

TaskWindow::Outcome TaskWindow::endSelected(bool force)
{
    return endProcess(selectedPid(), force);
}

void TaskWindow::promptEnd(bool force)
{
    const qint64 pid = selectedPid();
    if (pid < 0)
        return;

    const ProcessInfo *info = m_model->processAt(m_model->rowOfPid(pid));
    const QString name = info ? info->name : QString::number(pid);

    QString text = force ? tr("Force \"%1\" (PID %2) to end immediately?\n\nUnsaved data in it will be lost.").arg(name).arg(pid)
                         : tr("Ask \"%1\" (PID %2) to quit?").arg(name).arg(pid);
    if (info && info->uid == 0)
        text += tr("\n\nThis is a system process. Ending it may make the system unstable.");

    const auto answer = QMessageBox::warning(this, force ? tr("Force End Process") : tr("End Process"), text,
                                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    const Outcome outcome = endProcess(pid, force);
    if (outcome.ok)
        statusBar()->showMessage(outcome.message, 5000);
    else
        QMessageBox::warning(this, tr("Task Manager"), outcome.message);
}

void TaskWindow::copyCommand()
{
    const ProcessInfo *info = m_model->processAt(m_model->rowOfPid(selectedPid()));
    if (info)
        QGuiApplication::clipboard()->setText(info->command);
}

void TaskWindow::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (index.isValid())
        m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else
        m_view->selectionModel()->clearSelection();
    updateActions();

    if (!index.isValid())
        return;
    QMenu menu(this);
    menu.addAction(m_end);
    menu.addAction(m_forceEnd);
    menu.addSeparator();
    menu.addAction(m_copyCommand);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Numbers shown
// ---------------------------------------------------------------------------

QString TaskWindow::summaryText() const
{
    const SystemInfo &s = m_monitor.system();
    QString text = tr("%1 processes").arg(s.processCount);
    if (visibleCount() != s.processCount)
        text += tr(" (%1 shown)").arg(visibleCount());
    text += tr("  ·  CPU %1").arg(formatPercent(s.cpuPercent));
    text += tr("  ·  Memory %1 of %2 (%3)")
                .arg(formatBytes(s.memUsed), formatBytes(s.memTotal), percentOf(s.memUsed, s.memTotal));
    return text;
}

void TaskWindow::updateSummary()
{
    m_status->setText(summaryText());
}

QString TaskWindow::performanceText(const QString &key) const
{
    const QLabel *label = m_values.value(key);
    return label ? label->text() : QString();
}

void TaskWindow::updatePerformance()
{
    const SystemInfo &s = m_monitor.system();
    auto set = [this](const QString &key, const QString &text) {
        if (QLabel *label = m_values.value(key))
            label->setText(text);
    };

    set(QStringLiteral("cpu"), formatPercent(s.cpuPercent));
    set(QStringLiteral("cores"), QString::number(s.cpuCount));
    set(QStringLiteral("memory"), tr("%1 of %2 (%3)")
                                      .arg(formatBytes(s.memUsed), formatBytes(s.memTotal), percentOf(s.memUsed, s.memTotal)));
    set(QStringLiteral("swap"), s.swapTotal > 0
                                    ? tr("%1 of %2 (%3)")
                                          .arg(formatBytes(s.swapUsed), formatBytes(s.swapTotal), percentOf(s.swapUsed, s.swapTotal))
                                    : tr("none"));
    set(QStringLiteral("load"), QStringLiteral("%1   %2   %3")
                                    .arg(s.load1, 0, 'f', 2)
                                    .arg(s.load5, 0, 'f', 2)
                                    .arg(s.load15, 0, 'f', 2));
    set(QStringLiteral("processes"), QString::number(s.processCount));
    set(QStringLiteral("threads"), QString::number(s.threadCount));
    set(QStringLiteral("uptime"), formatUptime(s.uptimeSeconds));
}

void TaskWindow::showPerformance(bool performance)
{
    m_tabs->setCurrentIndex(performance ? 1 : 0);
}

bool TaskWindow::isPerformanceShown() const
{
    return m_tabs->currentIndex() == 1;
}

void TaskWindow::updateActions()
{
    const bool selected = selectedPid() >= 0;
    m_end->setEnabled(selected);
    m_forceEnd->setEnabled(selected);
    m_copyCommand->setEnabled(selected);
}

void TaskWindow::closeEvent(QCloseEvent *event)
{
    m_store.setValue(Keys::TaskGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace colibry::tm
