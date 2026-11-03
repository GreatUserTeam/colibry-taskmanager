#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeManager.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry;

class ThemeSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void appliesStoredThemeAtStartup()
    {
        QTemporaryDir dir;
        SettingsStore store(dir.filePath(QStringLiteral("s.ini")));
        store.setValue(Keys::Theme, QStringLiteral("dark"));
        store.setValue(Keys::Accent, QStringLiteral("#ef4444"));

        ThemeSync sync(*qobject_cast<QApplication *>(QApplication::instance()), store);

        QVERIFY(qApp->palette().color(QPalette::Window).lightness() < 100);
        QCOMPARE(qApp->palette().color(QPalette::Highlight), QColor(QStringLiteral("#ef4444")));
    }

    void picksUpChangesWrittenByAnotherProcess()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("shared.ini"));

        SettingsStore settingsApp(path); // plays the Settings application
        settingsApp.setValue(Keys::Theme, QStringLiteral("light"));
        settingsApp.sync();

        SettingsStore otherApp(path); // plays another running application
        ThemeSync sync(*qobject_cast<QApplication *>(QApplication::instance()), otherApp);
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() > 200);

        settingsApp.setValue(Keys::Theme, QStringLiteral("dark"));
        settingsApp.sync(); // flushes to disk

        QTRY_VERIFY_WITH_TIMEOUT(qApp->palette().color(QPalette::Window).lightness() < 100, 5000);
        QCOMPARE(otherApp.value(Keys::Theme).toString(), QStringLiteral("dark"));
    }
};

QTEST_MAIN(ThemeSyncTest)
#include "test_theme_sync.moc"
