#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#include <cstdio>

#include "app/Browser.h"
#include "app/InternalPages.h"
#include "app/MainWindow.h"
#include "app/Settings.h"
#include "app/SingleInstance.h"
#include "browser/Engine.h"
#include "browser/Profile.h"
#include "browser/Tab.h"
#include "browser/TabManager.h"
#include "privacy/AdblockService.h"
#include "privacy/FilterListStore.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
// En portátiles con dos gráficas, Windows y los controladores de NVIDIA/AMD leen estas exportaciones
// del .exe y ponen a Sora en la dedicada (como los juegos). La GPU de Chromium va en sora-helper.exe,
// que también las lleva. Sin gráfica dedicada no hacen nada.
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

    QApplication app(argc, argv);
    mark("QApplication");
    QApplication::setApplicationName(QStringLiteral("Sora"));
    QApplication::setOrganizationName(QStringLiteral("Sora"));
    QApplication::setApplicationVersion(QStringLiteral(SORA_VERSION));

    // Menús y diálogos de Qt en el idioma del sistema (los de la web los traduce CEF con locale=es).
    // Instalado: junto a sora.exe (QLibraryInfo apunta a la carpeta de arriba). Compilado: la de Qt.
    auto* translator = new QTranslator(&app);
    for (const QString& dir : {QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
                               QLibraryInfo::path(QLibraryInfo::TranslationsPath)})
        if (translator->load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"), dir)) {
            QCoreApplication::installTranslator(translator);
            break;
        }

    QCommandLineParser cli;
    cli.addHelpOption();
    const QCommandLineOption benchOption(QStringLiteral("bench"),
        QStringLiteral("Mide el arranque, imprime una línea JSON al cargar la primera página y sale."));
    cli.addOption(benchOption);
    cli.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Dirección o búsqueda a abrir."));
    cli.parse(app.arguments()); // parse y no process: CEF también recibe sus interruptores (--remote-debugging-port...)
    if (cli.isSet(QStringLiteral("help")))
        cli.showHelp();
    const bool bench = cli.isSet(benchOption);
    QStringList positional;
    for (const QString& arg : cli.positionalArguments())
        if (!arg.startsWith(QLatin1String("--")))
            positional.append(arg);

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

    if (!sora::engine::start()) {
        QMessageBox::critical(nullptr, QStringLiteral("Sora"), QStringLiteral("No se pudo arrancar el motor web (CEF)."));
        return 1;
    }
    mark("CEF");

    int code = 0;
    {
        auto* adblock = new sora::AdblockService(&app);
        auto* profile = new sora::Profile(false, adblock, &app);
        sora::installInternalPages(profile);
        mark("perfil + adblock + paginas internas");

        sora::Browser browser(profile, adblock);
        sora::MainWindow* window = browser.showFirstWindow();
        const qint64 windowShownMs = sinceStart.elapsed(); // la ventana ya está en pantalla
        mark("ventana en pantalla");
        // La primera pestaña nace ya con su dirección.
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
            QObject::connect(window->currentTab(), &sora::Tab::loadFinished, &app, [&](bool) {
                mark("primera pagina cargada");
                std::printf("{\"window_shown_ms\":%lld,\"first_load_ms\":%lld}\n",
                            static_cast<long long>(windowShownMs), static_cast<long long>(sinceStart.elapsed()));
                std::fflush(stdout);
                QApplication::closeAllWindows();
            }, Qt::SingleShotConnection);
        }

        // Historial y marcadores, justo después de mostrar la ventana (no frenan el arranque).
        QTimer::singleShot(0, &browser, [&browser, &mark] {
            mark("abrir historial y marcadores...");
            browser.openStores(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/sora.db"));
            mark("historial y marcadores abiertos");
        });

        // Las listas de bloqueo se preparan después de mostrar la ventana; hasta entonces no se bloquea nada.
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

        code = app.exec();
        // Las ventanas cerradas esperan su deleteLater: sus pestañas piden ahora cerrar los navegadores.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        delete profile; // suelta su contexto de CEF antes de apagarlo
    }
    sora::engine::shutdown();
    return code;
}
