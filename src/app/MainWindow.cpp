#include "app/MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWindow>

#include "browser/Tab.h"
#include "browser/TabManager.h"
#include "ui/BookmarksBar.h"
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

    QUrl search(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), input);
    search.setQuery(query);
    return search;
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

MainWindow::MainWindow(QWebEngineProfile* profile, const AdblockService* adblock, QWidget* parent)
    : QMainWindow(parent)
    , m_titleBar(new TitleBar(this))
    , m_toolbar(new Toolbar(this))
    , m_bookmarks(new BookmarksBar(this))
    , m_stack(new QStackedWidget(this))
    , m_tabs(new TabManager(profile, adblock, m_stack, this))
    , m_linkPreview(new QLabel(m_stack))
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

    TabStrip* strip = m_titleBar->tabs();
    connect(m_tabs, &TabManager::tabInserted, this, [this, strip](int index) {
        m_syncing = true;
        strip->insertTab(index, kNewTabTitle);
        m_syncing = false;
        watchTab(m_tabs->tab(index));
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
            setWindowTitle(windowTitleFor(title));
    });
    connect(m_tabs, &TabManager::tabIconChanged, strip, &QTabBar::setTabIcon);
    connect(m_tabs, &TabManager::lastTabClosed, this, &QWidget::close);

    connect(strip, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_syncing)
            m_tabs->setCurrentIndex(index);
    });
    connect(strip, &QTabBar::tabCloseRequested, m_tabs, &TabManager::closeTab);
    connect(strip, &QTabBar::tabMoved, m_tabs, &TabManager::moveTab);
    connect(strip, &TabStrip::newTabRequested, this, &MainWindow::newTab);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &MainWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);

    connect(m_toolbar, &Toolbar::backRequested, this, [this] { if (auto* t = currentTab()) t->back(); });
    connect(m_toolbar, &Toolbar::forwardRequested, this, [this] { if (auto* t = currentTab()) t->forward(); });
    connect(m_toolbar, &Toolbar::reloadRequested, this, [this] { if (auto* t = currentTab()) t->reload(); });
    connect(m_toolbar, &Toolbar::stopRequested, this, [this] { if (auto* t = currentTab()) t->stop(); });
    connect(m_toolbar, &Toolbar::newTabRequested, this, &MainWindow::newTab);
    connect(m_toolbar, &Toolbar::closeTabRequested, this, [this] { m_tabs->closeTab(m_tabs->currentIndex()); });
    connect(m_toolbar, &Toolbar::quitRequested, this, &QWidget::close);
    connect(m_toolbar->omnibox(), &Omnibox::submitted, this, [this](const QString& text) {
        navigate(text);
        if (auto* t = currentTab())
            t->setFocus();
    });

    addShortcuts();
    applyColorScheme(QGuiApplication::styleHints()->colorScheme());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::applyColorScheme);

    setWindowTitle(QStringLiteral("Sora"));
    resize(1280, 800);
    m_tabs->newTab();
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
    m_tabs->newTab();
    m_toolbar->omnibox()->setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::applyColorScheme(Qt::ColorScheme scheme)
{
    const Palette p = nieblaPalette(scheme);
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
        refreshNavigation();
    }));
    connect(tab, &QWebEngineView::loadStarted, this, ifCurrent([this] { refreshNavigation(); }));
    connect(tab, &QWebEngineView::loadFinished, this, ifCurrent([this](bool) { refreshNavigation(); }));
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
    setWindowTitle(windowTitleFor(tab->title()));
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
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            m_titleBar->setMaximizeHovered(true);
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_NONCLIENT, msg->hwnd, 0};
            TrackMouseEvent(&track);
        } else {
            m_titleBar->setMaximizeHovered(false);
        }
        return false;
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE:
        m_titleBar->setMaximizeHovered(false);
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            *result = 0;
            return true; // el clic se completa al soltar
        }
        return false;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            toggleMaximized();
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
