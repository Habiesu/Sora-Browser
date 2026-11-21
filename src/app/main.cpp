#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QTimer>
#include <QWebEngineView>
#include <cstdio>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QWebEngineProfile>

#include "app/MainWindow.h"
#include "browser/Tab.h"
#include "privacy/AdblockService.h"
#include "privacy/FilterListStore.h"
#include "app/SingleInstance.h"
#include "browser/TabManager.h"

#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

int main(int argc, char* argv[])
{
    QElapsedTimer sinceStart; // --bench mide desde aquí
    sinceStart.start();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));

    // Menús de Qt y de la web (clic derecho) en el idioma del sistema.
    // Instalado: junto a sora.exe (QLibraryInfo apunta a la carpeta de arriba). Compilado: la de Qt.
    const QStringList translationDirs = {QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
                                         QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
    for (const char* catalog : {"qtbase", "qtwebengine"}) {
        auto* translator = new QTranslator(&app);
        for (const QString& dir : translationDirs) {
            if (translator->load(QLocale(), QLatin1String(catalog), QStringLiteral("_"), dir)) {
                QCoreApplication::installTranslator(translator);
                break;
            }
        }
    }

    QCommandLineParser cli;
    cli.addHelpOption();
    const QCommandLineOption benchOption(QStringLiteral("bench"),
        QStringLiteral("Mide el arranque, imprime una línea JSON al cargar la primera página y sale."));
    cli.addOption(benchOption);
    cli.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Dirección o búsqueda a abrir."));
    cli.process(app);
    const bool bench = cli.isSet(benchOption);
    const QStringList positional = cli.positionalArguments();

    // Si ya hay un Sora abierto, que abra él la dirección (mismo perfil) y este sale enseguida.
    // --bench siempre mide un arranque propio.
    sora::SingleInstance instance(QStringLiteral("sora-") + QString::number(qHash(QDir::homePath()), 16));
    if (!bench) {
#ifdef Q_OS_WIN
        AllowSetForegroundWindow(ASFW_ANY); // deja que el Sora abierto se ponga delante
#endif
        if (instance.sendToRunning(positional.value(0)))
            return 0;
        instance.listen();
    }

    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);

    auto* adblock = new sora::AdblockService(&app);
    adblock->attach(profile);

    sora::MainWindow window(profile, adblock);
    window.show();
    QObject::connect(&instance, &sora::SingleInstance::received, &window, [&window](const QString& input) {
        if (input.isEmpty())
            window.newTab();
        else
            window.tabs()->newTab(sora::urlFromInput(input));
        if (window.isMinimized())
            window.showNormal();
        window.raise();
        window.activateWindow();
    });

    qint64 windowShownMs = -1;
    if (bench) {
        // El primer ciclo del bucle de eventos tras show() es cuando la ventana ya está en pantalla.
        QTimer::singleShot(0, &window, [&] { windowShownMs = sinceStart.elapsed(); });
        QObject::connect(window.currentTab(), &QWebEngineView::loadFinished, &app, [&](bool) {
            std::printf("{\"window_shown_ms\":%lld,\"first_load_ms\":%lld}\n",
                        static_cast<long long>(windowShownMs), static_cast<long long>(sinceStart.elapsed()));
            std::fflush(stdout);
            QApplication::quit();
        }, Qt::SingleShotConnection);
    }

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
                     [&window](const QString& reason) { window.showNotice(reason); });
    filters->start();

    return app.exec();
}
