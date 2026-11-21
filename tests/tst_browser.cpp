#include <QtTest>
#include <QWebEngineProfile>

#include "app/Browser.h"
#include "app/MainWindow.h"
#include "browser/Tab.h"
#include "browser/TabManager.h"
#include "privacy/AdblockService.h"
#include "data/BookmarkStore.h"
#include "ui/TabStrip.h"
#include <QToolButton>

using sora::Browser;
using sora::MainWindow;

class TestBrowser : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    sora::AdblockService m_adblock;
    QTemporaryDir m_storage;

private slots:
    void initTestCase()
    {
        // Perfil normal (con nombre) como el de Sora, pero en una carpeta temporal.
        m_profile = new QWebEngineProfile(QStringLiteral("tst-browser"), this);
        m_profile->setPersistentStoragePath(m_storage.path());
        m_profile->setCachePath(m_storage.path() + "/cache");
    }

    void newWindowOpensAnotherWindow()
    {
        Browser browser(m_profile, &m_adblock);
        MainWindow* first = browser.newWindow();
        MainWindow* second = browser.newWindow();
        QCOMPARE(browser.windows().size(), 2);
        QVERIFY(first != second);
        QVERIFY(second->isVisible());
        QCOMPARE(second->tabs()->count(), 1);
        QVERIFY(!second->isIncognito());
    }
    void incognitoWindowDoesNotTouchDisk()
    {
        Browser browser(m_profile, &m_adblock);
        browser.newWindow();
        MainWindow* incognito = browser.newWindow(true);
        QVERIFY(incognito->isIncognito());
        QWebEngineProfile* profile = incognito->currentTab()->page()->profile();
        QVERIFY(profile->isOffTheRecord());
        QVERIFY(profile != m_profile);
        QVERIFY(incognito->windowTitle().contains(QString::fromUtf8("incógnito")));
    }
    void closedWindowsAreForgotten()
    {
        Browser browser(m_profile, &m_adblock);
        browser.newWindow();
        MainWindow* second = browser.newWindow();
        second->close();
        QTRY_COMPARE(browser.windows().size(), 1);
    }
    void detachingKeepsTheSamePage()
    {
        // Arrastrar una pestaña fuera: ventana nueva con la misma página (no se recarga).
        Browser browser(m_profile, &m_adblock);
        MainWindow* first = browser.newWindow();
        first->tabs()->newTab();
        sora::Tab* moved = first->tabs()->tab(1);
        MainWindow* created = browser.detachTab(first, 1, QPoint(300, 300));
        QVERIFY(created && created != first);
        QCOMPARE(first->tabs()->count(), 1);
        QCOMPARE(created->tabs()->count(), 1);
        QCOMPARE(created->tabs()->tab(0), moved);
        QCOMPARE(created->currentTab(), moved);
    }
    void movingIntoAnotherWindow()
    {
        Browser browser(m_profile, &m_adblock);
        MainWindow* a = browser.newWindow();
        MainWindow* b = browser.newWindow();
        a->tabs()->newTab();
        sora::Tab* moved = a->tabs()->tab(1);
        QVERIFY(browser.moveTab(a, 1, b, 0));
        QCOMPARE(a->tabs()->count(), 1);
        QCOMPARE(b->tabs()->count(), 2);
        QCOMPARE(b->tabs()->tab(0), moved);
    }
    void movingTheLastTabClosesItsWindow()
    {
        Browser browser(m_profile, &m_adblock);
        MainWindow* a = browser.newWindow();
        MainWindow* b = browser.newWindow();
        QVERIFY(browser.moveTab(a, 0, b, 1));
        QTRY_COMPARE(browser.windows().size(), 1);
        QCOMPARE(b->tabs()->count(), 2);
    }
    void detachedWindowFollowsTheMouse()
    {
        // Como Chrome: al sacar la pestaña aparece su ventana bajo el ratón y lo sigue hasta soltar.
        Browser browser(m_profile, &m_adblock);
        MainWindow* first = browser.newWindow();
        first->tabs()->newTab();
        emit first->tabStrip()->tabDetachRequested(1, QPoint(500, 400));
        QCOMPARE(browser.windows().size(), 2);
        MainWindow* created = browser.windows().last();
        QVERIFY(created != first);
        QVERIFY(created->tabStrip()->isDraggingWindow());
        created->tabStrip()->endWindowDrag();
    }
    void draggedWindowJoinsAnotherStrip()
    {
        // Ventana de una pestaña arrastrada sobre la tira de otra: se une ya y se sigue arrastrando ahí.
        Browser browser(m_profile, &m_adblock);
        MainWindow* target = browser.newWindow();
        target->tabs()->newTab();
        target->move(100, 100);
        MainWindow* floating = browser.newWindow();
        sora::Tab* moved = floating->currentTab();
        floating->tabStrip()->beginWindowDrag(QPoint(900, 600));
        const QPoint overStrip = target->tabStrip()->mapToGlobal(target->tabStrip()->tabRect(1).center());
        emit floating->tabStrip()->windowDragMoved(overStrip);
        QCOMPARE(target->tabs()->count(), 3);
        QVERIFY(target->tabs()->tab(1) == moved || target->tabs()->tab(2) == moved);
        QVERIFY(target->tabStrip()->isDraggingTab() || target->tabStrip()->mouseGrabber() == target->tabStrip());
        QTRY_COMPARE(browser.windows().size(), 1);
        QTest::mouseRelease(target->tabStrip(), Qt::LeftButton, {}, target->tabStrip()->mapFromGlobal(overStrip));
    }
    void tabsCannotCrossIntoIncognito()
    {
        Browser browser(m_profile, &m_adblock);
        MainWindow* normal = browser.newWindow();
        normal->tabs()->newTab();
        MainWindow* incognito = browser.newWindow(true);
        QVERIFY(!browser.moveTab(normal, 1, incognito, 0));
        QCOMPARE(normal->tabs()->count(), 2);
    }
    void storesReachEveryWindow()
    {
        Browser browser(m_profile, &m_adblock);
        browser.newWindow();
        browser.openStores(m_storage.filePath("sora.db"));
        browser.bookmarks()->add(QUrl("https://a.test/"), "A");
        MainWindow* later = browser.newWindow();
        auto* bar = later->findChild<QWidget*>("BookmarksBar");
        QTRY_VERIFY(bar->findChildren<QToolButton*>().size() == 1);
    }
    void ctrlNOpensWindow()
    {
        Browser browser(m_profile, &m_adblock);
        MainWindow* w = browser.newWindow();
        w->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(w));
        QTest::keyClick(w, Qt::Key_N, Qt::ControlModifier);
        QCOMPARE(browser.windows().size(), 2);
        QTest::keyClick(browser.windows().last(), Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(browser.windows().size(), 3);
        QVERIFY(browser.windows().last()->isIncognito());
    }
};

QTEST_MAIN(TestBrowser)
#include "tst_browser.moc"
