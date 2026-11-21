#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QTimer>
#include <QWebEngineView>
#include <cstdio>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QWebEngineProfile>

#include "app/Browser.h"
#include "app/InternalPages.h"
#include "app/MainWindow.h"
#include "app/Settings.h"
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

#ifdef Q_OS_WIN
// En portátiles con dos gráficas, Windows y los controladores de NVIDIA/AMD leen estas exportaciones
// del .exe y ponen a Sora en la dedicada (como los juegos). En la integrada, capturar la pantalla desde
// la dedicada (directos de Discord) hacía parpadear las páginas. Sin gráfica dedicada no hacen nada.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char* argv[])
{
    QElapsedTimer sinceStart; // --bench mide desde aquí
    sinceStart.start();
    // SORA_TRACE=1: imprime en stderr cuánto tarda cada fase del arranque (para optimizarlo).
    const bool trace = qEnvironmentVariableIsSet("SORA_TRACE");
    auto mark = [&](const char* what) { if (trace) std::fprintf(stderr, "%6lld ms  %s\n", (long long)sinceStart.elapsed(), what); };

    sora::registerSoraScheme(); // antes de la aplicación
    QApplication app(argc, argv);
    mark("QApplication");
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));
    QApplication::setApplicationVersion(QStringLiteral(SORA_VERSION));

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

    mark("traductores");
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

    mark("instancia unica");
    auto* profile = new QWebEngineProfile(QStringLiteral("default"), &app);
    mark("perfil");

    auto* adblock = new sora::AdblockService(&app);
    adblock->attach(profile);
    sora::installInternalPages(profile);

    mark("adblock + paginas internas");
    sora::Browser browser(profile, adblock);
    mark("Browser");
    sora::MainWindow* window = browser.showFirstWindow();
    const qint64 windowShownMs = sinceStart.elapsed(); // la ventana ya está en pantalla
    mark("ventana en pantalla");
    // La primera pestaña nace ya con su dirección (antes cargaba "Nueva pestaña" y luego navegaba).
    const sora::Settings& settings = sora::Settings::instance();
    const QString homepage = settings.value(QStringLiteral("homepage")).toString();
    if (!positional.isEmpty())
        window->tabs()->newTab(sora::urlFromInput(positional.first()));
    else if (settings.value(QStringLiteral("startup")) == QStringLiteral("homepage") && !homepage.isEmpty())
        window->tabs()->newTab(sora::urlFromInput(homepage));
    else
        window->newTab();
    mark("primera pestaña");
    QObject::connect(&instance, &sora::SingleInstance::received, &browser, [&browser](const QString& input) {
        sora::MainWindow* w = browser.activeWindow();
        if (!w)
            return;
        if (input.isEmpty())
            w->newTab();
        else
            w->tabs()->newTab(sora::urlFromInput(input));
        if (w->isMinimized())
            w->showNormal();
        w->raise();
        w->activateWindow();
    });

    if (bench) {
        QObject::connect(window->currentTab(), &QWebEngineView::loadFinished, &app, [&](bool) {
            mark("primera pagina cargada");
            std::printf("{\"window_shown_ms\":%lld,\"first_load_ms\":%lld}\n",
                        static_cast<long long>(windowShownMs), static_cast<long long>(sinceStart.elapsed()));
            std::fflush(stdout);
            QApplication::quit();
        }, Qt::SingleShotConnection);
    }


    // Historial y marcadores, justo después de mostrar la ventana (no frenan el arranque).
    QTimer::singleShot(0, &browser, [&browser, &mark] {
        mark("abrir historial y marcadores...");
        browser.openStores(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/sora.db"));
        mark("historial y marcadores abiertos");
    });

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
    QObject::connect(filters, &sora::FilterListStore::failed, &browser, [&browser](const QString& reason) {
        if (sora::MainWindow* w = browser.activeWindow())
            w->showNotice(reason);
    });
    filters->start();

    return app.exec();
}
