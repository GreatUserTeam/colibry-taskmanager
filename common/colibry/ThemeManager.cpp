#include "ThemeManager.h"

#include "Keys.h"
#include "SettingsStore.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleHints>

namespace colibry {

QPalette ThemeManager::makePalette(bool dark)
{
    const QColor window = dark ? QColor(32, 33, 36) : QColor(243, 244, 246);
    const QColor base = dark ? QColor(24, 25, 28) : QColor(255, 255, 255);
    const QColor altBase = dark ? QColor(36, 38, 42) : QColor(236, 238, 241);
    const QColor text = dark ? QColor(230, 232, 236) : QColor(28, 30, 34);
    const QColor button = dark ? QColor(45, 47, 52) : QColor(233, 235, 238);
    const QColor disabled = dark ? QColor(120, 123, 130) : QColor(150, 153, 160);

    // Building from (button, window) derives Light, Midlight, Mid, Dark and Shadow from them,
    // so frames, hover colors and picture backgrounds follow the theme as well.
    QPalette p(button, window);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, altBase);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::BrightText, QColor(255, 80, 80));
    p.setColor(QPalette::Link, dark ? QColor(120, 170, 255) : QColor(30, 90, 200));

    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    return p;
}

bool ThemeManager::isDark(const SettingsStore &store)
{
    const QString theme = store.value(Keys::Theme).toString();
    if (theme == QStringLiteral("dark"))
        return true;
    if (theme == QStringLiteral("light"))
        return false;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

void ThemeManager::apply(QApplication &app, const SettingsStore &store)
{
    if (app.style()->objectName().compare(QStringLiteral("fusion"), Qt::CaseInsensitive) != 0)
        app.setStyle(QStringLiteral("Fusion"));

    QPalette palette = makePalette(isDark(store));

    QColor accent(store.value(Keys::Accent).toString());
    if (!accent.isValid())
        accent = QColor(QStringLiteral("#3b82f6"));
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText,
                     accent.lightness() > 150 ? QColor(Qt::black) : QColor(Qt::white));
    app.setPalette(palette);

    QFont font = app.font();
    font.setPointSize(qBound(8, store.value(Keys::FontSize).toInt(), 24));
    app.setFont(font);
}

} // namespace colibry
