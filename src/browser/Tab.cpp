#include "browser/Tab.h"

#include "privacy/RequestInterceptor.h"

namespace sora {

Tab::Tab(QWebEngineProfile* profile, const AdblockService* adblock, QWidget* parent)
    : QWebEngineView(parent)
{
    auto* page = new QWebEnginePage(profile, this);
    auto* interceptor = new RequestInterceptor(adblock, page);
    page->setUrlRequestInterceptor(interceptor);
    setPage(page);
    connect(interceptor, &RequestInterceptor::blocked, this, [this] { countBlocked(); }, Qt::QueuedConnection);
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
