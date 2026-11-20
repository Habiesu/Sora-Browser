#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QTimer>
#include <QWebEngineView>
#include <cstdio>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QStatusBar>
#include <QWebEngineProfile>

#include "app/MainWindow.h"
#include "privacy/AdblockService.h"
#include "privacy/FilterListStore.h"
#include "privacy/RequestInterceptor.h"

int main(int argc, char* argv[])
{
    QElapsedTimer sinceStart; // --bench mide desde aquí
    sinceStart.start();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));

    QCommandLineParser cli;
    cli.addHelpOption();
    const QCommandLineOption benchOption(QStringLiteral("bench"),
        QStringLiteral("Mide el arranque, imprime una línea JSON al cargar la primera página y sale."));
    cli.addOption(benchOption);
    cli.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Dirección o búsqueda a abrir."));
    cli.process(app);
    const bool bench = cli.isSet(benchOption);

    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);

    auto* adblock = new sora::AdblockService(&app);

    sora::MainWindow window(profile);
    auto* interceptor = new sora::RequestInterceptor(adblock, window.view()->page());
    window.view()->page()->setUrlRequestInterceptor(interceptor);
    QObject::connect(interceptor, &sora::RequestInterceptor::blocked, &window,
                     [&window] { window.countBlocked(); }, Qt::QueuedConnection);
    window.show();

    qint64 windowShownMs = -1;
    if (bench) {
        // El primer ciclo del bucle de eventos tras show() es cuando la ventana ya está en pantalla.
        QTimer::singleShot(0, &window, [&] { windowShownMs = sinceStart.elapsed(); });
        QObject::connect(window.view(), &QWebEngineView::loadFinished, &app, [&](bool) {
            std::printf("{\"window_shown_ms\":%lld,\"first_load_ms\":%lld}\n",
                        static_cast<long long>(windowShownMs), static_cast<long long>(sinceStart.elapsed()));
            std::fflush(stdout);
            QApplication::quit();
        }, Qt::SingleShotConnection);
    }

    const QStringList positional = cli.positionalArguments();
    window.navigate(positional.isEmpty() ? QStringLiteral("about:blank") : positional.first());

    // El motor se prepara después de mostrar la ventana; hasta entonces no se bloquea nada.
    auto* nam = new QNetworkAccessManager(&app);
    auto* filters = new sora::FilterListStore(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/adblock"),
        {QUrl(QStringLiteral("https://easylist.to/easylist/easylist.txt")),
         QUrl(QStringLiteral("https://easylist.to/easylist/easyprivacy.txt")),
         QUrl(QStringLiteral("https://ublockorigin.github.io/uAssets/filters/filters.txt")),
         QUrl(QStringLiteral("https://ublockorigin.github.io/uAssets/filters/badware.txt"))},
        nam, &app);
    QObject::connect(filters, &sora::FilterListStore::engineReady, adblock,
                     [adblock](std::shared_ptr<const sora::AdblockEngine> engine) { adblock->setEngine(std::move(engine)); });
    QObject::connect(filters, &sora::FilterListStore::failed, &window,
                     [&window](const QString& reason) { window.statusBar()->showMessage(reason); });
    filters->start();

    return app.exec();
}
