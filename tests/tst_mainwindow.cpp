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
#include "ui/Omnibox.h"
#include "ui/Toolbar.h"

class TestMainWindow : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    sora::AdblockService m_adblock;

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
    void initTestCase() { m_profile = new QWebEngineProfile(this); }

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
