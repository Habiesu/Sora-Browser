#include <QApplication>
#include <QWebEngineProfile>

#include "app/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));

    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);

    sora::MainWindow window(profile);
    window.show();

    const QStringList args = QApplication::arguments();
    window.navigate(args.size() > 1 ? args.last() : QStringLiteral("about:blank"));

    return app.exec();
}
