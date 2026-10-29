// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdFont.h"
#include "LcdFrame.h"
#include "LcdIntro.h"
#include "LcdIntroImages.h"
#include "LcdPages.h"

#include <QTest>

class TestLcd : public QObject {
    Q_OBJECT

private slots:
    void reportLayout();
    void text();
    void ascii();
    void asciiTwice();
    void elide();
    void pages();
    void profilePage_data();
    void profilePage();
    void introAnimation();
    void introStartsDarkWithoutFirmwareLogo();
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
    // Day names and punctuation of the supported GUI languages.
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("sábado, março")), QStringLiteral("sabado, marco"));
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("miércoles, año")), QStringLiteral("miercoles, ano"));
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("«\u00a0œuvre\u00a0» l’été")), QStringLiteral("\" oeuvre \" l'ete"));
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("1ª 2º")), QStringLiteral("1a 2o"));
}

// Converting already converted text changes nothing (drawText() converts
// again what elided() returned); the degree sign used to become another glyph.
void TestLcd::asciiTwice()
{
    const QString text = QStringLiteral("Temperatur 21\u00b0C \u00e4 \u2013 caf\u00e9");
    const QString once = LcdFrame::toAscii(text);
    QCOMPARE(LcdFrame::toAscii(once), once);

    LcdFrame direct(160, 43);
    direct.clear();
    direct.drawText(1, 1, text);
    LcdFrame viaElided(160, 43);
    viaElided.clear();
    viaElided.drawText(1, 1, LcdFrame::elided(text, 158));
    QCOMPARE(viaElided.toG13Report(), direct.toG13Report());
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

void TestLcd::profilePage_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("lines");
    QTest::addColumn<int>("scale");
    // Scale 5 (about the G13 logo) for up to 5 characters, then smaller.
    QTest::newRow("5 characters") << QStringLiteral("Raid") << QStringList{QStringLiteral("Raid")} << 5;
    QTest::newRow("6 characters") << QStringLiteral("Gaming") << QStringList{QStringLiteral("Gaming")} << 4;
    QTest::newRow("8 characters") << QStringLiteral("Shooter2") << QStringList{QStringLiteral("Shooter2")} << 3;
    QTest::newRow("13 characters") << QStringLiteral("Flight Sim XL") << QStringList{QStringLiteral("Flight Sim XL")}
                                   << 2;
    // Too long for one line in scale 2: two lines, broken at the balancing space.
    QTest::newRow("two lines") << QStringLiteral("Elite Dangerous")
                               << QStringList{QStringLiteral("Elite"), QStringLiteral("Dangerous")} << 2;
    QTest::newRow("balanced") << QStringLiteral("World of Warcraft")
                              << QStringList{QStringLiteral("World of"), QStringLiteral("Warcraft")} << 2;
    // No part fits into 13 characters: one small line, cut after 26.
    QTest::newRow("no space") << QStringLiteral("Supercalifragilistic")
                              << QStringList{QStringLiteral("Supercalifragilistic")} << 1;
    QTest::newRow("cut") << QStringLiteral("Microsoft Flight Simulator 2024")
                         << QStringList{QStringLiteral("Microsoft Flight Simulato~")} << 1;
}

// The profile page draws the expected lines, centred, in the expected scale.
void TestLcd::profilePage()
{
    QFETCH(QString, name);
    QFETCH(QStringList, lines);
    QFETCH(int, scale);
    LcdFrame frame(160, 43);
    LcdPages::renderProfile(frame, name);

    LcdFrame expected(160, 43);
    const int lineHeight = LcdFont::Height * scale;
    const int total = static_cast<int>(lines.size()) * lineHeight + (static_cast<int>(lines.size()) - 1) * 4;
    int y = (43 - total) / 2;
    for (const QString &line : lines) {
        expected.drawText((160 - LcdFrame::textWidth(line, scale)) / 2, y, line, scale);
        y += lineHeight + 4;
    }
    QVERIFY(frame == expected);
    QVERIFY(!LcdPages::rotation(LcdPages::allPages()).contains(LcdPages::ProfilePage));
}

// The start animation from linux-g13-driver: about 9 s at 20 frames per second;
// the G13 starts with its firmware logo and ends with its final image.
void TestLcd::introAnimation()
{
    QCOMPARE(LcdIntro::frameCount(), 176);
    QVERIFY(LcdPages::allPages().startsWith(LcdPages::IntroPage));
    QVERIFY(!LcdPages::rotation(LcdPages::allPages()).contains(LcdPages::IntroPage));

    const auto bit = [](const auto &bitmap, int x, int y) {
        return ((static_cast<unsigned>(bitmap[y][x / 8]) >> (7U - static_cast<unsigned>(x % 8))) & 1U) != 0;
    };
    LcdIntro intro(QStringLiteral("g13"));
    LcdFrame frame(160, 43);
    intro.step();
    intro.render(frame);
    for (int y = 0; y < 43; ++y)
        for (int x = 0; x < 160; ++x)
            QCOMPARE(frame.pixel(x, y), bit(LcdIntroImages::g13Firmware, x, y));

    LcdIntro again(QStringLiteral("g13"));   // same seed: the same frames
    LcdFrame other(160, 43);
    while (!intro.finished()) {
        intro.step();
        again.step();
        if (intro.frame() == 100) {
            intro.render(frame);
            again.render(other);
            QCOMPARE(frame.toG13Report(), other.toG13Report());
        }
    }
    intro.render(frame);
    for (int y = 0; y < 43; ++y)
        for (int x = 0; x < 160; ++x)
            QCOMPARE(frame.pixel(x, y), bit(LcdIntroImages::g13Final, x, y));
}

void TestLcd::introStartsDarkWithoutFirmwareLogo()
{
    const auto bit = [](const auto &bitmap, int x, int y) {
        return ((static_cast<unsigned>(bitmap[y][x / 8]) >> (7U - static_cast<unsigned>(x % 8))) & 1U) != 0;
    };
    LcdIntro intro(QStringLiteral("g510"));
    LcdFrame frame(160, 43);
    intro.step();
    intro.render(frame);
    QCOMPARE(litPixels(frame), 160 * 43);   // dark display
    while (!intro.finished())
        intro.step();
    intro.render(frame);
    QCOMPARE(frame.pixel(100, 20), bit(LcdIntroImages::g510Final, 100, 20));
    bool sameAsG510 = true;
    for (int y = 0; y < 43; ++y)
        for (int x = 0; x < 160; ++x)
            sameAsG510 = sameAsG510 && frame.pixel(x, y) == bit(LcdIntroImages::g510Final, x, y);
    QVERIFY(sameAsG510);
}

QTEST_GUILESS_MAIN(TestLcd)
#include "tst_lcd.moc"
