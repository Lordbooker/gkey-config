#include "LcdFrame.h"
#include "LcdPages.h"

#include <QTest>

class TestLcd : public QObject {
    Q_OBJECT

private slots:
    void reportLayout();
    void text();
    void ascii();
    void elide();
    void pages();
};

namespace {

int litPixels(const LcdFrame &frame)
{
    int count = 0;
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            count += frame.pixel(x, y) ? 1 : 0;
    return count;
}

}

void TestLcd::reportLayout()
{
    LcdFrame frame(160, 43);
    QByteArray report = frame.toG13Report();
    QCOMPARE(report.size(), 992);
    QCOMPARE(static_cast<unsigned char>(report.at(0)), 0x03);
    for (int i = 1; i < report.size(); ++i)
        QCOMPARE(report.at(i), '\0');

    frame.setPixel(0, 0);       // band 0, column 0, bit 0
    frame.setPixel(5, 9);       // band 1, column 5, bit 1
    frame.setPixel(159, 42);    // band 5, column 159, bit 2
    report = frame.toG13Report();
    QCOMPARE(static_cast<unsigned char>(report.at(32)), 0x01);
    QCOMPARE(static_cast<unsigned char>(report.at(32 + 160 + 5)), 0x02);
    QCOMPARE(static_cast<unsigned char>(report.at(32 + 5 * 160 + 159)), 0x04);
}

void TestLcd::text()
{
    LcdFrame frame(40, 10);
    const int end = frame.drawText(0, 0, QStringLiteral("I"));
    QCOMPARE(end, 6);
    // "I" in the 5x7 font: column 1..3 top row lit (0x41 at col 1 and 3, 0x7F at col 2)
    QVERIFY(frame.pixel(2, 0));
    QVERIFY(frame.pixel(2, 6));
    QVERIFY(!frame.pixel(0, 3));
    QCOMPARE(LcdFrame::textWidth(QStringLiteral("AB")), 11);
    QCOMPARE(LcdFrame::textWidth(QStringLiteral("AB"), 2), 22);

    LcdFrame inverted(10, 10);
    inverted.fillRect(0, 0, 10, 10);
    inverted.drawText(0, 0, QStringLiteral("I"), 1, false);
    QVERIFY(!inverted.pixel(2, 0));
    QVERIFY(inverted.pixel(0, 3));
}

void TestLcd::ascii()
{
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("Grüße")), QStringLiteral("Gruesse"));
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("Café")), QStringLiteral("Cafe"));
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("日")), QStringLiteral("?"));
}

void TestLcd::elide()
{
    const QString shown = LcdFrame::elided(QStringLiteral("A very long profile name"), 60);
    QVERIFY(LcdFrame::textWidth(shown) <= 60);
    QVERIFY(shown.endsWith(u'~'));
    QCOMPARE(LcdFrame::elided(QStringLiteral("Short"), 60), QStringLiteral("Short"));
}

void TestLcd::pages()
{
    LcdFrame frame(160, 43);
    LcdPages::renderStatus(frame, {QStringLiteral("Logitech G13"), QStringLiteral("World of Warcraft"), 2, true});
    QVERIFY(litPixels(frame) > 200);
    // Active layer box M2 is filled: its centre column area has many lit pixels.
    QVERIFY(frame.pixel(70, 34));

    LcdPages::renderClock(frame, QDateTime(QDate(2026, 10, 21), QTime(19, 48, 7)));
    QVERIFY(litPixels(frame) > 200);
    for (int x = 0; x < 160; ++x)
        QVERIFY(!frame.pixel(x, 43));   // nothing outside the 43 visible rows

    LcdPages::renderText(frame, {QStringLiteral("line 1"), QStringLiteral("line 2")});
    QVERIFY(litPixels(frame) > 20);
    LcdPages::renderText(frame, {});
    QCOMPARE(litPixels(frame), 0);
}

QTEST_GUILESS_MAIN(TestLcd)
#include "tst_lcd.moc"
