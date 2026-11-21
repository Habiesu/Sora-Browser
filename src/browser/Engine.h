#pragma once
// Arranque y apagado de CEF, y el paso de tareas entre su hilo y el de Qt.
//
// CEF va con multi_threaded_message_loop: tiene su propio hilo UI y Qt sigue en el principal.
// Todo lo que llega de CEF a la interfaz pasa por onQt(); lo que va de Qt a CEF y debe correr en su
// hilo, por onUi(). Ninguno de los dos hilos espera nunca al otro (sería un cuelgue).
#include <QSet>
#include <QString>
#include <functional>

#include "include/cef_base.h"
#include "include/wrapper/cef_message_router.h"

namespace sora::engine {

// Antes de crear ventanas. false si CEF no arranca.
bool start();
// Espera (unos segundos como mucho) a que se cierren los navegadores y apaga CEF.
void shutdown();
QString chromiumVersion();

void onUi(std::function<void()> task);
void onQt(std::function<void()> task);

// Puente de las páginas sora:// (window.cefQuery). Lo usan los clientes de las pestañas.
CefRefPtr<CefMessageRouterBrowserSide> router();

// Navegadores vivos (shutdown espera a que lleguen a 0).
void browserCreated();
void browserClosed();

// Atajos de la ventana (Qt::Key | modificadores) que se atienden aunque la web tenga el foco.
void setShortcuts(const QSet<int>& keys);
bool isShortcut(int keyCombination);

} // namespace sora::engine
