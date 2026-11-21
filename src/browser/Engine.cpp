#include "browser/Engine.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <atomic>
#include <memory>
#include <mutex>

#include "include/cef_app.h"
#include "include/cef_task.h"
#include "include/cef_version.h"

#include "app/Settings.h"
#include "browser/Extensions.h"
#include "browser/Profile.h"
#include <QFile>
#include "engine/EngineApp.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace sora::engine {
namespace {

std::atomic<int> s_browsers{0};
std::atomic<std::shared_ptr<const QSet<int>>> s_shortcuts;
CefRefPtr<CefMessageRouterBrowserSide> s_router;
bool s_started = false;

class Task : public CefTask {
public:
    explicit Task(std::function<void()> f) : m_f(std::move(f)) {}
    void Execute() override { m_f(); }

private:
    std::function<void()> m_f;
    IMPLEMENT_REFCOUNTING(Task);
};

std::string native(const QString& path) { return QDir::toNativeSeparators(path).toStdString(); }

} // namespace

bool start()
{
    CefRefPtr<EngineApp> app = new EngineApp;
    const Settings& s = Settings::instance();
    // shortcut: la reproducción automática se fija al arrancar (cambiarla pide reiniciar). "sites" usa
    // la regla de Chrome (deja en los sitios de vídeo que usas); la lista de Ajustes no se aplica aún.
    const QString autoplay = s.value(QStringLiteral("autoplay")).toString();
    if (autoplay == QLatin1String("always"))
        app->switches.emplace_back("autoplay-policy", "no-user-gesture-required");
    else if (autoplay == QLatin1String("never"))
        app->switches.emplace_back("autoplay-policy", "document-user-activation-required");
    const QStringList extensions = Extensions::enabledPaths();
    if (!extensions.isEmpty())
        app->switches.emplace_back("load-extension", native(extensions.join(QLatin1Char(','))));

    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/cef");
    QDir().mkpath(root + QStringLiteral("/Default"));
    if (QFile::exists(clearCacheMarker())) { // "Borrar datos > caché" pedido en la sesión anterior
        for (const char* dir : {"Cache", "Code Cache", "GPUCache"})
            QDir(root + QStringLiteral("/Default/") + QLatin1String(dir)).removeRecursively();
        QFile::remove(clearCacheMarker());
    }
    CefSettings settings;
    settings.no_sandbox = true; // shortcut: sin sandbox hasta enlazar cef_sandbox.lib (plan CEF, tarea 2)
    settings.multi_threaded_message_loop = true;
    settings.persist_session_cookies = true;
    settings.log_severity = LOGSEVERITY_DISABLE; // sin debug.log en la carpeta de instalación
    CefString(&settings.browser_subprocess_path) =
        native(QCoreApplication::applicationDirPath() + QStringLiteral("/sora-helper.exe"));
    CefString(&settings.root_cache_path) = native(root);
    CefString(&settings.cache_path) = native(root + QStringLiteral("/Default"));
    CefString(&settings.locale) = "es";
#ifdef Q_OS_WIN
    CefMainArgs args(GetModuleHandle(nullptr));
#else
    CefMainArgs args(0, nullptr);
#endif
    if (!CefInitialize(args, settings, app, nullptr))
        return false;
    s_started = true;
    s_router = CefMessageRouterBrowserSide::Create(messageRouterConfig());
    return true;
}

void shutdown()
{
    if (!s_started)
        return;
    // Las pestañas ya pidieron cerrar sus navegadores; CEF exige que acaben antes de apagarse.
    const QDeadlineTimer deadline(3000);
    while (s_browsers.load() > 0 && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    s_router = nullptr;
    CefShutdown();
    s_started = false;
}

QString chromiumVersion()
{
    return QStringLiteral("%1.%2.%3.%4").arg(CHROME_VERSION_MAJOR).arg(CHROME_VERSION_MINOR).arg(CHROME_VERSION_BUILD).arg(CHROME_VERSION_PATCH);
}

void onUi(std::function<void()> task)
{
    CefPostTask(TID_UI, new Task(std::move(task)));
}

void onQt(std::function<void()> task)
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(task), Qt::QueuedConnection);
}

CefRefPtr<CefMessageRouterBrowserSide> router() { return s_router; }
void browserCreated() { ++s_browsers; }
void browserClosed() { --s_browsers; }

void setShortcuts(const QSet<int>& keys)
{
    s_shortcuts.store(std::make_shared<const QSet<int>>(keys));
}

bool isShortcut(int keyCombination)
{
    const auto keys = s_shortcuts.load();
    return keys && keys->contains(keyCombination);
}

} // namespace sora::engine
