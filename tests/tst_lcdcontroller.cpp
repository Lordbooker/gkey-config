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

QTEST_GUILESS_MAIN(TestLcdController)
#include "tst_lcdcontroller.moc"
