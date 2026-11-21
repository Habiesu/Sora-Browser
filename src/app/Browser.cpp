#include "app/Browser.h"

#include <QApplication>
#include <QEvent>
#include <QWebEngineProfile>

#include "app/InternalPages.h"
#include "app/MainWindow.h"
#include "browser/Tab.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"
#include "browser/TabManager.h"
#include "privacy/AdblockService.h"
#include "ui/TabStrip.h"

namespace sora {

Browser::Browser(QWebEngineProfile* profile, AdblockService* adblock, QObject* parent)
    : QObject(parent)
    , m_profile(profile)
    , m_adblock(adblock)
{
    connect(qApp, &QApplication::focusWindowChanged, this, [this] {
        for (const auto& w : std::as_const(m_windows))
            if (w && w->isActiveWindow())
                m_active = w;
    });
}

MainWindow* Browser::newWindow(bool incognito)
{
    MainWindow* w = createWindow(incognito, true);
    w->show();
    return w;
}

MainWindow* Browser::createWindow(bool incognito, bool startWithTab)
{
    QWebEngineProfile* profile = m_profile;
    if (incognito) {
        if (!m_incognito) {
            m_incognito = new QWebEngineProfile(this); // sin nombre: no escribe nada en disco
            if (m_adblock)
                m_adblock->attach(m_incognito);
            installInternalPages(m_incognito);
        }
        profile = m_incognito;
    }
    auto* w = new MainWindow(profile, m_adblock, startWithTab);
    if (m_history)
        w->setStores(m_history.get(), m_bookmarks);
    w->setAttribute(Qt::WA_DeleteOnClose);
    m_windows.append(w);
    m_active = w;
    connect(w, &QObject::destroyed, this, [this] { m_windows.removeAll(nullptr); });
    connect(w, &MainWindow::newWindowRequested, this, [this](bool incognitoWindow) { newWindow(incognitoWindow); });
    connect(w, &MainWindow::tabDetachRequested, this,
            [this, w](int index, QPoint globalPos) { onTabDetachRequested(w, index, globalPos); });
    return w;
}

void Browser::openStores(const QString& databasePath)
{
    m_history = std::make_unique<HistoryStore>(databasePath);
    m_bookmarks = new BookmarkStore(databasePath, this);
    for (const auto& w : std::as_const(m_windows))
        if (w)
            w->setStores(m_history.get(), m_bookmarks);
}

Browser::~Browser() = default;

QList<MainWindow*> Browser::windows() const
{
    QList<MainWindow*> list;
    for (const auto& w : m_windows)
        if (w && w->isVisible())
            list.append(w);
    return list;
}

MainWindow* Browser::activeWindow() const
{
    if (m_active && m_active->isVisible())
        return m_active;
    const auto list = windows();
    return list.isEmpty() ? nullptr : list.last();
}

MainWindow* Browser::detachTab(MainWindow* from, int index, QPoint globalPos)
{
    if (from->tabs()->count() < 2)
        return nullptr; // con una sola pestaña, arrastrarla es mover la ventana
    MainWindow* w = createWindow(from->isIncognito(), false);
    w->resize(from->size());
    w->move(globalPos - QPoint(120, 20)); // la pestaña queda bajo el ratón, como en Chrome
    w->adoptTab(from->takeTab(index));
    w->show();
    w->activateWindow();
    return w;
}

bool Browser::moveTab(MainWindow* from, int index, MainWindow* to, int toIndex)
{
    if (from == to || from->isIncognito() != to->isIncognito() || !from->tabs()->tab(index))
        return false; // nunca se mezclan pestañas normales e incógnito
    to->adoptTab(from->takeTab(index), toIndex); // si era la última, `from` se cierra sola
    to->activateWindow();
    return true;
}

void Browser::onTabDetachRequested(MainWindow* from, int index, QPoint globalPos)
{
    // ¿Se soltó sobre la tira de pestañas de otra ventana? Allí; si no, ventana nueva.
    if (QWidget* under = QApplication::widgetAt(globalPos)) {
        for (const auto& w : std::as_const(m_windows)) {
            if (!w || w == from || under->window() != w)
                continue;
            auto* strip = w->findChild<TabStrip*>();
            if (strip && strip->rect().contains(strip->mapFromGlobal(globalPos))) {
                const int at = strip->tabAt(strip->mapFromGlobal(globalPos));
                moveTab(from, index, w, at < 0 ? w->tabs()->count() : at);
                return;
            }
        }
    }
    detachTab(from, index, globalPos);
}

} // namespace sora
