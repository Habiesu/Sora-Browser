#include <QtTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "app/InternalPages.h"
#include "browser/Tab.h"
#include "ui/Omnibox.h"

class TestInternalPages : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;

    static QVariant runJs(QWebEnginePage& page, const QString& js)
    {
        QVariant result;
        bool done = false;
        page.runJavaScript(js, [&](const QVariant& v) { result = v; done = true; });
        QDeadlineTimer deadline(5000);
        while (!done && !deadline.hasExpired())
            QTest::qWait(20);
        return result;
    }

private slots:
    void initTestCase()
    {
        m_profile = new QWebEngineProfile(this);
        sora::installInternalPages(m_profile);
    }
    void newTabPageLoads()
    {
        QWebEnginePage page(m_profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setUrl(QUrl("sora://newtab"));
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(loaded.first().at(0).toBool(), true);
        QCOMPARE(page.title(), QString::fromUtf8("Nueva pestaña"));
        QVERIFY(runJs(page, "document.querySelector('input[type=search]') !== null").toBool());
    }
    void unknownPageFails()
    {
        QWebEnginePage page(m_profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setUrl(QUrl("sora://no-existe"));
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(loaded.first().at(0).toBool(), false);
    }
    void webPagesCannotOpenInternalPages()
    {
        // Una web normal no puede navegar a sora:// (ni incrustarla) para leer o tocar ajustes.
        sora::Page page(m_profile); // la página real de las pestañas de Sora
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml("<p>web</p>", QUrl("https://ataque.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        runJs(page, "location.href = 'sora://settings'");
        QTest::qWait(800);
        QCOMPARE(page.url().scheme(), QString("https"));
    }
    void newTabShowsEmptyAddress()
    {
        QCOMPARE(sora::displayUrl(QUrl("sora://newtab")), QString());
        QCOMPARE(sora::displayUrl(QUrl("sora://history")), QString("sora://history"));
    }
};

int main(int argc, char** argv)
{
    sora::registerSoraScheme(); // antes de crear la aplicación, como en main.cpp
    QApplication app(argc, argv);
    TestInternalPages t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_internalpages.moc"
