#pragma once

#include <QObject>

class QApplication;
class QFileSystemWatcher;
class QTimer;

namespace colibry {

class SettingsStore;

/**
 * Keeps an application's look in sync with the Settings app.
 *
 * Applies the stored theme / accent / font size at startup, then watches the
 * shared settings file so that a change made in Settings shows up in every
 * running Colibry application immediately.
 */
class ThemeSync : public QObject
{
    Q_OBJECT

public:
    ThemeSync(QApplication &app, SettingsStore &store, QObject *parent = nullptr);

private:
    void onFileActivity();
    void watchFile();

    QApplication &m_app;
    SettingsStore &m_store;
    QFileSystemWatcher *m_watcher;
    QTimer *m_debounce;
};

} // namespace colibry
