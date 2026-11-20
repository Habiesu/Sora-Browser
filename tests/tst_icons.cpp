#include <QtTest>
#include <QIcon>
#include <QImage>
#include "ui/Icons.h"

using sora::Icon;

class TestIcons : public QObject {
    Q_OBJECT
private slots:
    void everyIconDraws_data()
    {
        QTest::addColumn<Icon>("which");
        const Icon all[] = {Icon::Back, Icon::Forward, Icon::Reload, Icon::Stop, Icon::Lock, Icon::Globe,
                            Icon::Shield, Icon::Star, Icon::Menu, Icon::Plus, Icon::Close,
                            Icon::Minimize, Icon::Maximize, Icon::Restore};
        for (Icon i : all)
            QTest::newRow(QByteArray::number(int(i)).constData()) << i;
    }
    void everyIconDraws()
    {
        QFETCH(Icon, which);
        const QImage img = sora::icon(which, Qt::black).pixmap(16, 16).toImage();
        QVERIFY(!img.isNull());
        int painted = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                painted += qAlpha(img.pixel(x, y)) > 0;
        QVERIFY(painted > 0);
        QVERIFY(painted < img.width() * img.height());
    }
    void usesRequestedColor()
    {
        const QColor green("#2b8a3e");
        const QImage img = sora::icon(Icon::Shield, green).pixmap(16, 16).toImage();
        QRgb best = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (qAlpha(img.pixel(x, y)) > qAlpha(best))
                    best = img.pixel(x, y);
        QCOMPARE(QColor(best).rgb(), green.rgb());
    }
};

QTEST_MAIN(TestIcons)
#include "tst_icons.moc"
