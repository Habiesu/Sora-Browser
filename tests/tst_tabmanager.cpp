#include <QtTest>
#include <QAction>
#include <QWebEngineSettings>
#include <QStackedWidget>
#include <QWebEngineProfile>

#include "browser/Tab.h"
#include "app/Settings.h"
#include "browser/TabManager.h"
#include <QFile>
#include "privacy/AdblockEngine.h"
#include "privacy/AdblockService.h"

using sora::Tab;
using sora::TabManager;

class TestTabManager : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    sora::AdblockService m_adblock;

private slots:
    void initTestCase() { m_profile = new QWebEngineProfile(this); } // sin nombre: no toca disco

    void newTabAddsAndActivates()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        QSignalSpy inserted(&tabs, &TabManager::tabInserted);
        tabs.newTab();
        tabs.newTab();
        QCOMPARE(tabs.count(), 2);
        QCOMPARE(tabs.currentIndex(), 1);
        QCOMPARE(stack.currentWidget(), tabs.tab(1));
        QCOMPARE(inserted.count(), 2);
        QCOMPARE(inserted.last().at(0).toInt(), 1);
    }

    void closeCurrentSelectsNeighbor()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab(); tabs.newTab(); tabs.newTab();
        tabs.setCurrentIndex(1);
        Tab* third = tabs.tab(2);
        tabs.closeTab(1);
        QCOMPARE(tabs.count(), 2);
        QCOMPARE(tabs.currentIndex(), 1);
        QCOMPARE(tabs.currentTab(), third);
        tabs.closeTab(1); // la última y actual
        QCOMPARE(tabs.currentIndex(), 0);
    }

    void closingLastTabEmitsLastTabClosed()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        QSignalSpy last(&tabs, &TabManager::lastTabClosed);
        tabs.newTab();
        tabs.closeTab(0);
        QCOMPARE(last.count(), 1);
        QCOMPARE(tabs.count(), 0);
    }

    void titleChangeIsReported()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        QSignalSpy titles(&tabs, &TabManager::tabTitleChanged);
        tabs.newTab();
        tabs.newTab();
        tabs.tab(0)->setHtml("<title>A</title>");
        QTRY_VERIFY(std::any_of(titles.begin(), titles.end(), [](const QList<QVariant>& args) {
            return args.at(0).toInt() == 0 && args.at(1).toString() == "A";
        }));
    }

    void blockedCountIsPerTab()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab(); tabs.newTab();
        tabs.tab(0)->countBlocked();
        tabs.tab(0)->countBlocked();
        QCOMPARE(tabs.tab(0)->blockedCount(), 2);
        QCOMPARE(tabs.tab(1)->blockedCount(), 0);
        QSignalSpy loaded(tabs.tab(0), &Tab::loadFinished);
        tabs.tab(0)->setHtml("<p>otra</p>");
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(tabs.tab(0)->blockedCount(), 0);
    }

    void tabTracksLoading()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        Tab* tab = tabs.newTab();
        bool loadingWhenStarted = false;
        connect(tab, &Tab::loadStarted, this, [&] { loadingWhenStarted = tab->isLoading(); });
        QSignalSpy loaded(tab, &Tab::loadFinished);
        QVERIFY(!tab->isLoading());
        tab->setHtml("<p>x</p>");
        QTRY_COMPARE(loaded.count(), 1);
        QVERIFY(loadingWhenStarted);
        QVERIFY(!tab->isLoading());
    }

    void pageCanCloseItsTab()
    {
        // Las ventanas de inicio de sesión (Google, GitHub) se cierran con window.close().
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab();
        tabs.newTab();
        emit tabs.tab(1)->page()->windowCloseRequested();
        QCOMPARE(tabs.count(), 1);
    }

    void blockedRequestsAreCountedWithProfileInterceptor()
    {
        // El perfil bloquea todo (también service workers); cada pestaña sigue contando lo suyo.
        QFile f(qEnvironmentVariable("SORA_TEST_DATA") + "/rules-small.txt");
        QVERIFY(f.open(QIODevice::ReadOnly));
        sora::AdblockService adblock;
        adblock.setEngine(std::make_shared<const sora::AdblockEngine>(sora::AdblockEngine::fromRules(f.readAll())));
        auto* profile = new QWebEngineProfile(this);
        adblock.attach(profile);
        QStackedWidget stack;
        TabManager tabs(profile, &adblock, &stack);
        tabs.newTab();
        tabs.tab(0)->setHtml("<img src='https://ads.example/x.js'>", QUrl("https://site.test/"));
        QTRY_COMPARE(tabs.tab(0)->blockedCount(), 1);
    }

    void blockedCountsAreBatched()
    {
        // Muchos bloqueos seguidos (una web con cientos de anuncios) avisan a la interfaz una sola vez,
        // no uno por uno: el filtrado corre en el mismo hilo que la interfaz y que Chromium.
        sora::AdblockService adblock;
        QSignalSpy spy(&adblock, &sora::AdblockService::blockedOnSite);
        for (int i = 0; i < 300; ++i)
            adblock.recordBlocked(QUrl("https://site.test/p"));
        adblock.recordBlocked(QUrl("https://otro.test/"));
        QCOMPARE(spy.count(), 0);
        QTRY_COMPARE(spy.count(), 2);
        QHash<QString, int> got;
        for (const auto& args : spy)
            got[args.at(0).toString()] = args.at(1).toInt();
        QCOMPARE(got.value("site.test"), 300);
        QCOMPARE(got.value("otro.test"), 1);
    }
    void contextMenuIsInSpanish()
    {
        // Qt 6.12 no traduce las acciones del menú de la web aunque haya traductor.
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        QWebEnginePage* page = tabs.newTab()->page();
        QCOMPARE(page->action(QWebEnginePage::Back)->text(), QString::fromUtf8("Atrás"));
        QCOMPARE(page->action(QWebEnginePage::Reload)->text(), QString("Volver a cargar"));
        QCOMPARE(page->action(QWebEnginePage::OpenLinkInNewTab)->text(), QString::fromUtf8("Abrir enlace en una pestaña nueva"));
        QCOMPARE(page->action(QWebEnginePage::Copy)->text(), QString("Copiar"));
    }
    void downloadActionsAreInSpanish()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        QWebEnginePage* page = tabs.newTab()->page();
        QCOMPARE(page->action(QWebEnginePage::DownloadImageToDisk)->text(), QString("Descargar imagen"));
        QVERIFY(page->action(QWebEnginePage::SavePage)->isVisible());
        QVERIFY(!page->action(QWebEnginePage::InspectElement)->isVisible());
    }
    void webglOffOnlyWhereItCrashes()
    {
        // ANGLE de Qt 6.12 se cae con el globo de github.com (GPU en el mismo proceso que Sora).
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        Tab* tab = tabs.newTab();
        QWebEngineSettings* s = tab->page()->settings();
        auto* page = static_cast<sora::Page*>(tab->page());
        QVERIFY(page->acceptNavigationRequest(QUrl("https://github.com/"), QWebEnginePage::NavigationTypeTyped, true));
        QVERIFY(!s->testAttribute(QWebEngineSettings::WebGLEnabled));
        page->acceptNavigationRequest(QUrl("https://threejs.org/"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(s->testAttribute(QWebEngineSettings::WebGLEnabled));
    }
    void autoplayLikeChrome()
    {
        // Como Chrome: los vídeos arrancan solos (con sonido) en sitios de vídeo; en los demás, al tocar.
        sora::Settings& settings = sora::Settings::instance();
        settings.setValue("autoplay", "sites");
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        Tab* tab = tabs.newTab();
        QWebEngineSettings* s = tab->page()->settings();
        auto* page = static_cast<sora::Page*>(tab->page());
        const auto gesture = QWebEngineSettings::PlaybackRequiresUserGesture;
        page->acceptNavigationRequest(QUrl("https://www.youtube.com/watch?v=1"), QWebEnginePage::NavigationTypeTyped, true);
        QVERIFY(!s->testAttribute(gesture));
        page->acceptNavigationRequest(QUrl("https://noticias.test/"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(s->testAttribute(gesture));
        page->acceptNavigationRequest(QUrl("https://m.twitch.tv/lobo"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(!s->testAttribute(gesture)); // subdominios también

        settings.setValue("autoplay", "always");
        page->acceptNavigationRequest(QUrl("https://noticias.test/"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(!s->testAttribute(gesture));
        settings.setValue("autoplay", "never");
        page->acceptNavigationRequest(QUrl("https://www.youtube.com/"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(s->testAttribute(gesture));

        QVERIFY(!settings.setValue("autoplaySites", QStringList{"javascript:alert(1)"}));
        QVERIFY(settings.setValue("autoplaySites", QStringList{"lobos.test"}));
        settings.setValue("autoplay", "sites");
        page->acceptNavigationRequest(QUrl("https://lobos.test/v"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(!s->testAttribute(gesture));
        page->acceptNavigationRequest(QUrl("https://www.youtube.com/"), QWebEnginePage::NavigationTypeLinkClicked, true);
        QVERIFY(s->testAttribute(gesture));
        settings.setValue("autoplaySites", settings.defaultAutoplaySites());
    }
    void popupOpensNextToCurrent()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab(); tabs.newTab(); tabs.newTab();
        tabs.setCurrentIndex(0);
        QWebEngineView* opened = tabs.tab(0)->createWindow(QWebEnginePage::WebBrowserTab);
        QCOMPARE(tabs.count(), 4);
        QCOMPARE(opened, tabs.tab(1));
        QCOMPARE(tabs.currentIndex(), 1);
        tabs.tab(1)->createWindow(QWebEnginePage::WebBrowserBackgroundTab);
        QCOMPARE(tabs.count(), 5);
        QCOMPARE(tabs.currentIndex(), 1);
    }

    void movingCurrentTabDoesNotHideIt()
    {
        // Arrastrar la pestaña visible no debe ocultarla (la web recibiría visibilitychange).
        struct HideCounter : QObject {
            int hides = 0;
            bool eventFilter(QObject*, QEvent* e) override { hides += e->type() == QEvent::Hide; return false; }
        } counter;
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab(); tabs.newTab(); tabs.newTab();
        stack.show();
        tabs.setCurrentIndex(0);
        Tab* first = tabs.tab(0);
        QSignalSpy current(&tabs, &TabManager::currentChanged);
        first->installEventFilter(&counter);
        tabs.moveTab(0, 1);
        tabs.moveTab(1, 2);
        QCOMPARE(counter.hides, 0);
        QCOMPARE(tabs.tab(2), first);
        QCOMPARE(tabs.currentIndex(), 2);
        QCOMPARE(tabs.currentTab(), first);
        QCOMPARE(current.count(), 0); // sigue siendo la misma pestaña: nada que anunciar
    }

    void moveKeepsStackInSync()
    {
        QStackedWidget stack;
        TabManager tabs(m_profile, &m_adblock, &stack);
        tabs.newTab(); tabs.newTab(); tabs.newTab();
        Tab* first = tabs.tab(0);
        tabs.moveTab(0, 2);
        QCOMPARE(tabs.tab(2), first);
        QCOMPARE(tabs.indexOf(first), 2);
    }
};

QTEST_MAIN(TestTabManager)
#include "tst_tabmanager.moc"
