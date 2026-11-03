#pragma once

#include "ProcessData.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

namespace colibry::tm {

/// Table of processes. Updates change rows in place (insert / remove / dataChanged), never
/// reset the model, so the selection and the scroll position survive every refresh.
class ProcessModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, Pid, User, Cpu, Memory, Status, Threads, Command, ColumnCount };

    enum Role {
        SortRole = Qt::UserRole + 1, ///< value used for sorting: numbers stay numbers
        PidRole,
        ProcessRole, ///< the whole ProcessInfo as QVariant
    };

    explicit ProcessModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// `processes` must be sorted by pid (ProcessMonitor guarantees it).
    void setProcesses(const QVector<ProcessInfo> &processes);

    const ProcessInfo *processAt(int row) const;
    int rowOfPid(qint64 pid) const;

private:
    QVector<ProcessInfo> m_rows; ///< sorted by pid
};

/// Sorting and filtering for the list.
class ProcessFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ProcessFilterProxy(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    QString filterText() const { return m_text; }

    /// Show only processes of this user ("" = everybody).
    void setOnlyUser(const QString &user);
    QString onlyUser() const { return m_user; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_text;
    QString m_user;
};

} // namespace colibry::tm
