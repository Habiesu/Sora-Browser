#include "browser/Tab.h"

#include <QAction>
#include <QWebEngineSettings>

namespace sora {

namespace {
// Qt 6.12 no traduce las acciones del menú contextual de la web aunque haya traductor instalado.
void translateActions(QWebEnginePage* page)
{
    using A = QWebEnginePage;
    const std::pair<A::WebAction, const char*> texts[] = {
        {A::Back, "Atrás"}, {A::Forward, "Adelante"}, {A::Reload, "Volver a cargar"}, {A::Stop, "Detener"},
        {A::Cut, "Cortar"}, {A::Copy, "Copiar"}, {A::Paste, "Pegar"},
        {A::PasteAndMatchStyle, "Pegar con el mismo estilo"}, {A::Undo, "Deshacer"}, {A::Redo, "Rehacer"},
        {A::SelectAll, "Seleccionar todo"}, {A::OpenLinkInThisWindow, "Abrir enlace aquí"},
        {A::OpenLinkInNewWindow, "Abrir enlace en una ventana nueva"},
        {A::OpenLinkInNewTab, "Abrir enlace en una pestaña nueva"},
        {A::OpenLinkInNewBackgroundTab, "Abrir enlace en segundo plano"},
        {A::CopyLinkToClipboard, "Copiar dirección del enlace"}, {A::CopyImageToClipboard, "Copiar imagen"},
        {A::CopyImageUrlToClipboard, "Copiar dirección de la imagen"},
        {A::CopyMediaUrlToClipboard, "Copiar dirección del vídeo o audio"},
        {A::ToggleMediaControls, "Mostrar controles"}, {A::ToggleMediaLoop, "Repetir"},
        {A::ToggleMediaPlayPause, "Reproducir o pausar"}, {A::ToggleMediaMute, "Silenciar"},
        {A::ViewSource, "Ver código fuente"}, {A::ExitFullScreen, "Salir de pantalla completa"}};
    for (const auto& [action, text] : texts)
        page->action(action)->setText(QString::fromUtf8(text));
    // Descargas y herramientas de desarrollo aún no existen (Hito 6): estas opciones no harían nada.
    for (auto action : {A::SavePage, A::DownloadLinkToDisk, A::DownloadImageToDisk, A::DownloadMediaToDisk,
                        A::InspectElement})
        page->action(action)->setVisible(false);
}
} // namespace

bool Page::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame)
{
    if (isMainFrame) {
        // shortcut: ANGLE de Qt 6.12 se cae (y con él Sora, la GPU va en el mismo proceso) con el
        // WebGL de la portada de github.com; ahí la web usa una imagen fija. Quitar al actualizar
        // Qt WebEngine si https://github.com deja de cerrar Sora con WebGL activado.
        const bool crashesAngle = url.host() == QLatin1String("github.com");
        settings()->setAttribute(QWebEngineSettings::WebGLEnabled, !crashesAngle);
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

Tab::Tab(QWebEngineProfile* profile, QWidget* parent)
    : QWebEngineView(parent)
{
    setPage(new Page(profile, this));
    translateActions(page());
    connect(this, &QWebEngineView::loadFinished, this, [this] { m_loading = false; });
    connect(this, &QWebEngineView::loadStarted, this, [this] {
        m_loading = true;
        m_blocked = 0;
        emit blockedCountChanged(0);
    });
}

void Tab::countBlocked()
{
    emit blockedCountChanged(++m_blocked);
}

QWebEngineView* Tab::createWindow(QWebEnginePage::WebWindowType type)
{
    return onCreateWindow ? onCreateWindow(type) : nullptr;
}

} // namespace sora
