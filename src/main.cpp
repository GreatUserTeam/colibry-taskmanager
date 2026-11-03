#include "TaskWindow.h"
#include "core/ProcFsProvider.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-tasks"));
    QApplication::setApplicationDisplayName(QStringLiteral("Task Manager"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-tasks"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Task manager for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    colibry::SettingsStore store;
    colibry::ThemeSync themeSync(app, store);

    // To run on a system without /proc, return another ProcessProvider here.
    colibry::tm::TaskWindow window(std::make_unique<colibry::tm::ProcFsProvider>(), store);
    window.show();
    return QApplication::exec();
}
