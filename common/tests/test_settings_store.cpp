#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry;

class SettingsStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("settings.ini"));
        QFile::remove(m_path);
    }

    void returnsDefaultsWhenEmpty()
    {
        SettingsStore store(m_path);
        QCOMPARE(store.value(Keys::Theme).toString(), QStringLiteral("system"));
        QCOMPARE(store.value(Keys::Volume).toInt(), 60);
        QCOMPARE(store.value(Keys::Clock24).toBool(), true);
    }

    void everyKnownKeyHasADefault()
    {
        const QStringList keys = SettingsStore::knownKeys();
        QVERIFY(keys.size() >= 15);
        for (const QString &key : keys)
            QVERIFY2(SettingsStore::defaultValue(key).isValid(), qPrintable(key));
    }

    void storesAndReadsValues()
    {
        SettingsStore store(m_path);
        store.setValue(Keys::Volume, 25);
        QCOMPARE(store.value(Keys::Volume).toInt(), 25);
    }

    void persistsBetweenInstances()
    {
        {
            SettingsStore store(m_path);
            store.setValue(Keys::Theme, QStringLiteral("dark"));
        }
        SettingsStore again(m_path);
        QCOMPARE(again.value(Keys::Theme).toString(), QStringLiteral("dark"));
    }

    void emitsValueChangedOnlyWhenChanged()
    {
        SettingsStore store(m_path);
        QSignalSpy spy(&store, &SettingsStore::valueChanged);

        store.setValue(Keys::Volume, 10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), Keys::Volume);

        store.setValue(Keys::Volume, 10); // same value
        QCOMPARE(spy.count(), 1);

        store.setValue(Keys::Volume, SettingsStore::defaultValue(Keys::Volume)); // changes it back
        QCOMPARE(spy.count(), 2);
    }

    void writingTheDefaultToAnUntouchedKeyIsANoOp()
    {
        SettingsStore store(m_path);
        QSignalSpy spy(&store, &SettingsStore::valueChanged);
        store.setValue(Keys::Volume, SettingsStore::defaultValue(Keys::Volume));
        QCOMPARE(spy.count(), 0);
    }

    void resetAllRestoresDefaultsAndSignals()
    {
        SettingsStore store(m_path);
        store.setValue(Keys::Volume, 5);
        store.setValue(Keys::Hostname, QStringLiteral("other"));
        QSignalSpy spy(&store, &SettingsStore::reloaded);

        store.resetAll();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(store.value(Keys::Volume).toInt(), 60);
        QCOMPARE(store.value(Keys::Hostname).toString(), QStringLiteral("colibry-pc"));
    }

    void localeFollowsRegionSetting()
    {
        SettingsStore store(m_path);
        QCOMPARE(store.locale(), QLocale::system());

        store.setValue(Keys::Region, QStringLiteral("de_DE"));
        QCOMPARE(store.locale().name(), QStringLiteral("de_DE"));
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
};

QTEST_MAIN(SettingsStoreTest)
#include "test_settings_store.moc"
