#pragma once

#include <QString>

namespace colibry::Keys {

// Appearance
inline const QString Theme = QStringLiteral("appearance/theme");      // system | light | dark
inline const QString Accent = QStringLiteral("appearance/accent");    // #rrggbb
inline const QString FontSize = QStringLiteral("appearance/fontSize"); // points

// Language & region
inline const QString Language = QStringLiteral("region/language");        // ISO code, e.g. "en"
inline const QString Region = QStringLiteral("region/locale");            // locale name, "" = system
inline const QString FirstDayOfWeek = QStringLiteral("region/firstDay");  // Qt::DayOfWeek (1 = Monday)

// Display
inline const QString Scale = QStringLiteral("display/scale");                    // percent
inline const QString Brightness = QStringLiteral("display/brightness");          // percent
inline const QString NightLight = QStringLiteral("display/nightLight");          // bool
inline const QString NightLightStrength = QStringLiteral("display/nightLightStrength"); // percent

// Sound
inline const QString Volume = QStringLiteral("sound/volume");          // percent
inline const QString Muted = QStringLiteral("sound/muted");            // bool
inline const QString SystemSounds = QStringLiteral("sound/systemSounds"); // bool

// Network
inline const QString Hostname = QStringLiteral("network/hostname");

// Date & time
inline const QString AutoTime = QStringLiteral("datetime/auto");         // bool
inline const QString TimeZone = QStringLiteral("datetime/timezone");     // IANA id, "" = system
inline const QString Clock24 = QStringLiteral("datetime/clock24");       // bool

// Window state (not part of the resettable defaults)
inline const QString WindowGeometry = QStringLiteral("window/geometry");

// Per-application state (no built-in defaults needed)
inline const QString FmGeometry = QStringLiteral("filemanager/geometry");
inline const QString FmViewMode = QStringLiteral("filemanager/viewMode");     // 0 = details, 1 = icons
inline const QString FmShowHidden = QStringLiteral("filemanager/showHidden"); // bool
inline const QString ViewerGeometry = QStringLiteral("viewer/geometry");
inline const QString TaskGeometry = QStringLiteral("taskmanager/geometry");
inline const QString TaskSortColumn = QStringLiteral("taskmanager/sortColumn");
inline const QString TaskSortOrder = QStringLiteral("taskmanager/sortOrder");   // 0 = ascending, 1 = descending
inline const QString TaskInterval = QStringLiteral("taskmanager/intervalMs");   // 0 = paused
inline const QString TaskOnlyMine = QStringLiteral("taskmanager/onlyMine");     // bool
inline const QString CalendarGeometry = QStringLiteral("calendar/geometry");
inline const QString NotesGeometry = QStringLiteral("notes/geometry");
inline const QString ClockGeometry = QStringLiteral("clock/geometry");
inline const QString WeatherGeometry = QStringLiteral("weather/geometry");
inline const QString WeatherCity = QStringLiteral("weather/city");
inline const QString WeatherLatitude = QStringLiteral("weather/latitude");
inline const QString WeatherLongitude = QStringLiteral("weather/longitude");
inline const QString WeatherUnits = QStringLiteral("weather/units");           // metric | imperial

} // namespace colibry::Keys
