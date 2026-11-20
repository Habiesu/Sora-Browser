// Pruebas con ventana real de Windows (no offscreen): estilos que da Windows a la ventana Niebla.
#include <QtTest>
#include <QWebEngineProfile>
#include <qt_windows.h>

#include "app/MainWindow.h"
#include "privacy/AdblockService.h"

class TestNativeWindow : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    sora::AdblockService m_adblock;
private slots:
    void initTestCase() { m_profile = new QWebEngineProfile(this); }
    void windowHasMinMaxAndSystemMenu()
    {
        // Sin estos estilos Windows ignora el doble clic en la barra, Win+flechas, minimizar desde
        // la barra de tareas y Alt+Espacio.
        sora::MainWindow w(m_profile, &m_adblock);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const LONG_PTR style = GetWindowLongPtr(reinterpret_cast<HWND>(w.winId()), GWL_STYLE);
        QVERIFY(style & WS_MAXIMIZEBOX);
        QVERIFY(style & WS_MINIMIZEBOX);
        QVERIFY(style & WS_SYSMENU);
        QVERIFY(style & WS_THICKFRAME);
    }
    void titleBarStaysOurs()
    {
        // Con los estilos, la barra nativa no debe volver: el área cliente sigue empezando arriba.
        sora::MainWindow w(m_profile, &m_adblock);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        RECT win, client;
        const HWND h = reinterpret_cast<HWND>(w.winId());
        GetWindowRect(h, &win);
        GetClientRect(h, &client);
        POINT origin{0, 0};
        ClientToScreen(h, &origin);
        QCOMPARE(origin.y, win.top);
    }
};

QTEST_MAIN(TestNativeWindow)
#include "tst_nativewindow.moc"
