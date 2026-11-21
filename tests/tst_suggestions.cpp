#include <QtTest>

#include <QAbstractItemView>
#include <QCompleter>

#include "browser/Suggestions.h"
#include "ui/Omnibox.h"

using sora::Suggestion;

class TestSuggestions : public QObject {
    Q_OBJECT
private slots:
    void parsesDuckDuckGo()
    {
        QCOMPARE(sora::parseSearchSuggestions(R"(["lobo",["lobo gris","lobo ibérico"]])"),
                 QStringList({"lobo gris", QString::fromUtf8("lobo ibérico")}));
        QCOMPARE(sora::parseSearchSuggestions("no es json"), QStringList());
        QCOMPARE(sora::parseSearchSuggestions(R"({"a":1})"), QStringList());
    }
    void mergesInChromeOrder()
    {
        const QList<sora::OpenTab> tabs = {{0, "Lobos en YouTube", QUrl("https://www.youtube.com/lobos")},
                                           {1, "Otra cosa", QUrl("https://ejemplo.test/")}};
        const QList<sora::BookmarkStore::Bookmark> bookmarks = {{1, QUrl("https://lobopedia.test/"), "Lobopedia"}};
        QList<sora::HistoryStore::Entry> history;
        history.append({QUrl("https://lobos.test/manada"), "Manada", QDateTime::currentDateTime(), 3});
        history.append({QUrl("https://lobopedia.test/"), "Lobopedia", QDateTime::currentDateTime(), 1});

        const auto list = sora::buildSuggestions("lobo", tabs, bookmarks, history, {"lobo gris", "lobo"});
        QCOMPARE(list.first().kind, Suggestion::Search); // lo escrito, primero
        QCOMPARE(list.first().text, QString("lobo"));
        QCOMPARE(list.at(1).kind, Suggestion::Tab);
        QCOMPARE(list.at(1).tab, 0);
        QCOMPARE(list.at(2).kind, Suggestion::Bookmark);
        QCOMPARE(list.at(3).kind, Suggestion::History);
        QCOMPARE(list.at(3).text, QString("https://lobos.test/manada")); // lobopedia no se repite
        QCOMPARE(list.at(4).kind, Suggestion::Search);
        QCOMPARE(list.at(4).text, QString("lobo gris")); // "lobo" ya estaba
        QCOMPARE(list.size(), 5);
    }
    void typedAddressGoesFirstAsUrl()
    {
        const auto list = sora::buildSuggestions("lobos.test", {}, {}, {}, {});
        QCOMPARE(list.first().kind, Suggestion::Url);
    }
    void omniboxEnterPicksRowOrTypedText()
    {
        sora::Omnibox box;
        box.show();
        box.setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&box));
        QSignalSpy submitted(&box, &sora::Omnibox::submitted);
        QSignalSpy tabs(&box, &sora::Omnibox::tabRequested);
        QTest::keyClicks(&box, "lobo");
        box.setSuggestions({{Suggestion::Search, "lobo", {}}, {Suggestion::Tab, "https://lobos.test/", "Lobos", 2}});
        QTRY_VERIFY(box.completer()->popup()->isVisible());
        QTest::keyClick(box.completer()->popup(), Qt::Key_Return); // sin fila elegida: lo escrito
        QCOMPARE(submitted.count(), 1);
        QCOMPARE(submitted.first().first().toString(), QString("lobo"));

        box.setSuggestions({{Suggestion::Search, "lobo", {}}, {Suggestion::Tab, "https://lobos.test/", "Lobos", 2}});
        QTRY_VERIFY(box.completer()->popup()->isVisible());
        QTest::keyClick(box.completer()->popup(), Qt::Key_Down);
        QTest::keyClick(box.completer()->popup(), Qt::Key_Down);
        QTest::keyClick(box.completer()->popup(), Qt::Key_Return);
        QCOMPARE(tabs.count(), 1);
        QCOMPARE(tabs.first().first().toInt(), 2);
        QTest::qWait(50); // el activated de QCompleter que llega después no debe repetir nada
        QCOMPARE(submitted.count(), 1);
    }
    void inlineCompletesHosts()
    {
        QList<sora::HistoryStore::Entry> history;
        history.append({QUrl("https://www.youtube.com/watch?v=1"), "Vídeo", QDateTime::currentDateTime(), 9});
        QCOMPARE(sora::inlineCompletion("you", {}, history), QString("youtube.com"));
        QCOMPARE(sora::inlineCompletion("YouT", {}, history), QString("YouTube.com"));
        QCOMPARE(sora::inlineCompletion("youtube.com", {}, history), QString()); // ya está completo
        QCOMPARE(sora::inlineCompletion("lobo gris", {}, history), QString());
        QCOMPARE(sora::inlineCompletion("x", {}, history), QString());
    }
};

QTEST_MAIN(TestSuggestions)
#include "tst_suggestions.moc"
