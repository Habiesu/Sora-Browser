#include <QtTest>
#include <cmath>
#include "ui/Theme.h"

using sora::nieblaPalette;

// Luminancia relativa y contraste según WCAG 2.x.
static double luminance(const QColor& c)
{
    auto ch = [](double v) { v /= 255.0; return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
}
static double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void lightMatchesSketch()
    {
        const auto p = nieblaPalette(Qt::ColorScheme::Light);
        QCOMPARE(p.window, QColor("#e9edf3"));
        QCOMPARE(p.surface, QColor("#f8fafc"));
        QCOMPARE(p.text, QColor("#1c2330"));
        QCOMPARE(p.shield, QColor("#2b8a3e"));
        QCOMPARE(p.closeHover, QColor("#c42b1c"));
    }
    void unknownIsLight() { QCOMPARE(nieblaPalette(Qt::ColorScheme::Unknown).window, QColor("#e9edf3")); }
    void darkIsDark() { QVERIFY(luminance(nieblaPalette(Qt::ColorScheme::Dark).window) < 0.05); }
    void textIsReadable_data()
    {
        QTest::addColumn<Qt::ColorScheme>("scheme");
        QTest::newRow("claro") << Qt::ColorScheme::Light;
        QTest::newRow("oscuro") << Qt::ColorScheme::Dark;
    }
    void textIsReadable()
    {
        QFETCH(Qt::ColorScheme, scheme);
        const auto p = nieblaPalette(scheme);
        QVERIFY(contrast(p.text, p.surface) >= 7);
        QVERIFY(contrast(p.text, p.window) >= 7);
        QVERIFY(contrast(p.mutedText, p.window) >= 4.5);
    }
    void styleSheetUsesPalette()
    {
        const QString light = sora::nieblaStyleSheet(nieblaPalette(Qt::ColorScheme::Light));
        QVERIFY(light.contains("#e9edf3"));
        QVERIFY(light.contains("#c42b1c"));
        QVERIFY(sora::nieblaStyleSheet(nieblaPalette(Qt::ColorScheme::Dark)).contains("#1b1f27"));
    }
};

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
