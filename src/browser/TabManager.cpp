#include "browser/TabManager.h"

#include <QStackedWidget>

#include "browser/Tab.h"

namespace sora {

TabManager::TabManager(QWebEngineProfile* profile, const AdblockService* adblock, QStackedWidget* stack,
                       QObject* parent)
    : QObject(parent)
    , m_profile(profile)
    , m_adblock(adblock)
    , m_stack(stack)
{
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int index) {
        if (!m_quiet)
            emit currentChanged(index);
    });
}

Tab* TabManager::newTab(const QUrl& url, int index, bool activate)
{
    auto* tab = new Tab(m_profile, m_adblock);
    tab->onCreateWindow = [this, tab](QWebEnginePage::WebWindowType type) {
        return newTab({}, indexOf(tab) + 1, type != QWebEnginePage::WebBrowserBackgroundTab);
    };
    connect(tab, &QWebEngineView::titleChanged, this,
            [this, tab](const QString& title) { emit tabTitleChanged(indexOf(tab), title); });
    connect(tab, &QWebEngineView::iconChanged, this,
            [this, tab](const QIcon& icon) { emit tabIconChanged(indexOf(tab), icon); });

    if (index < 0 || index > count())
        index = count();
    m_quiet = true; // insertar el primero lo hace actual; se anuncia después de tabInserted
    m_stack->insertWidget(index, tab);
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
    m_stack->removeWidget(t);
    m_quiet = false;
    t->deleteLater();
    emit tabRemoved(index);
    if (count() == 0) {
        emit lastTabClosed();
        return;
    }
    if (wasCurrent)
        m_stack->setCurrentIndex(std::min(index, count() - 1));
    emit currentChanged(currentIndex());
}

void TabManager::moveTab(int from, int to)
{
    QWidget* w = m_stack->widget(from);
    if (!w || to < 0 || to >= count() || from == to)
        return;
    const bool wasCurrent = from == currentIndex();
    QWidget* current = m_stack->currentWidget();
    m_quiet = true;
    m_stack->removeWidget(w);
    m_stack->insertWidget(to, w);
    m_stack->setCurrentWidget(wasCurrent ? w : current);
    m_quiet = false;
}

void TabManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= count())
        return;
    if (index == currentIndex()) {
        tab(index)->setFocus();
        return;
    }
    m_stack->setCurrentIndex(index);
    tab(index)->setFocus();
}

int TabManager::currentIndex() const { return m_stack->currentIndex(); }
int TabManager::count() const { return m_stack->count(); }
Tab* TabManager::tab(int index) const { return qobject_cast<Tab*>(m_stack->widget(index)); }
Tab* TabManager::currentTab() const { return tab(currentIndex()); }
int TabManager::indexOf(const Tab* tab) const { return m_stack->indexOf(const_cast<Tab*>(tab)); }

} // namespace sora
