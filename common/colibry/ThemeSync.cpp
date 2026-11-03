#include "ThemeSync.h"

#include "Keys.h"
#include "SettingsStore.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTimer>

namespace colibry {

ThemeSync::ThemeSync(QApplication &app, SettingsStore &store, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_store(store)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounce(new QTimer(this))
{
    ThemeManager::apply(m_app, m_store);

    connect(&m_store, &SettingsStore::valueChanged, this, [this](const QString &key) {
        if (key == Keys::Theme || key == Keys::Accent || key == Keys::FontSize)
            ThemeManager::apply(m_app, m_store);
    });
    connect(&m_store, &SettingsStore::reloaded, this, [this] { ThemeManager::apply(m_app, m_store); });

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_store.value(Keys::Theme).toString() == QStringLiteral("system"))
            ThemeManager::apply(m_app, m_store);
    });
#endif

    // Editors replace the file instead of rewriting it, so watch the folder too.
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);
    connect(m_debounce, &QTimer::timeout, this, [this] {
        watchFile();
        m_store.sync();
    });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { onFileActivity(); });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { onFileActivity(); });

    const QFileInfo info(m_store.fileName());
    QDir().mkpath(info.absolutePath());
    m_watcher->addPath(info.absolutePath());
    watchFile();
}

void ThemeSync::onFileActivity()
{
    m_debounce->start();
}

void ThemeSync::watchFile()
{
    const QString file = m_store.fileName();
    if (QFileInfo::exists(file) && !m_watcher->files().contains(file))
        m_watcher->addPath(file);
}

} // namespace colibry
