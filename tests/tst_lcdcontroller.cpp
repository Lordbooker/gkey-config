// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdController.h"
#include "LcdIntro.h"
#include "ModelRegistry.h"

#include <QTemporaryDir>
#include <QTest>

// Page logic of the LCD with the start animation. The device has no hidraw
// node here, so nothing is written; only the pages are checked.
class TestLcdController : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void introFirstAndNotInRotation();
    void introDisabled();
    void introRemovedWhileShown();
    void textInterruptsIntro();
    void hardwarePages();
    void defaultPages();

private:
    std::unique_ptr<LcdController> controller(const QStringList &pages);

    ModelRegistry m_registry;
    QTemporaryDir m_dir;
};

void TestLcdController::initTestCase()
{
    m_registry.load({QStringLiteral(GKEY_SOURCE_MODEL_DIR)});
    QVERIFY(m_registry.findById(QStringLiteral("g13")));
}

std::unique_ptr<LcdController> TestLcdController::controller(const QStringList &pages)
{
    auto lcd = std::make_unique<LcdController>(*m_registry.findById(QStringLiteral("g13")), m_dir.path());
    lcd->setPages(pages);
    lcd->start();
    return lcd;
}

void TestLcdController::introFirstAndNotInRotation()
{
    auto lcd = controller({QStringLiteral("intro"), QStringLiteral("status"), QStringLiteral("clock")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));

    // The animation runs and its final image stays until the page is changed.
    QTest::qWait(LcdIntro::FrameMs * 5);
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));

    lcd->showPage(QStringLiteral("next"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));
    lcd->showPage(QStringLiteral("next"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("clock"));
    lcd->showPage(QStringLiteral("next"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));   // not back to the animation
    lcd->showPage(QStringLiteral("previous"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("clock"));

    lcd->showPage(QStringLiteral("intro"));   // explicitly (D-Bus, GUI): plays again
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));
    lcd->showPage(QStringLiteral("previous"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("clock"));
}

void TestLcdController::introDisabled()
{
    auto lcd = controller({QStringLiteral("clock"), QStringLiteral("status")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("clock"));
}

void TestLcdController::introRemovedWhileShown()
{
    auto lcd = controller({QStringLiteral("intro"), QStringLiteral("system")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));
    lcd->setPages({QStringLiteral("system")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("system"));
}

void TestLcdController::textInterruptsIntro()
{
    auto lcd = controller({QStringLiteral("intro"), QStringLiteral("status")});
    lcd->setText(QStringLiteral("hello"), 1);
    QCOMPARE(lcd->currentPage(), QStringLiteral("text"));
    // After the temporary text: back to the animation, not to the next page.
    QTRY_COMPARE_WITH_TIMEOUT(lcd->currentPage(), QStringLiteral("intro"), 3000);
}

// CPU and GPU pages are in the rotation and show their title; the GPU pages
// show it with and without that GPU (the test machine may have neither).
void TestLcdController::hardwarePages()
{
    auto lcd = controller({QStringLiteral("cpu"), QStringLiteral("amdgpu"), QStringLiteral("nvidia")});
    const auto titleShown = [&lcd](const QString &title) {
        LcdFrame expected(160, 43);
        expected.drawText(1, 1, title);
        for (int y = 1; y < 8; ++y)
            for (int x = 0; x < LcdFrame::textWidth(title) + 2; ++x)
                if (lcd->frame().pixel(x, y) != expected.pixel(x, y))
                    return false;
        return true;
    };
    QCOMPARE(lcd->currentPage(), QStringLiteral("cpu"));
    QVERIFY(titleShown(QStringLiteral("CPU")));
    lcd->showPage(QStringLiteral("next"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("amdgpu"));
    QVERIFY(titleShown(QStringLiteral("AMD")));
    lcd->showPage(QStringLiteral("next"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("nvidia"));
    QVERIFY(titleShown(QStringLiteral("NV")));
}

// Without settings the hardware pages are off: they are only useful with that
// hardware.
void TestLcdController::defaultPages()
{
    for (const QString &page : {QStringLiteral("cpu"), QStringLiteral("amdgpu"), QStringLiteral("nvidia")}) {
        QVERIFY(LcdPages::allPages().contains(page));
        QVERIFY(!LcdPages::defaultPages().contains(page));
    }
    auto lcd = controller({});
    for (int i = 0; i < LcdPages::allPages().size(); ++i) {
        QVERIFY(LcdPages::defaultPages().contains(lcd->currentPage()));
        lcd->showPage(QStringLiteral("next"));
    }
}

QTEST_GUILESS_MAIN(TestLcdController)
#include "tst_lcdcontroller.moc"
