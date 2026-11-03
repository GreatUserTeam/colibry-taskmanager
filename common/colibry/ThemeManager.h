#pragma once

#include <QPalette>

class QApplication;

namespace colibry {

class SettingsStore;

/// Applies theme, accent color and font size to the whole application.
class ThemeManager
{
public:
    /// Applies the current values from the store to the application.
    static void apply(QApplication &app, const SettingsStore &store);

    /// True when "system" (or "dark") currently resolves to a dark scheme.
    static bool isDark(const SettingsStore &store);

    /// Deterministic light or dark palette (without accent).
    static QPalette makePalette(bool dark);
};

} // namespace colibry
