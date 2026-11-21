#include <QtTest>
#include <QUrlQuery>
#include <QShortcut>
#include <QToolButton>
#include <QWebEngineProfile>
#include "app/MainWindow.h"
#include "browser/Tab.h"
#include "browser/TabManager.h"
#include "privacy/AdblockService.h"
#include "ui/TabStrip.h"
#include "ui/TitleBar.h"
#include "ui/MainMenu.h"
#include "ui/BookmarksBar.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"
#include <QLabel>
#include <QLineEdit>
#include <QWebEngineSettings>
#include "ui/Omnibox.h"
#include "ui/Toolbar.h"

class TestMainWindow : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    sora::AdblockService m_adblock;
    QTemporaryDir m_storage;

    // Ventana visible y activa: los atajos solo funcionan en la ventana activa.
    std::unique_ptr<sora::MainWindow> openWindow()
    {
        auto w = std::make_unique<sora::MainWindow>(m_profile, &m_adblock);
        w->show();
        w->activateWindow();
        if (!QTest::qWaitForWindowActive(w.get()))
            qWarning("la ventana no llegó a activarse");
        return w;
    }
    static QToolButton* shieldOf(sora::MainWindow& w) { return w.findChild<QToolButton*>("ShieldButton"); }

private slots:
    void initTestCase()
    {
        // Perfil normal (con nombre), como el de Sora, en una carpeta temporal.
        m_profile = new QWebEngineProfile(QStringLiteral("tst-mainwindow"), this);
        m_profile->setPersistentStoragePath(m_storage.path());
        m_profile->setCachePath(m_storage.path() + "/cache");
    }

    void startsWithOneTab()
    {
        auto w = openWindow();
        QCOMPARE(w->tabs()->count(), 1);
        QVERIFY(w->currentTab());
    }
    void titleBarStartsAtTheTop()
    {
        // La barra de título propia ocupa el sitio de la nativa: nada de reservar el "área segura".
        auto w = openWindow();
        QVERIFY(!w->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea));
        QVERIFY(!w->centralWidget()->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea));
        QCOMPARE(w->findChild<sora::TitleBar*>()->mapTo(w.get(), QPoint(0, 0)).y(), 0);
    }
    void switchingTabsShowsThatTabsUrl()
    {
        // Escribir en A y pasar a B (la tira no quita el foco): la dirección debe ser la de B.
        auto w = openWindow();
        sora::Tab* b = w->tabs()->newTab({}, -1, false);
        QSignalSpy loaded(b, &sora::Tab::loadFinished);
        b->setHtml("<p>b</p>", QUrl("https://b.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        auto* box = w->findChild<sora::Omnibox*>();
        box->setFocus();
        QTRY_VERIFY(box->hasFocus());
        QTest::keyClicks(box, "hola");
        w->tabs()->setCurrentIndex(1);
        QCOMPARE(box->text(), QString("b.test"));
    }
    void blankPageShowsNewTabTitle()
    {
        auto w = openWindow();
        QSignalSpy loaded(w->currentTab(), &sora::Tab::loadFinished);
        w->navigate("about:blank");
        QTRY_COMPARE(loaded.count(), 1);
        QTest::qWait(50); // titleChanged llega después de loadFinished
        QCOMPARE(w->findChild<sora::TabStrip*>()->tabText(0), QString::fromUtf8("Nueva pestaña"));
        QCOMPARE(w->windowTitle(), QString("Sora"));
    }
    void closingCurrentTabFocusesTheNext()
    {
        auto w = openWindow();
        w->tabs()->newTab();
        w->tabs()->newTab();
        w->tabs()->setCurrentIndex(1);
        w->tabs()->closeTab(1);
        sora::Tab* now = w->currentTab();
        QWidget* focus = QApplication::focusWidget();
        QVERIFY(focus);
        QVERIFY(focus == now || now->isAncestorOf(focus) || focus == now->focusProxy());
    }
    void escapeStopsLoadingOnlyWhileLoading()
    {
        auto w = openWindow();
        auto* stop = w->findChild<QShortcut*>("StopShortcut");
        QVERIFY(stop);
        QVERIFY(!stop->isEnabled()); // sin cargar, Esc es para la web
        QSignalSpy activated(stop, &QShortcut::activated);
        sora::Tab* tab = w->currentTab();
        emit tab->loadStarted();
        QVERIFY(stop->isEnabled());
        tab->setFocus();
        QTRY_VERIFY(QApplication::focusWidget() && (QApplication::focusWidget() == tab || tab->isAncestorOf(QApplication::focusWidget())));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QCOMPARE(activated.count(), 1);
        // Con el foco en la dirección, Esc sigue siendo para la dirección.
        auto* box = w->findChild<sora::Omnibox*>();
        box->setFocus();
        QTRY_VERIFY(box->hasFocus());
        QTest::keyClick(box, Qt::Key_Escape);
        QCOMPARE(activated.count(), 1);
        emit tab->loadFinished(true);
        QVERIFY(!stop->isEnabled());
    }
    void zoomLikeChrome()
    {
        auto w = openWindow();
        QTest::keyClick(w.get(), Qt::Key_Equal, Qt::ControlModifier);
        QCOMPARE(w->currentTab()->zoomFactor(), 1.1);
        QCOMPARE(w->findChild<QLabel*>("ZoomLevel")->text(), QString("110 %"));
        QTest::keyClick(w.get(), Qt::Key_Minus, Qt::ControlModifier);
        QTest::keyClick(w.get(), Qt::Key_Minus, Qt::ControlModifier);
        QCOMPARE(w->currentTab()->zoomFactor(), 0.9);
        QTest::keyClick(w.get(), Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(w->currentTab()->zoomFactor(), 1.0);
    }
    void f11HidesBrowserBars()
    {
        auto w = openWindow();
        QTest::keyClick(w.get(), Qt::Key_F11);
        QTRY_VERIFY(w->isFullScreen());
        QVERIFY(!w->findChild<sora::TitleBar*>()->isVisible());
        QTest::keyClick(w.get(), Qt::Key_F11);
        QTRY_VERIFY(!w->isFullScreen());
        QVERIFY(w->findChild<sora::TitleBar*>()->isVisible());
    }
    void findBarSearchesThePage()
    {
        auto w = openWindow();
        QSignalSpy loaded(w->currentTab(), &sora::Tab::loadFinished);
        w->currentTab()->setHtml("<p>lobo pino lobo</p>");
        QTRY_COMPARE(loaded.count(), 1);
        QTest::keyClick(w.get(), Qt::Key_F, Qt::ControlModifier);
        auto* find = w->findChild<QLineEdit*>("FindText");
        QVERIFY(find && find->isVisible());
        QTRY_VERIFY(find->hasFocus());
        QTest::keyClicks(find, "lobo");
        auto* count = w->findChild<QLabel*>("FindCount");
        QTRY_COMPARE(count->text(), QString("1 de 2"));
        QTest::keyClick(find, Qt::Key_Escape);
        QVERIFY(!find->isVisible());
    }
    void menuOpensInternalPagesInPlaceOfNewTab()
    {
        auto w = openWindow();
        w->newTab();
        const int tabs = w->tabs()->count();
        emit w->findChild<sora::MainMenu*>()->command(sora::MainMenu::Command::History);
        QCOMPARE(w->tabs()->count(), tabs); // reutiliza la nueva pestaña, como Chrome
        QTRY_COMPARE(w->currentTab()->url(), QUrl("sora://history"));
    }
    void bookmarksBarToggles()
    {
        auto w = openWindow();
        auto* bar = w->findChild<sora::BookmarksBar*>();
        QVERIFY(bar->isVisible());
        QTest::keyClick(w.get(), Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(!bar->isVisible());
    }
    void devToolsOpenInTheirOwnWindow()
    {
        auto w = openWindow();
        QTest::keyClick(w.get(), Qt::Key_I, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(w->currentTab()->page()->devToolsPage() != nullptr);
    }
    void pageFullScreenIsAccepted()
    {
        // Pantalla completa de un vídeo (YouTube): la ventana pasa a pantalla completa sin barras.
        auto w = openWindow();
        QSignalSpy loaded(w->currentTab(), &sora::Tab::loadFinished);
        w->currentTab()->page()->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
        w->currentTab()->setHtml("<button id=b onclick='document.documentElement.requestFullscreen()'>x</button>", QUrl("https://v.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        QVERIFY(w->currentTab()->page()->settings()->testAttribute(QWebEngineSettings::FullScreenSupportEnabled));
    }
    void visitsGoToHistoryButNotInIncognito()
    {
        sora::HistoryStore history(m_storage.filePath("h1.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("h1.db"));
        auto w = openWindow();
        w->setStores(&history, &bookmarks);
        QSignalSpy loaded(w->currentTab(), &sora::Tab::loadFinished);
        w->currentTab()->setHtml("<title>Lobos</title>", QUrl("https://h.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_COMPARE(history.search("h.test").size(), 1);
        QCOMPARE(history.search("h.test").first().title, QString("Lobos"));

        auto* otr = new QWebEngineProfile(this); // incógnito
        sora::MainWindow incognito(otr, &m_adblock);
        incognito.setStores(&history, &bookmarks);
        QSignalSpy loaded2(incognito.currentTab(), &sora::Tab::loadFinished);
        incognito.currentTab()->setHtml("<title>Secreto</title>", QUrl("https://secreto.test/"));
        QTRY_VERIFY(loaded2.count() >= 1);
        QTest::qWait(100);
        QCOMPARE(history.search("secreto").size(), 0);
    }
    void starAndCtrlDToggleBookmark()
    {
        sora::HistoryStore history(m_storage.filePath("b1.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("b1.db"));
        auto w = openWindow();
        w->setStores(&history, &bookmarks);
        QSignalSpy loaded(w->currentTab(), &sora::Tab::loadFinished);
        w->currentTab()->setHtml("<title>Pinos</title>", QUrl("https://p.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        auto* star = w->findChild<QToolButton*>("StarButton");
        QVERIFY(star && star->isEnabled() && !star->isChecked());
        QTest::keyClick(w.get(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(bookmarks.contains(QUrl("https://p.test/")));
        QCOMPARE(bookmarks.all().first().title, QString("Pinos"));
        QVERIFY(star->isChecked());
        QTest::mouseClick(star, Qt::LeftButton);
        QVERIFY(!bookmarks.contains(QUrl("https://p.test/")));
        QVERIFY(!star->isChecked());
    }
    void bookmarksBarShowsAndOpensBookmarks()
    {
        sora::HistoryStore history(m_storage.filePath("b2.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("b2.db"));
        bookmarks.add(QUrl("https://lobo.test/"), "Lobo");
        auto w = openWindow();
        w->setStores(&history, &bookmarks);
        auto* bar = w->findChild<sora::BookmarksBar*>();
        QToolButton* button = nullptr;
        for (auto* b : bar->findChildren<QToolButton*>())
            if (b->text() == "Lobo") button = b;
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        QTRY_COMPARE(w->currentTab()->url(), QUrl("https://lobo.test/"));
        bookmarks.add(QUrl("https://pino.test/"), "Pino"); // la barra sigue al almacén
        QTRY_VERIFY(std::any_of(bar->findChildren<QToolButton*>().begin(), bar->findChildren<QToolButton*>().end(),
                                [](QToolButton* b) { return b->text() == "Pino"; }));
    }
    void ctrlTOpensTab()
    {
        auto w = openWindow();
        QTest::keyClick(w.get(), Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(w->tabs()->count(), 2);
        QCOMPARE(w->tabs()->currentIndex(), 1);
        QCOMPARE(w->findChild<sora::TabStrip*>()->count(), 2);
    }
    void ctrlWClosesTab()
    {
        auto w = openWindow();
        w->tabs()->newTab();
        QTest::keyClick(w.get(), Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(w->tabs()->count(), 1);
        QCOMPARE(w->findChild<sora::TabStrip*>()->count(), 1);
    }
    void ctrlNumberSelects()
    {
        auto w = openWindow();
        w->tabs()->newTab();
        w->tabs()->newTab();
        QTest::keyClick(w.get(), Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(w->tabs()->currentIndex(), 0);
        QTest::keyClick(w.get(), Qt::Key_9, Qt::ControlModifier);
        QCOMPARE(w->tabs()->currentIndex(), 2);
        QCOMPARE(w->findChild<sora::TabStrip*>()->currentIndex(), 2);
    }
    void tabTitleFollowsPage()
    {
        auto w = openWindow();
        w->currentTab()->setHtml("<title>Hola</title>");
        QTRY_COMPARE(w->findChild<sora::TabStrip*>()->tabText(0), QString("Hola"));
        QCOMPARE(w->windowTitle(), QString::fromUtf8("Hola — Sora"));
    }
    void shieldFollowsCurrentTab()
    {
        auto w = openWindow();
        w->tabs()->tab(0)->countBlocked();
        QCOMPARE(shieldOf(*w)->text(), QString("1"));
        w->tabs()->newTab();
        QCOMPARE(shieldOf(*w)->text(), QString("0"));
        w->tabs()->setCurrentIndex(0);
        QCOMPARE(shieldOf(*w)->text(), QString("1"));
    }
    void darkSchemeRestyles()
    {
        auto w = openWindow();
        w->applyColorScheme(Qt::ColorScheme::Dark);
        QVERIFY(w->styleSheet().contains("#1b1f27"));
        w->applyColorScheme(Qt::ColorScheme::Light);
        QVERIFY(w->styleSheet().contains("#e9edf3"));
    }
    void closingLastTabClosesWindow()
    {
        auto w = openWindow();
        QTest::keyClick(w.get(), Qt::Key_W, Qt::ControlModifier);
        QTRY_VERIFY(!w->isVisible());
    }
    void stripCloseButtonClosesTab()
    {
        auto w = openWindow();
        w->tabs()->newTab();
        emit w->findChild<sora::TabStrip*>()->tabCloseRequested(0);
        QCOMPARE(w->tabs()->count(), 1);
    }
    void movingTabsKeepsOrder()
    {
        auto w = openWindow();
        w->tabs()->newTab();
        sora::Tab* first = w->tabs()->tab(0);
        w->findChild<sora::TabStrip*>()->moveTab(0, 1);
        QCOMPARE(w->tabs()->tab(1), first);
    }
    void bareDomainBecomesHttps() { QCOMPARE(sora::urlFromInput("example.com"), QUrl("https://example.com")); }
    void explicitSchemeIsKept() { QCOMPARE(sora::urlFromInput("http://a.b/c"), QUrl("http://a.b/c")); }
    void textBecomesSearch() {
        const QUrl u = sora::urlFromInput("hola mundo");
        QCOMPARE(u.host(), QString("duckduckgo.com"));
        QCOMPARE(QUrlQuery(u).queryItemValue("q"), QString("hola mundo"));
    }
    void localhostUsesHttp() { QCOMPARE(sora::urlFromInput("localhost:8080"), QUrl("http://localhost:8080")); }
    void surroundingSpacesIgnored() { QCOMPARE(sora::urlFromInput("  example.com  "), QUrl("https://example.com")); }

    void displayUrlHidesHttpsAndWww()
    {
        QCOMPARE(sora::displayUrl(QUrl("https://www.example.com/a?b")), QString("example.com/a?b"));
        QCOMPARE(sora::displayUrl(QUrl("https://example.com/")), QString("example.com"));
        QCOMPARE(sora::displayUrl(QUrl("http://x.test/")), QString("http://x.test/"));
        QCOMPARE(sora::displayUrl(QUrl("about:blank")), QString());
    }
    void omniboxKeepsWhatUserTypes()
    {
        sora::Omnibox box;
        box.show();
        box.activateWindow();
        box.setFocus();
        QTRY_VERIFY(box.hasFocus());
        QTest::keyClicks(&box, "hola");
        box.setUrl(QUrl("https://x.test"));
        QCOMPARE(box.text(), QString("hola"));
        box.clearFocus();
        box.setUrl(QUrl("https://x.test"));
        QCOMPARE(box.text(), QString("x.test"));
    }
    void escapeRestoresUrl()
    {
        sora::Omnibox box;
        box.setUrl(QUrl("https://x.test"));
        box.show();
        box.activateWindow();
        box.setFocus();
        QTRY_VERIFY(box.hasFocus());
        box.selectAll();
        QTest::keyClicks(&box, "otra cosa");
        QTest::keyClick(&box, Qt::Key_Escape);
        QCOMPARE(box.text(), QString("x.test"));
    }
    void submitEmitsText()
    {
        sora::Omnibox box;
        QSignalSpy spy(&box, &sora::Omnibox::submitted);
        box.show();
        box.setFocus();
        QTest::keyClicks(&box, "hola");
        QTest::keyClick(&box, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("hola"));
    }
    void shieldShowsCount()
    {
        sora::Toolbar bar;
        bar.setBlockedCount(14);
        auto* shield = bar.findChild<QToolButton*>("ShieldButton");
        QVERIFY(shield);
        QVERIFY(shield->text().contains("14"));
        QVERIFY(shield->toolTip().contains("14"));
    }
    void reloadBecomesStop()
    {
        sora::Toolbar bar;
        bar.show();
        QSignalSpy stop(&bar, &sora::Toolbar::stopRequested);
        QSignalSpy reload(&bar, &sora::Toolbar::reloadRequested);
        auto* button = bar.findChild<QToolButton*>("ReloadButton");
        QVERIFY(button);
        bar.setNavigation(false, false, true);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(stop.count(), 1);
        bar.setNavigation(false, false, false);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(reload.count(), 1);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
