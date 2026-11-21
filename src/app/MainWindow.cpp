#include "app/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QWebEngineFindTextResult>
#include <QWebEngineFullScreenRequest>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#include <QWebEngineView>
#include <QLibraryInfo>
#include <QMenu>
#include <QScreen>
#include <QWindow>

#include "app/InternalApi.h"
#include "app/InternalPages.h"
#include "app/Settings.h"
#include "browser/Suggestions.h"
#include "browser/Tab.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"
#include "browser/TabManager.h"
#include "ui/BookmarksBar.h"
#include "ui/FindBar.h"
#include "ui/Omnibox.h"
#include "ui/TabStrip.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"
#include "ui/Toolbar.h"
#include "ui/WindowFrame.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

namespace sora {

QUrl urlFromInput(const QString& rawInput)
{
    const QString input = rawInput.trimmed();

    static const QRegularExpression scheme(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    static const QRegularExpression localhost(QStringLiteral("^localhost(:\\d+)?(/.*)?$"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression domain(
        QStringLiteral("^([a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,}(:\\d+)?(/\\S*)?$"));

    if (localhost.match(input).hasMatch())
        return QUrl(QStringLiteral("http://") + input);
    if (domain.match(input).hasMatch())
        return QUrl(QStringLiteral("https://") + input);
    // "localhost:8080" también encaja con el patrón de esquema, por eso se comprueba antes.
    if (!input.contains(QLatin1Char(' ')) && scheme.match(input).hasMatch())
        return QUrl(input);

    return Settings::instance().searchUrl(input);
}

namespace {
constexpr int kLinuxResizeMargin = 4; // sin marco nativo: franja para redimensionar
const QString kNewTabTitle = QObject::tr("Nueva pestaña");

// Una página sin título (o about:blank) no tiene nada que mostrar: cadena vacía.
QString pageTitle(const QString& title)
{
    return title == QLatin1String("about:blank") ? QString() : title;
}
QString windowTitleFor(const QString& title)
{
    return pageTitle(title).isEmpty() ? QStringLiteral("Sora") : title + QStringLiteral(" — Sora");
}
} // namespace

MainWindow::MainWindow(QWebEngineProfile* profile, const AdblockService* adblock, bool startWithTab, QWidget* parent)
    : QMainWindow(parent)
    , m_titleBar(new TitleBar(this))
    , m_toolbar(new Toolbar(this))
    , m_bookmarks(new BookmarksBar(this))
    , m_stack(new QStackedWidget(this))
    , m_tabs(new TabManager(profile, adblock, m_stack, this))
    , m_linkPreview(new QLabel(m_stack))
    , m_findBar(new FindBar(m_stack))
    , m_incognito(profile->isOffTheRecord())
{
#ifdef Q_OS_WIN
    // Marco nativo de Windows 11 (sombra, esquinas, Snap) con la barra de título dibujada por Sora.
    setWindowFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint | Qt::CustomizeWindowHint);
#else
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
#endif

    auto* central = new QWidget(this);
    // Con ExpandedClientAreaHint, Qt reserva arriba un "área segura" para la barra nativa; la
    // barra de Sora va justo ahí, así que no se respeta.
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    central->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    auto* layout = new QVBoxLayout(central);
#ifndef Q_OS_WIN
    layout->setContentsMargins(kLinuxResizeMargin, kLinuxResizeMargin, kLinuxResizeMargin, kLinuxResizeMargin);
#else
    layout->setContentsMargins(0, 0, 0, 0);
#endif
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addWidget(m_toolbar);
    layout->addWidget(m_bookmarks);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    m_linkPreview->setObjectName(QStringLiteral("LinkPreview"));
    m_linkPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_linkPreview->hide();
    connect(m_findBar, &FindBar::findRequested, this, [this](const QString& text, bool backward) {
        if (Tab* t = currentTab())
            t->findText(text, backward ? QWebEnginePage::FindBackward : QWebEnginePage::FindFlags(),
                        [bar = QPointer<FindBar>(m_findBar)](const QWebEngineFindTextResult& r) {
                            if (bar)
                                bar->setResult(r.activeMatch(), r.numberOfMatches());
                        });
    });
    connect(m_findBar, &FindBar::closed, this, [this] {
        if (Tab* t = currentTab()) {
            t->findText(QString()); // quita el resaltado
            t->setFocus();
        }
    });

    TabStrip* strip = m_titleBar->tabs();
    connect(m_tabs, &TabManager::tabInserted, this, [this, strip](int index) {
        Tab* tab = m_tabs->tab(index);
        const QString title = pageTitle(tab->title()); // una pestaña que llega de otra ventana ya lo tiene
        m_syncing = true;
        strip->insertTab(index, title.isEmpty() ? kNewTabTitle : title);
        strip->setPageIcon(index, tab->icon());
        m_syncing = false;
        watchTab(tab);
    });
    connect(m_tabs, &TabManager::tabRemoved, this, [this, strip](int index) {
        m_syncing = true;
        strip->removeTab(index);
        m_syncing = false;
    });
    connect(m_tabs, &TabManager::currentChanged, this, [this, strip](int index) {
        m_syncing = true;
        strip->setCurrentIndex(index);
        m_syncing = false;
        showCurrentTab();
    });
    connect(m_tabs, &TabManager::tabTitleChanged, this, [this, strip](int index, const QString& title) {
        const QString shown = pageTitle(title);
        strip->setTabText(index, shown.isEmpty() ? kNewTabTitle : shown);
        strip->setTabToolTip(index, shown);
        if (index == m_tabs->currentIndex())
            setWindowTitle(titleFor(title));
    });
    connect(m_tabs, &TabManager::tabIconChanged, strip, &TabStrip::setPageIcon);
    connect(m_tabs, &TabManager::lastTabClosed, this, &QWidget::close);

    connect(strip, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_syncing)
            m_tabs->setCurrentIndex(index);
    });
    connect(strip, &QTabBar::tabCloseRequested, m_tabs, &TabManager::closeTab);
    connect(strip, &QTabBar::tabMoved, m_tabs, &TabManager::moveTab);
    connect(strip, &TabStrip::newTabRequested, this, &MainWindow::newTab);
    connect(strip, &TabStrip::tabDetachRequested, this, &MainWindow::tabDetachRequested);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &MainWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);

    connect(m_toolbar, &Toolbar::backRequested, this, [this] { if (auto* t = currentTab()) t->back(); });
    connect(m_toolbar, &Toolbar::forwardRequested, this, [this] { if (auto* t = currentTab()) t->forward(); });
    connect(m_toolbar, &Toolbar::reloadRequested, this, [this] { if (auto* t = currentTab()) t->reload(); });
    connect(m_toolbar, &Toolbar::stopRequested, this, [this] { if (auto* t = currentTab()) t->stop(); });
    connect(m_toolbar->mainMenu(), &MainMenu::command, this, &MainWindow::runCommand);
    connect(m_toolbar, &Toolbar::starClicked, this, &MainWindow::toggleBookmark);
    connect(m_bookmarks, &BookmarksBar::openRequested, this, [this](const QUrl& url, bool inNewTab) {
        if (inNewTab)
            m_tabs->newTab(url, m_tabs->currentIndex() + 1, false);
        else if (Tab* t = currentTab())
            t->setUrl(url);
    });
    connect(m_toolbar->omnibox(), &Omnibox::submitted, this, [this](const QString& text) {
        navigate(text);
        if (auto* t = currentTab())
            t->setFocus();
    });

    // Predicción de la barra de direcciones: al momento lo local; las búsquedas de DuckDuckGo, al parar de escribir.
    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(120);
    connect(m_suggestTimer, &QTimer::timeout, this, &MainWindow::requestSearchSuggestions);
    connect(m_toolbar->omnibox(), &QLineEdit::textEdited, this, [this](const QString& text) {
        suggest(text, {});
        m_suggestTimer->start();
    });
    connect(m_toolbar->omnibox(), &Omnibox::tabRequested, this, [this](int index) {
        m_tabs->setCurrentIndex(index);
        if (auto* t = currentTab())
            t->setFocus();
    });

    // Botón de extensiones (puzle), como en Chrome. Incógnito no tiene extensiones.
    m_toolbar->setExtensionsVisible(!m_incognito && profile->extensionManager());
    QMenu* extensions = m_toolbar->extensionsMenu();
    connect(extensions, &QMenu::aboutToShow, this, [this, profile, extensions] {
        extensions->clear();
        bool any = false;
        for (const QWebEngineExtensionInfo& e : profile->extensionManager()->extensions()) {
            if (!e.isEnabled() || isBuiltInExtension(e.id()))
                continue;
            any = true;
            QAction* a = extensions->addAction(e.name());
            const QUrl popup = e.actionPopupUrl();
            a->setEnabled(popup.isValid());
            connect(a, &QAction::triggered, this, [this, profile, popup] { openExtensionPopup(profile, popup); });
        }
        if (!any)
            extensions->addAction(tr("No hay extensiones activas"))->setEnabled(false);
        extensions->addSeparator();
        connect(extensions->addAction(tr("Gestionar extensiones")), &QAction::triggered, this,
                [this] { runCommand(MainMenu::Command::Extensions); });
    });

    addShortcuts();
    applyColorScheme(QGuiApplication::styleHints()->colorScheme());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::applyColorScheme);

    setWindowTitle(titleFor({}));
    resize(1280, 800);
    if (startWithTab)
        m_tabs->newTab(m_incognito ? kIncognitoNewTabUrl : kNewTabUrl);
}

QString MainWindow::titleFor(const QString& pageTitle) const
{
    const QString title = windowTitleFor(pageTitle);
    return m_incognito ? title + QString::fromUtf8(" (incógnito)") : title;
}

Tab* MainWindow::takeTab(int index)
{
    if (Tab* tab = m_tabs->tab(index)) {
        tab->disconnect(this);
        tab->page()->disconnect(this);
    }
    return m_tabs->takeTab(index);
}

void MainWindow::adoptTab(Tab* tab, int index)
{
    m_tabs->adoptTab(tab, index);
}

MainWindow::~MainWindow()
{
    // Las pestañas se destruyen después que los miembros de la ventana: que no avisen a nadie.
    // También las cerradas que esperan su deleteLater: siguen siendo hijas del stack.
    m_tabs->disconnect(this);
    for (Tab* tab : m_stack->findChildren<Tab*>(Qt::FindDirectChildrenOnly)) {
        tab->disconnect(this);
        tab->page()->disconnect(this);
    }
}

Tab* MainWindow::currentTab() const
{
    return m_tabs->currentTab();
}

void MainWindow::navigate(const QString& input)
{
    if (auto* t = currentTab())
        t->setUrl(urlFromInput(input));
}

void MainWindow::newTab()
{
    m_tabs->newTab(m_incognito ? kIncognitoNewTabUrl : kNewTabUrl);
    m_toolbar->omnibox()->setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::runCommand(MainMenu::Command command)
{
    using C = MainMenu::Command;
    switch (command) {
    case C::NewTab: newTab(); break;
    case C::NewWindow: emit newWindowRequested(false); break;
    case C::NewIncognitoWindow: emit newWindowRequested(true); break;
    case C::History: openInternal(QUrl(QStringLiteral("sora://history"))); break;
    case C::Downloads: openInternal(QUrl(QStringLiteral("sora://downloads"))); break;
    case C::BookmarkManager: openInternal(QUrl(QStringLiteral("sora://bookmarks"))); break;
    case C::ToggleBookmarksBar:
        m_bookmarksWanted = !m_bookmarksWanted;
        m_bookmarks->setVisible(m_bookmarksWanted && !isFullScreen());
        break;
    case C::ZoomIn: zoom(+1); break;
    case C::ZoomOut: zoom(-1); break;
    case C::ZoomReset: zoom(0); break;
    case C::FullScreen:
        if (Tab* t = currentTab(); t && m_pageFullScreen)
            t->page()->triggerAction(QWebEnginePage::ExitFullScreen); // vídeo a pantalla completa
        else
            setBrowserFullScreen(!isFullScreen());
        break;
    case C::Print: printPage(); break;
    case C::Find: placeFindBar(); m_findBar->open(); break;
    case C::ClearBrowsingData: openInternal(QUrl(QStringLiteral("sora://settings#borrar"))); break;
    case C::DevTools: openDevTools(); break;
    case C::Extensions: openInternal(QUrl(QStringLiteral("sora://extensions"))); break;
    case C::Settings: openInternal(QUrl(QStringLiteral("sora://settings"))); break;
    case C::About:
        QMessageBox::about(this, tr("Acerca de Sora"),
                           tr("<b>Sora %1</b><br>Navegador rápido y privado.<br><br>Motor: Qt WebEngine %2 "
                              "(Chromium %3).<br>Bloqueo de anuncios: adblock-rust de Brave.")
                               .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion()),
                                    QString::fromLatin1(qWebEngineChromiumVersion())));
        break;
    case C::Quit: qApp->closeAllWindows(); break;
    case C::AddBookmark: toggleBookmark(); break;
    case C::SavePage:
        if (Tab* t = currentTab())
            t->page()->triggerAction(QWebEnginePage::SavePage); // DownloadManager pregunta dónde
        break;
    }
}

void MainWindow::suggest(const QString& text, const QStringList& searches)
{
    QList<OpenTab> tabs;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (i != m_tabs->currentIndex())
            tabs.append({i, m_tabs->tab(i)->title(), m_tabs->tab(i)->url()});
    const auto bookmarks = m_bookmarkStore ? m_bookmarkStore->all() : QList<BookmarkStore::Bookmark>();
    // Incógnito no usa el historial (tampoco lo guarda).
    const auto history = m_history && !m_incognito && !text.trimmed().isEmpty() ? m_history->search(text.trimmed(), 30)
                                                                                : QList<HistoryStore::Entry>();
    Omnibox* box = m_toolbar->omnibox();
    const QString inlined = inlineCompletion(text, bookmarks, history);
    if (searches.isEmpty())
        box->setInlineCompletion(inlined);
    QList<Suggestion> list = buildSuggestions(text, tabs, bookmarks, history, searches);
    if (!inlined.isEmpty() && !list.isEmpty())
        list.first() = {Suggestion::Url, inlined, {}}; // como Chrome: la primera fila es lo completado
    box->setSuggestions(list);
}

void MainWindow::requestSearchSuggestions()
{
    if (m_suggestReply)
        m_suggestReply->abort();
    Omnibox* box = m_toolbar->omnibox();
    // Lo escrito, sin la parte completada en línea (seleccionada).
    const QString typed = box->hasSelectedText() ? box->text().left(box->selectionStart()) : box->text();
    if (typed.trimmed().isEmpty() || typed.startsWith(QLatin1String("sora:")) || typed.contains(QLatin1String("://")))
        return;
    static QNetworkAccessManager* network = new QNetworkAccessManager(qApp); // sin cookies ni caché del perfil
    QUrl url(QStringLiteral("https://duckduckgo.com/ac/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), typed);
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("list"));
    url.setQuery(query);
    QNetworkReply* reply = network->get(QNetworkRequest(url));
    m_suggestReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, typed] {
        reply->deleteLater();
        Omnibox* box = m_toolbar->omnibox();
        const QString now = box->hasSelectedText() ? box->text().left(box->selectionStart()) : box->text();
        if (reply->error() == QNetworkReply::NoError && now == typed && box->hasFocus())
            suggest(typed, parseSearchSuggestions(reply->readAll()));
    });
}

void MainWindow::openExtensionPopup(QWebEngineProfile* profile, const QUrl& url)
{
    // shortcut: tamaño fijo; Chrome ajusta la ventana al contenido. Medirlo si alguna extensión queda cortada.
    auto* popup = new QWebEngineView(profile, this);
    popup->setWindowFlags(Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->resize(380, 480);
    connect(popup->page(), &QWebEnginePage::windowCloseRequested, popup, &QWidget::close);
    popup->setUrl(url);
    const QRect screen = this->screen()->availableGeometry();
    QPoint at = mapToGlobal(QPoint(width() - popup->width() - 8, m_titleBar->height() + m_toolbar->height()));
    popup->move(qBound(screen.left(), at.x(), screen.right() - popup->width()), at.y());
    popup->show();
}

void MainWindow::openInternal(const QUrl& url)
{
    // Como Chrome: si la pestaña actual es una nueva pestaña, se reutiliza.
    Tab* t = currentTab();
    if (t && t->url().toString().startsWith(QLatin1String("sora://newtab")))
        t->setUrl(url);
    else
        m_tabs->newTab(url);
}

void MainWindow::zoom(int step)
{
    static constexpr double levels[] = {0.25, 0.33, 0.5, 0.67, 0.75, 0.8, 0.9, 1.0, 1.1, 1.25,
                                        1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0};
    Tab* t = currentTab();
    if (!t)
        return;
    double next = 1.0;
    const double now = t->zoomFactor();
    if (step > 0) {
        next = levels[std::size(levels) - 1];
        for (double l : levels)
            if (l > now + 0.001) { next = l; break; }
    } else if (step < 0) {
        next = levels[0];
        for (double l : levels)
            if (l < now - 0.001) next = l;
    }
    t->setZoomFactor(next);
    m_toolbar->mainMenu()->setZoomPercent(qRound(next * 100));
}

void MainWindow::setBrowserFullScreen(bool on)
{
    if (on == isFullScreen())
        return;
    m_titleBar->setVisible(!on);
    m_toolbar->setVisible(!on);
    m_bookmarks->setVisible(!on && m_bookmarksWanted);
    if (on) {
        m_wasMaximized = isMaximized();
        showFullScreen();
    } else if (m_wasMaximized) {
        showMaximized();
    } else {
        showNormal();
    }
}

void MainWindow::openDevTools()
{
    Tab* t = currentTab();
    if (!t)
        return;
    if (QWebEnginePage* dev = t->page()->devToolsPage()) {
        if (auto* view = qobject_cast<QWidget*>(dev->parent())) {
            view->raise();
            view->activateWindow();
        }
        return;
    }
    // Ventana aparte, como Chrome con las herramientas desacopladas.
    auto* view = new QWebEngineView;
    view->setAttribute(Qt::WA_DeleteOnClose);
    view->setPage(new QWebEnginePage(t->page()->profile(), view));
    t->page()->setDevToolsPage(view->page());
    view->setWindowTitle(tr("Herramientas para desarrolladores — %1").arg(t->title()));
    view->resize(1000, 700);
    connect(t, &QObject::destroyed, view, &QWidget::close);
    view->show();
}

void MainWindow::printPage()
{
    Tab* t = currentTab();
    if (!t)
        return;
    auto* printer = new QPrinter(QPrinter::HighResolution);
    QPrintDialog dialog(printer, this);
    dialog.setWindowTitle(tr("Imprimir"));
    if (dialog.exec() != QDialog::Accepted) {
        delete printer;
        return;
    }
    connect(t, &QWebEngineView::printFinished, this, [printer](bool) { delete printer; }, Qt::SingleShotConnection);
    t->print(printer);
}

void MainWindow::setStores(HistoryStore* history, BookmarkStore* bookmarks)
{
    m_history = m_incognito ? nullptr : history; // incógnito no deja rastro
    if (m_bookmarkStore)
        m_bookmarkStore->disconnect(this);
    m_bookmarkStore = bookmarks;
    if (m_bookmarkStore)
        connect(m_bookmarkStore, &BookmarkStore::changed, this, &MainWindow::refreshStar);
    m_bookmarks->setStore(bookmarks);
    refreshStar();
}

void MainWindow::toggleBookmark()
{
    Tab* t = currentTab();
    if (!m_bookmarkStore || !t || t->url().scheme() == QLatin1String("sora") || t->url().isEmpty()) {
        refreshStar();
        return;
    }
    if (m_bookmarkStore->contains(t->url()))
        m_bookmarkStore->removeUrl(t->url());
    else
        m_bookmarkStore->add(t->url(), t->title());
}

void MainWindow::refreshStar()
{
    Tab* t = currentTab();
    m_toolbar->setBookmarked(m_bookmarkStore && t && m_bookmarkStore->contains(t->url()));
}

void MainWindow::placeFindBar()
{
    m_findBar->adjustSize();
    m_findBar->move(m_stack->width() - m_findBar->width() - 16, 0);
}

void MainWindow::applyColorScheme(Qt::ColorScheme scheme)
{
    // Incógnito siempre en oscuro, como Chrome: se distingue de un vistazo.
    const Palette p = nieblaPalette(m_incognito ? Qt::ColorScheme::Dark : scheme);
    setStyleSheet(nieblaStyleSheet(p));
    m_titleBar->applyPalette(p);
    m_toolbar->applyPalette(p);
}

void MainWindow::showNotice(const QString& text)
{
    m_linkPreview->setText(text);
    placeLinkPreview();
    m_linkPreview->show();
    m_linkPreview->raise();
    QTimer::singleShot(5000, m_linkPreview, [this, text] {
        if (m_linkPreview->text() == text)
            m_linkPreview->hide();
    });
}

void MainWindow::watchTab(Tab* tab)
{
    const auto ifCurrent = [this, tab](auto&& f) {
        return [this, tab, f](auto&&... args) {
            if (tab == currentTab())
                f(args...);
        };
    };
    connect(tab, &QWebEngineView::urlChanged, this, ifCurrent([this](const QUrl& url) {
        m_toolbar->omnibox()->setUrl(url);
        refreshStar();
        refreshNavigation();
    }));
    connect(tab, &QWebEngineView::loadStarted, this, ifCurrent([this] { refreshNavigation(); }));
    connect(tab, &QWebEngineView::loadFinished, this, ifCurrent([this](bool) { refreshNavigation(); }));
    connect(tab, &QWebEngineView::loadFinished, this, [this, tab](bool ok) {
        const QString scheme = tab->url().scheme();
        if (ok && m_history && (scheme == QLatin1String("https") || scheme == QLatin1String("http")))
            m_history->addVisit(tab->url(), tab->title());
    });
    connect(tab->page(), &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        request.accept();
        m_pageFullScreen = request.toggleOn();
        setBrowserFullScreen(request.toggleOn());
    });
    connect(tab->page(), &QWebEnginePage::printRequested, this, ifCurrent([this] { printPage(); }));
    connect(tab, &Tab::blockedCountChanged, this, ifCurrent([this](int count) { m_toolbar->setBlockedCount(count); }));
    connect(tab->page(), &QWebEnginePage::linkHovered, this, ifCurrent([this](const QUrl& url) {
        if (url.isEmpty()) {
            m_linkPreview->hide();
            return;
        }
        m_linkPreview->setText(url.toDisplayString());
        placeLinkPreview();
        m_linkPreview->show();
        m_linkPreview->raise();
    }));
}

void MainWindow::showCurrentTab()
{
    Tab* tab = currentTab();
    if (!tab)
        return;
    m_toolbar->omnibox()->setModified(false); // lo escrito era para la pestaña anterior
    m_toolbar->omnibox()->setUrl(tab->url());
    m_toolbar->setBlockedCount(tab->blockedCount());
    m_linkPreview->hide();
    m_findBar->hide();
    refreshStar();
    m_toolbar->mainMenu()->setZoomPercent(qRound(tab->zoomFactor() * 100));
    setWindowTitle(titleFor(tab->title()));
    refreshNavigation();
}

void MainWindow::refreshNavigation()
{
    Tab* tab = currentTab();
    if (!tab)
        return;
    m_toolbar->setNavigation(tab->history()->canGoBack(), tab->history()->canGoForward(), tab->isLoading());
    if (m_stop)
        m_stop->setEnabled(tab->isLoading());
}

void MainWindow::selectTab(int index)
{
    if (m_tabs->count())
        m_tabs->setCurrentIndex(std::clamp(index, 0, m_tabs->count() - 1));
}

void MainWindow::toggleMaximized()
{
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::placeLinkPreview()
{
    m_linkPreview->adjustSize();
    const int maxWidth = m_stack->width() / 2;
    if (m_linkPreview->width() > maxWidth)
        m_linkPreview->resize(maxWidth, m_linkPreview->height());
    m_linkPreview->move(4, m_stack->height() - m_linkPreview->height() - 4);
}

void MainWindow::addShortcuts()
{
    const auto on = [this](const QKeySequence& keys, auto&& f) {
        connect(new QShortcut(keys, this), &QShortcut::activated, this, f);
    };
    on(QKeySequence(Qt::CTRL | Qt::Key_T), [this] { newTab(); });
    on(QKeySequence(Qt::CTRL | Qt::Key_N), [this] { emit newWindowRequested(false); });
    using C = MainMenu::Command;
    const std::pair<QKeySequence, C> commands[] = {
        {QKeySequence(Qt::CTRL | Qt::Key_H), C::History},
        {QKeySequence(Qt::CTRL | Qt::Key_J), C::Downloads},
        {QKeySequence(Qt::CTRL | Qt::Key_D), C::AddBookmark},
        {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), C::ToggleBookmarksBar},
        {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), C::BookmarkManager},
        {QKeySequence(Qt::CTRL | Qt::Key_Equal), C::ZoomIn},
        {QKeySequence(Qt::CTRL | Qt::Key_Plus), C::ZoomIn},
        {QKeySequence(Qt::CTRL | Qt::Key_Minus), C::ZoomOut},
        {QKeySequence(Qt::CTRL | Qt::Key_0), C::ZoomReset},
        {QKeySequence(Qt::Key_F11), C::FullScreen},
        {QKeySequence(Qt::CTRL | Qt::Key_P), C::Print},
        {QKeySequence(Qt::CTRL | Qt::Key_F), C::Find},
        {QKeySequence(Qt::CTRL | Qt::Key_S), C::SavePage},
        {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Delete), C::ClearBrowsingData},
        {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), C::DevTools},
        {QKeySequence(Qt::Key_F12), C::DevTools},
    };
    for (const auto& [keys, command] : commands)
        on(keys, [this, command] { runCommand(command); });
    on(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), [this] { emit newWindowRequested(true); });
    for (auto keys : {QKeySequence(Qt::CTRL | Qt::Key_W), QKeySequence(Qt::CTRL | Qt::Key_F4)})
        on(keys, [this] { m_tabs->closeTab(m_tabs->currentIndex()); });
    for (auto keys : {QKeySequence(Qt::CTRL | Qt::Key_Tab), QKeySequence(Qt::CTRL | Qt::Key_PageDown)})
        on(keys, [this] { selectTab((m_tabs->currentIndex() + 1) % m_tabs->count()); });
    for (auto keys : {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), QKeySequence(Qt::CTRL | Qt::Key_Backtab),
                      QKeySequence(Qt::CTRL | Qt::Key_PageUp)})
        on(keys, [this] { selectTab((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count()); });
    for (int n = 1; n <= 8; ++n)
        on(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_0 + n)), [this, n] { selectTab(n - 1); });
    on(QKeySequence(Qt::CTRL | Qt::Key_9), [this] { selectTab(m_tabs->count() - 1); });
    for (auto keys : {QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(Qt::ALT | Qt::Key_D), QKeySequence(Qt::Key_F6)})
        on(keys, [this] { m_toolbar->omnibox()->setFocus(Qt::ShortcutFocusReason); });
    for (auto keys : {QKeySequence(Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::Key_R)})
        on(keys, [this] { if (auto* t = currentTab()) t->reload(); });
    // Esc en la web: parar la carga. Fuera de la carga se apaga para no quitárselo a la página
    // (cerrar sus diálogos), y en la dirección sigue restaurando la URL.
    m_stop = new QShortcut(QKeySequence(Qt::Key_Escape), m_stack);
    m_stop->setObjectName(QStringLiteral("StopShortcut"));
    m_stop->setContext(Qt::WidgetWithChildrenShortcut);
    m_stop->setEnabled(false);
    connect(m_stop, &QShortcut::activated, this, [this] { if (auto* t = currentTab()) t->stop(); });
    on(QKeySequence(Qt::ALT | Qt::Key_Left), [this] { if (auto* t = currentTab()) t->back(); });
    on(QKeySequence(Qt::ALT | Qt::Key_Right), [this] { if (auto* t = currentTab()) t->forward(); });
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        m_titleBar->setMaximized(isMaximized());
#ifndef Q_OS_WIN
        const int m = isMaximized() ? 0 : kLinuxResizeMargin;
        centralWidget()->layout()->setContentsMargins(m, m, m, m);
#endif
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    // Las banderas de Qt que darían estos estilos le hacen pintar su propia barra; sin ellos
    // Windows ignora el doble clic en la barra, Win+flechas, minimizar desde la barra de tareas y
    // Alt+Espacio. Se añaden a mano y la barra sigue siendo la de Sora.
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR wanted = style | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    if (wanted != style) {
        SetWindowLongPtr(hwnd, GWL_STYLE, wanted);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
#endif
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_linkPreview->isVisible())
        placeLinkPreview();
}

// Fuera de Windows no hay marco: la franja del borde redimensiona.
void MainWindow::mousePressEvent(QMouseEvent* event)
{
#ifndef Q_OS_WIN
    if (event->button() == Qt::LeftButton && !isMaximized()) {
        const WindowHit hit = windowHitTest(event->position().toPoint(), size(), kLinuxResizeMargin, false, WindowHit::Client);
        Qt::Edges edges;
        if (hit == WindowHit::Left || hit == WindowHit::TopLeft || hit == WindowHit::BottomLeft) edges |= Qt::LeftEdge;
        if (hit == WindowHit::Right || hit == WindowHit::TopRight || hit == WindowHit::BottomRight) edges |= Qt::RightEdge;
        if (hit == WindowHit::Top || hit == WindowHit::TopLeft || hit == WindowHit::TopRight) edges |= Qt::TopEdge;
        if (hit == WindowHit::Bottom || hit == WindowHit::BottomLeft || hit == WindowHit::BottomRight) edges |= Qt::BottomEdge;
        if (edges && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            return;
        }
    }
#endif
    QMainWindow::mousePressEvent(event);
}

void MainWindow::leaveMaximizeButton()
{
    if (m_titleBar->maximizeButton()->property("hovered").toBool())
        QToolTip::hideText();
    m_titleBar->setMaximizeHovered(false);
    m_maximizePressed = false;
    m_titleBar->maximizeButton()->setDown(false);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    switch (msg->message) {
    case WM_NCHITTEST: {
        const POINT screen{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        // Los bordes se miden sobre el rectángulo de la ventana, que incluye el borde invisible
        // de redimensión de Windows (por fuera de la web a izquierda, derecha y abajo, así no tapa
        // la barra de desplazamiento). Con ExpandedClientAreaHint Windows no lo atiende solo.
        RECT win;
        GetWindowRect(msg->hwnd, &win);
        const UINT dpi = GetDpiForWindow(msg->hwnd);
        const int border = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        POINT pt = screen;
        ScreenToClient(msg->hwnd, &pt);
        const qreal dpr = devicePixelRatioF();
        const QPoint pos(qRound(pt.x / dpr), qRound(pt.y / dpr));
        WindowHit bar = WindowHit::Client;
        const QPoint inBar = m_titleBar->mapFrom(this, pos);
        if (m_titleBar->rect().contains(inBar))
            bar = m_titleBar->hitAt(inBar);
        const WindowHit hit = windowHitTest(QPoint(screen.x - win.left, screen.y - win.top),
                                            QSize(win.right - win.left, win.bottom - win.top),
                                            border, isMaximized(), bar);
        switch (hit) {
        case WindowHit::Left: *result = HTLEFT; break;
        case WindowHit::Right: *result = HTRIGHT; break;
        case WindowHit::Bottom: *result = HTBOTTOM; break;
        case WindowHit::BottomLeft: *result = HTBOTTOMLEFT; break;
        case WindowHit::BottomRight: *result = HTBOTTOMRIGHT; break;
        case WindowHit::Top: *result = HTTOP; break;
        case WindowHit::TopLeft: *result = HTTOPLEFT; break;
        case WindowHit::TopRight: *result = HTTOPRIGHT; break;
        case WindowHit::Caption: *result = HTCAPTION; break;
        case WindowHit::Maximize: *result = HTMAXBUTTON; break; // Snap Layouts de Windows 11
        default: *result = HTCLIENT; break;
        }
        return true;
    }
    // Maximizar es zona no cliente (HTMAXBUTTON, para Snap Layouts): Qt no le manda eventos de
    // ratón, así que el estado pasado, pulsado y el tooltip se llevan aquí.
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            QToolButton* max = m_titleBar->maximizeButton();
            if (!max->property("hovered").toBool())
                QToolTip::showText(QCursor::pos(), max->toolTip(), max);
            m_titleBar->setMaximizeHovered(true);
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_NONCLIENT, msg->hwnd, 0};
            TrackMouseEvent(&track);
        } else {
            leaveMaximizeButton();
        }
        return false;
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE:
        leaveMaximizeButton();
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            m_maximizePressed = true;
            m_titleBar->maximizeButton()->setDown(true);
            *result = 0;
            return true; // el clic se completa al soltar
        }
        return false;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            const bool click = m_maximizePressed; // pulsar fuera y soltar encima no cuenta
            m_maximizePressed = false;
            m_titleBar->maximizeButton()->setDown(false);
            if (click)
                toggleMaximized();
            *result = 0;
            return true;
        }
        return false;
    case WM_NCRBUTTONUP:
        // Clic derecho en la barra de título: el menú de la ventana de Windows, como en Chrome.
        // (Qt se queda el WM_CONTEXTMENU que lo abriría, así que se abre aquí.)
        if (msg->wParam == HTCAPTION) {
            HMENU menu = GetSystemMenu(msg->hwnd, FALSE);
            const bool maximized = IsZoomed(msg->hwnd);
            const UINT normal = maximized ? MF_GRAYED : MF_ENABLED;
            EnableMenuItem(menu, SC_RESTORE, maximized ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(menu, SC_MAXIMIZE, normal);
            EnableMenuItem(menu, SC_MOVE, normal);
            EnableMenuItem(menu, SC_SIZE, normal);
            const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, GET_X_LPARAM(msg->lParam),
                                               GET_Y_LPARAM(msg->lParam), 0, msg->hwnd, nullptr);
            if (command)
                PostMessageW(msg->hwnd, WM_SYSCOMMAND, WPARAM(command), 0);
            *result = 0;
            return true;
        }
        return false;
    default:
        break;
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

} // namespace sora
