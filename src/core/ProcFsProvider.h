#pragma once

#include "ProcessProvider.h"

#include <QHash>

namespace colibry::tm {

/// Reads processes from a Linux-style /proc tree.
/// The locations are parameters so that tests can point it at a fake tree.
class ProcFsProvider : public ProcessProvider
{
public:
    /// pageSize / clockTicks of 0 mean "ask the system".
    explicit ProcFsProvider(QString procRoot = QStringLiteral("/proc"),
                            QString passwdFile = QStringLiteral("/etc/passwd"), long pageSize = 0);

    RawSnapshot read() override;
    Result terminate(qint64 pid, bool force) override;

    RawSystem readSystem() const;
    bool readProcess(qint64 pid, RawProcess *out);

    /// Parses the contents of /proc/<pid>/stat. The name can contain spaces and parentheses,
    /// so the fields are located from the LAST closing parenthesis.
    static bool parseStat(const QByteArray &stat, RawProcess *out, long pageSize);

private:
    QString userName(quint32 uid);

    QString m_root;
    QString m_passwd;
    long m_pageSize;
    QHash<quint32, QString> m_users;
    bool m_usersLoaded = false;
};

} // namespace colibry::tm
