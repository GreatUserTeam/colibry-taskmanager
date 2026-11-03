#pragma once

#include "core/ProcessModel.h"
#include "core/ProcessMonitor.h"
#include "core/ProcessProvider.h"

#include <QMainWindow>

#include <memory>

class QAction;
class QLabel;
class QLineEdit;
class QTabWidget;
class QTimer;
class QTreeView;

namespace colibry {
class SettingsStore;
}

namespace colibry::tm {

class HistoryGraph;

/// The Task Manager window. Every user action is also a public method without dialogs.
class TaskWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct Outcome
    {
        bool ok = false;
        QString message;
    };

    TaskWindow(std::unique_ptr<ProcessProvider> provider, SettingsStore &store, QWidget *parent = nullptr);
    ~TaskWindow() override;

    // Refreshing
    void refreshNow();
    /// Milliseconds between refreshes; 0 pauses.
    void setInterval(int ms);
    int interval() const { return m_interval; }
    bool isPaused() const { return m_interval == 0; }
    bool isTimerRunning() const;

    // The list
    ProcessModel *model() const { return m_model; }
    ProcessFilterProxy *proxy() const { return m_proxy; }
    int visibleCount() const;
    void setFilterText(const QString &text);
    void setOnlyMine(bool on);
    bool onlyMine() const;
    QString currentUser() const;
    void sortBy(int column, Qt::SortOrder order);
    int sortColumn() const;
    Qt::SortOrder sortOrder() const;

    // Selection
    qint64 selectedPid() const;
    bool selectPid(qint64 pid);

    // Ending processes. These apply the safety rules and show no dialog.
    Outcome endProcess(qint64 pid, bool force);
    Outcome endSelected(bool force);

    // Numbers
    const SystemInfo &systemInfo() const { return m_monitor.system(); }
    QString summaryText() const;
    HistoryGraph *cpuGraph() const { return m_cpuGraph; }
    HistoryGraph *memoryGraph() const { return m_memGraph; }
    void showPerformance(bool performance);
    bool isPerformanceShown() const;
    QString performanceText(const QString &key) const;

    static constexpr int kDefaultInterval = 1000;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void buildUi();
    void buildActions();
    void buildMenus();
    void updateActions();
    void updateSummary();
    void updatePerformance();
    void promptEnd(bool force);
    void showContextMenu(const QPoint &pos);
    void copyCommand();

    std::unique_ptr<ProcessProvider> m_provider;
    SettingsStore &m_store;
    ProcessMonitor m_monitor;
    ProcessModel *m_model = nullptr;
    ProcessFilterProxy *m_proxy = nullptr;

    QTimer *m_timer = nullptr;
    int m_interval = kDefaultInterval;

    QTabWidget *m_tabs = nullptr;
    QTreeView *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
    QLabel *m_status = nullptr;
    HistoryGraph *m_cpuGraph = nullptr;
    HistoryGraph *m_memGraph = nullptr;
    QHash<QString, QLabel *> m_values;

    QAction *m_end = nullptr;
    QAction *m_forceEnd = nullptr;
    QAction *m_copyCommand = nullptr;
    QAction *m_refresh = nullptr;
    QAction *m_mine = nullptr;
    QList<QAction *> m_speedActions;
};

} // namespace colibry::tm
