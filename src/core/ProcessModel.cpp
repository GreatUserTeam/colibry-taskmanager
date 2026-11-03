#include "ProcessModel.h"

#include "Format.h"

namespace colibry::tm {

// Table model holding one row per process. Rows are kept sorted by PID,
// which lets setProcesses() apply updates with a single merge pass.
ProcessModel::ProcessModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    // Only the root item owns rows; children are not used.
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int ProcessModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    if (role == Qt::DisplayRole) {
        switch (section) {
        case Name:
            return tr("Name");
        case Pid:
            return tr("PID");
        case User:
            return tr("User");
        case Cpu:
            return tr("CPU");
        case Memory:
            return tr("Memory");
        case Status:
            return tr("Status");
        case Threads:
            return tr("Threads");
        case Command:
            return tr("Command");
        }
    } else if (role == Qt::TextAlignmentRole) {
        // Numeric columns are right-aligned; text columns are left-aligned.
        const bool number = section == Pid || section == Cpu || section == Memory || section == Threads;
        return int((number ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    }
    return {};
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const ProcessInfo &p = m_rows.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        // Formatted text exactly as shown in the table.
        switch (index.column()) {
        case Name:
            return p.name;
        case Pid:
            return p.pid;
        case User:
            return p.user;
        case Cpu:
            return formatPercent(p.cpu);
        case Memory:
            return formatBytes(p.memory);
        case Status:
            return stateText(p.state);
        case Threads:
            return p.threads;
        case Command:
            return p.command;
        }
        break;
    case SortRole:
        // Raw values drive sorting: numbers compare numerically, strings
        // case-folded so ordering does not depend on capitalization.
        switch (index.column()) {
        case Name:
            return p.name.toCaseFolded();
        case Pid:
            return p.pid;
        case User:
            return p.user.toCaseFolded();
        case Cpu:
            return p.cpu;
        case Memory:
            return p.memory;
        case Status:
            return stateText(p.state);
        case Threads:
            return p.threads;
        case Command:
            return p.command.toCaseFolded();
        }
        break;
    case Qt::TextAlignmentRole: {
        const int c = index.column();
        const bool number = c == Pid || c == Cpu || c == Memory || c == Threads;
        return int((number ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    }
    case Qt::ToolTipRole:
        // Full command line fits in the tooltip even when the column is narrow.
        return tr("%1 (PID %2)\n%3").arg(p.name).arg(p.pid).arg(p.command);
    case PidRole:
        return p.pid;
    case ProcessRole:
        // Lets views and tests grab the whole record without re-querying cells.
        return QVariant::fromValue(p);
    default:
        break;
    }
    return {};
}

const ProcessInfo *ProcessModel::processAt(int row) const
{
    return (row >= 0 && row < m_rows.size()) ? &m_rows.at(row) : nullptr;
}

int ProcessModel::rowOfPid(qint64 pid) const
{
    // Binary search: rows stay sorted by PID, so lookup is O(log n).
    int low = 0;
    int high = static_cast<int>(m_rows.size()) - 1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        const qint64 value = m_rows.at(mid).pid;
        if (value == pid)
            return mid;
        if (value < pid)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return -1;
}

void ProcessModel::setProcesses(const QVector<ProcessInfo> &incoming)
{
    // Merge the current rows with the fresh snapshot, both sorted by PID.
    // Every difference is announced with the matching begin/end pair so
    // attached views update incrementally instead of rebuilding.
    int i = 0; // position in the rows we already have
    int j = 0; // position in the incoming list

    while (i < m_rows.size() && j < incoming.size()) {
        const qint64 have = m_rows.at(i).pid;
        const qint64 want = incoming.at(j).pid;

        if (have < want) { // the process is gone
            // Consume the whole run of vanished PIDs in one remove operation.
            int last = i;
            while (last + 1 < m_rows.size() && m_rows.at(last + 1).pid < want)
                ++last;
            beginRemoveRows({}, i, last);
            m_rows.remove(i, last - i + 1);
            endRemoveRows();
        } else if (have > want) { // a new process
            // Consume the whole run of new PIDs in one insert operation.
            int last = j;
            while (last + 1 < incoming.size() && incoming.at(last + 1).pid < have)
                ++last;
            beginInsertRows({}, i, i + (last - j));
            m_rows.insert(i, last - j + 1, ProcessInfo());
            for (int k = j; k <= last; ++k)
                m_rows[i + (k - j)] = incoming.at(k);
            endInsertRows();
            i += last - j + 1;
            j = last + 1;
        } else { // same process: update only when something changed
            // Emitting dataChanged only for real changes keeps the CPU
            // column refresh from redrawing every row twice a second.
            if (m_rows.at(i) != incoming.at(j)) {
                m_rows[i] = incoming.at(j);
                emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
            }
            ++i;
            ++j;
        }
    }

    if (i < m_rows.size()) { // everything left over is gone
        beginRemoveRows({}, i, static_cast<int>(m_rows.size()) - 1);
        m_rows.remove(i, m_rows.size() - i);
        endRemoveRows();
    }
    if (j < incoming.size()) { // everything left over is new
        beginInsertRows({}, i, i + static_cast<int>(incoming.size()) - j - 1);
        for (int k = j; k < incoming.size(); ++k)
            m_rows.append(incoming.at(k));
        endInsertRows();
    }
}

// ---------------------------------------------------------------------------

// Proxy that narrows the full process list down to a user-chosen subset:
// free-text search plus an optional "only this user" restriction.
ProcessFilterProxy::ProcessFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(ProcessModel::SortRole);
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void ProcessFilterProxy::setFilterText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    // Re-evaluate the filter for all rows; invalidate() handles both the
    // filtering and the re-sorting that may follow.
    invalidate();
}

void ProcessFilterProxy::setOnlyUser(const QString &user)
{
    if (m_user == user)
        return;
    m_user = user;
    invalidate();
}

bool ProcessFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    const auto *model = qobject_cast<const ProcessModel *>(sourceModel());
    const ProcessInfo *p = model ? model->processAt(sourceRow) : nullptr;
    if (!p)
        return false;

    // User restriction applies first: a fixed non-matching row is rejected
    // no matter what the search text says.
    if (!m_user.isEmpty() && p->user != m_user)
        return false;

    const QString needle = m_text.trimmed();
    if (needle.isEmpty())
        return true;
    // The needle matches when it occurs in the name, the command line
    // or the user name, or when it is a substring of the PID digits.
    return p->name.contains(needle, Qt::CaseInsensitive) || p->command.contains(needle, Qt::CaseInsensitive)
           || p->user.contains(needle, Qt::CaseInsensitive) || QString::number(p->pid).contains(needle);
}

} // namespace colibry::tm
