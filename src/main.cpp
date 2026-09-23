#include "PreviewWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QStringList>
#include <QMimeDatabase>
#include <QScreen>
#include <QTextStream>
#include <QFileInfo>
#include <QIcon>
#include <QCursor>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("quicklook-linux"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QuickLook Linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("QuickLookLinux"));
    QApplication::setQuitOnLastWindowClosed(true);

    const QStringList args = QCoreApplication::arguments();

    if (args.contains(QStringLiteral("--print-available-geometry"))) {
        // Prefer the screen where the user is acting right now. This keeps
        // KWin placement correct with multiple monitors and after display changes.
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (!screen)
            return 40;

        const QRect a = screen->availableGeometry();
        QTextStream out(stdout);
        out << a.x() << ' '
            << a.y() << ' '
            << a.width() << ' '
            << a.height() << '\n';
        return 0;
    }

    QString path;
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith("--")) {
            path = args.at(i);
            break;
        }
    }

    bool startupAudio = false;
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        QMimeDatabase db;
        const QString mime =
            db.mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
        startupAudio = mime.startsWith(QStringLiteral("audio/"));
    }

    // KWin/Wayland uses this as the application identity for initial rules.
    QGuiApplication::setDesktopFileName(
        startupAudio
            ? QStringLiteral("quicklook-linux-audio")
            : QStringLiteral("quicklook-linux")
    );

    // Same icon-publication chain as Spotlight: stable desktop identity +
    // application-wide embedded icon. KWin/Plasma can resolve either path.
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/quicklook/assets/quicklook-linux-512.png"))
    );

    PreviewWindow window(path);
    window.show();
    window.raise();
    window.activateWindow();
    return app.exec();
}
