#include "browser/Tab.h"

namespace sora {

Tab::Tab(QWebEngineProfile* profile, QWidget* parent)
    : QWebEngineView(parent)
{
    setPage(new QWebEnginePage(profile, this));
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
