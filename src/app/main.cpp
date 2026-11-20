#include <QApplication>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QStatusBar>
#include <QWebEngineProfile>

#include "app/MainWindow.h"
#include "privacy/FilterListStore.h"
#include "privacy/RequestInterceptor.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));

    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);

    auto* interceptor = new sora::RequestInterceptor(&app);
    profile->setUrlRequestInterceptor(interceptor);

    sora::MainWindow window(profile);
    QObject::connect(interceptor, &sora::RequestInterceptor::blocked, &window,
                     [&window] { window.countBlocked(); }, Qt::QueuedConnection);
    window.show();

    const QStringList args = QApplication::arguments();
    window.navigate(args.size() > 1 ? args.last() : QStringLiteral("about:blank"));

    // El motor se prepara después de mostrar la ventana; hasta entonces no se bloquea nada.
    auto* nam = new QNetworkAccessManager(&app);
    auto* filters = new sora::FilterListStore(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/adblock"),
        {QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")),
         QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt")),
         QUrl(QStringLiteral("https://ublockorigin.github.io/uAssets/filters/filters.txt")),
         QUrl(QStringLiteral("https://ublockorigin.github.io/uAssets/filters/badware.txt"))},
        nam, &app);
    QObject::connect(filters, &sora::FilterListStore::engineReady, interceptor,
                     [interceptor](std::shared_ptr<const sora::AdblockEngine> engine) { interceptor->setEngine(std::move(engine)); });
    QObject::connect(filters, &sora::FilterListStore::failed, &window,
                     [&window](const QString& reason) { window.statusBar()->showMessage(reason); });
    filters->start();

    return app.exec();
}
