#include <QtTest>
#include <QLabel>
#include <QToolButton>

#include "ui/MainMenu.h"

using sora::MainMenu;
using Command = sora::MainMenu::Command;

static QStringList visibleTexts(const QMenu* menu)
{
    QStringList out;
    for (QAction* a : menu->actions())
        out << (a->isSeparator() ? QStringLiteral("—") : a->text());
    return out;
}

class TestMainMenu : public QObject {
    Q_OBJECT
private slots:
    void hasChromeStructure()
    {
        MainMenu menu;
        const QStringList texts = visibleTexts(&menu);
        const QStringList expected = {
            QString::fromUtf8("Nueva pestaña"), "Nueva ventana", QString::fromUtf8("Nueva ventana de incógnito"), "—",
            "Historial", "Descargas", "Marcadores", "—",
            "Zoom", "—",
            "Imprimir...", "Buscar...", QString::fromUtf8("Más herramientas"), "—",
            "Extensiones", QString::fromUtf8("Configuración"), "Ayuda", "—",
            "Salir"};
        QCOMPARE(texts, expected);
    }
    void showsChromeShortcuts()
    {
        MainMenu menu;
        const auto shortcutOf = [&](const QString& text) {
            for (QAction* a : menu.findChildren<QAction*>())
                if (a->text() == text)
                    return a->shortcut().toString();
            return QString("?");
        };
        QCOMPARE(shortcutOf(QString::fromUtf8("Nueva pestaña")), QString("Ctrl+T"));
        QCOMPARE(shortcutOf(QString::fromUtf8("Nueva ventana de incógnito")), QString("Ctrl+Shift+N"));
        QCOMPARE(shortcutOf("Historial"), QString("Ctrl+H"));
        QCOMPARE(shortcutOf("Descargas"), QString("Ctrl+J"));
        QCOMPARE(shortcutOf("Buscar..."), QString("Ctrl+F"));
        QCOMPARE(shortcutOf(QString::fromUtf8("Añadir esta página a marcadores...")), QString("Ctrl+D"));
        QCOMPARE(shortcutOf("Herramientas para desarrolladores"), QString("Ctrl+Shift+I"));
    }
    void actionsSendTheirCommand()
    {
        MainMenu menu;
        QSignalSpy spy(&menu, &MainMenu::command);
        for (QAction* a : menu.findChildren<QAction*>())
            if (a->text() == QString::fromUtf8("Nueva ventana de incógnito"))
                a->trigger();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<Command>(), Command::NewIncognitoWindow);
    }
    void zoomRowShowsAndChangesLevel()
    {
        MainMenu menu;
        QSignalSpy spy(&menu, &MainMenu::command);
        menu.setZoomPercent(125);
        auto* label = menu.findChild<QLabel*>("ZoomLevel");
        QVERIFY(label);
        QCOMPARE(label->text(), QString("125 %"));
        QTest::mouseClick(menu.findChild<QToolButton*>("ZoomIn"), Qt::LeftButton);
        QTest::mouseClick(menu.findChild<QToolButton*>("ZoomOut"), Qt::LeftButton);
        QTest::mouseClick(menu.findChild<QToolButton*>("FullScreen"), Qt::LeftButton);
        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).value<Command>(), Command::ZoomIn);
        QCOMPARE(spy.at(1).at(0).value<Command>(), Command::ZoomOut);
        QCOMPARE(spy.at(2).at(0).value<Command>(), Command::FullScreen);
    }
};

QTEST_MAIN(TestMainMenu)
#include "tst_mainmenu.moc"
