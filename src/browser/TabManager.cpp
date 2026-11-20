#include "browser/TabManager.h"

#include <QStackedWidget>

#include "browser/Tab.h"
#include "privacy/AdblockService.h"

namespace sora {

TabManager::TabManager(QWebEngineProfile* profile, const AdblockService* adblock, QStackedWidget* stack,
                       QObject* parent)
    : QObject(parent)
    , m_profile(profile)
    , m_adblock(adblock)
    , m_stack(stack)
{
    if (m_adblock)
        connect(m_adblock, &AdblockService::blocked, this,
                [this](const QUrl&, const QUrl& firstParty) { countBlocked(firstParty); });
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int) {
        if (!m_quiet)
            emit currentChanged(currentIndex());
    });
}

Tab* TabManager::newTab(const QUrl& url, int index, bool activate)
{
    auto* tab = new Tab(m_profile);
    tab->onCreateWindow = [this, tab](QWebEnginePage::WebWindowType type) {
        return newTab({}, indexOf(tab) + 1, type != QWebEnginePage::WebBrowserBackgroundTab);
    };
    connect(tab, &QWebEngineView::titleChanged, this,
            [this, tab](const QString& title) { emit tabTitleChanged(indexOf(tab), title); });
    connect(tab->page(), &QWebEnginePage::windowCloseRequested, this, [this, tab] { closeTab(indexOf(tab)); });
    connect(tab, &QWebEngineView::iconChanged, this,
            [this, tab](const QIcon& icon) { emit tabIconChanged(indexOf(tab), icon); });

    if (index < 0 || index > count())
        index = count();
    m_quiet = true; // insertar el primero lo hace actual; se anuncia después de tabInserted
    m_order.insert(index, tab);
    m_stack->addWidget(tab);
    m_quiet = false;
    emit tabInserted(index);
    if (activate || count() == 1) {
        setCurrentIndex(index);
        if (count() == 1)
            emit currentChanged(0);
    }
    if (url.isValid())
        tab->setUrl(url);
    return tab;
}

void TabManager::closeTab(int index)
{
    Tab* t = tab(index);
    if (!t)
        return;
    const bool wasCurrent = index == currentIndex();
    m_quiet = true;
    m_order.removeAt(index);
    m_stack->removeWidget(t);
    t->deleteLater();
    if (count() && wasCurrent)
        m_stack->setCurrentWidget(m_order[std::min(index, count() - 1)]);
    m_quiet = false;
    emit tabRemoved(index);
    if (count() == 0) {
        emit lastTabClosed();
        return;
    }
    emit currentChanged(currentIndex());
}

void TabManager::moveTab(int from, int to)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to)
        return;
    m_order.move(from, to);
}

void TabManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= count())
        return;
    if (index == currentIndex()) {
        tab(index)->setFocus();
        return;
    }
    m_stack->setCurrentWidget(m_order[index]);
    tab(index)->setFocus();
}

int TabManager::currentIndex() const { return m_order.indexOf(qobject_cast<Tab*>(m_stack->currentWidget())); }
int TabManager::count() const { return m_order.size(); }
Tab* TabManager::tab(int index) const { return index >= 0 && index < count() ? m_order[index] : nullptr; }
Tab* TabManager::currentTab() const { return tab(currentIndex()); }
void TabManager::countBlocked(const QUrl& firstParty)
{
    // shortcut: se reparte por sitio (desde el perfil no se sabe la página); con el mismo sitio
    // en varias pestañas de fondo se lo lleva la primera.
    Tab* target = nullptr;
    for (int i = 0; i < count(); ++i) {
        if (tab(i)->url().host() != firstParty.host())
            continue;
        if (i == currentIndex()) {
            target = tab(i);
            break;
        }
        if (!target)
            target = tab(i);
    }
    if (target)
        target->countBlocked();
}

int TabManager::indexOf(const Tab* tab) const { return m_order.indexOf(const_cast<Tab*>(tab)); }

} // namespace sora
