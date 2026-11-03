#include "SettingsStore.h"

#include "Keys.h"

#include <QHash>

namespace colibry {

namespace {

const QHash<QString, QVariant> &defaults()
{
    static const QHash<QString, QVariant> d = {
        {Keys::Theme, QStringLiteral("system")},
        {Keys::Accent, QStringLiteral("#3b82f6")},
        {Keys::FontSize, 10},

        {Keys::Language, QStringLiteral("en")},
        {Keys::Region, QString()},
        {Keys::FirstDayOfWeek, 1},

        {Keys::Scale, 100},
        {Keys::Brightness, 80},
        {Keys::NightLight, false},
        {Keys::NightLightStrength, 40},

        {Keys::Volume, 60},
        {Keys::Muted, false},
        {Keys::SystemSounds, true},

        {Keys::Hostname, QStringLiteral("colibry-pc")},

        {Keys::AutoTime, true},
        {Keys::TimeZone, QString()},
        {Keys::Clock24, true},
    };
    return d;
}

} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("GreatUserTeam"), QStringLiteral("colibry-settings"))
{
}

SettingsStore::SettingsStore(const QString &iniFilePath, QObject *parent)
    : QObject(parent)
    , m_settings(iniFilePath, QSettings::IniFormat)
{
}

QVariant SettingsStore::value(const QString &key) const
{
    return m_settings.value(key, defaultValue(key));
}

void SettingsStore::setValue(const QString &key, const QVariant &value)
{
    if (m_settings.contains(key) && m_settings.value(key) == value)
        return;
    if (!m_settings.contains(key) && defaultValue(key) == value)
        return;

    m_settings.setValue(key, value);
    emit valueChanged(key, value);
}

void SettingsStore::resetAll()
{
    m_settings.clear();
    m_settings.sync();
    emit reloaded();
}

QLocale SettingsStore::locale() const
{
    const QString name = value(Keys::Region).toString();
    return name.isEmpty() ? QLocale::system() : QLocale(name);
}

QString SettingsStore::fileName() const
{
    return m_settings.fileName();
}

void SettingsStore::sync()
{
    m_settings.sync();
    emit reloaded();
}

QVariant SettingsStore::defaultValue(const QString &key)
{
    return defaults().value(key);
}

QStringList SettingsStore::knownKeys()
{
    return defaults().keys();
}

} // namespace colibry
