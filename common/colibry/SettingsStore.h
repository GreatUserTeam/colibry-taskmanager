#pragma once

#include <QLocale>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariant>

namespace colibry {

/**
 * Typed-ish wrapper around QSettings with built-in defaults.
 *
 * All pages read and write through this class and never touch QSettings
 * directly, so the storage format can change in one place.
 */
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    /// Uses the per-user INI file (portable, no registry/plist dependency).
    explicit SettingsStore(QObject *parent = nullptr);

    /// Uses an explicit INI file. Intended for tests.
    explicit SettingsStore(const QString &iniFilePath, QObject *parent = nullptr);

    /// Stored value, or the built-in default when the key was never written.
    QVariant value(const QString &key) const;

    /// Writes the value and emits valueChanged() if it actually changed.
    void setValue(const QString &key, const QVariant &value);

    /// Removes every user value (window geometry included) and emits reloaded().
    void resetAll();

    /// Locale chosen in Language & Region (system locale when unset).
    QLocale locale() const;

    /// Path of the INI file backing this store.
    QString fileName() const;

    /// Re-reads the file (another process may have written it) and emits reloaded().
    void sync();

    /// Built-in default for a key (invalid QVariant for unknown keys).
    static QVariant defaultValue(const QString &key);

    /// All keys that have a built-in default.
    static QStringList knownKeys();

signals:
    void valueChanged(const QString &key, const QVariant &value);

    /// Emitted after resetAll(); pages should reload their widgets.
    void reloaded();

private:
    QSettings m_settings;
};

} // namespace colibry
