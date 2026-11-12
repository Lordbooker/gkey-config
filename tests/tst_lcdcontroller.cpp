// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdController.h"
#include "LcdIntro.h"
#include "ModelRegistry.h"
#include "Settings.h"

#include <QTemporaryDir>
#include <QTest>

#include <limits>
#include <memory>

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
    void pageAfterIntro();
    void settingsAfterIntro();
    void textWithHugeDuration();
    void hardwarePages();
    void defaultPages();
    void profilePageOnChange();
    void profilePageDisabled();

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
// The chosen page follows the final image of the start animation.
void TestLcdController::pageAfterIntro()
{
    auto lcd = controller({QStringLiteral("intro"), QStringLiteral("status"), QStringLiteral("clock")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));

    // A page that is not enabled does not follow: the final image stays.
    lcd->setPageAfterIntro(QStringLiteral("system"));
    const int animation = LcdIntro::frameCount() * LcdIntro::FrameMs;
    const int finalImage = LcdController::IntroEndSeconds * 1000;
    QTest::qWait(animation + finalImage + 1000);
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));

    // Chosen while the final image is shown: follows after the usual time.
    lcd->setPageAfterIntro(QStringLiteral("clock"));
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));
    QTRY_COMPARE_WITH_TIMEOUT(lcd->currentPage(), QStringLiteral("clock"), finalImage * 3);

    // Not while the animation still runs.
    lcd->showPage(QStringLiteral("intro"));
    QTest::qWait(finalImage + 500);
    QCOMPARE(lcd->currentPage(), QStringLiteral("intro"));
}

void TestLcdController::settingsAfterIntro()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    {
        Settings settings(path);
        QCOMPARE(settings.lcdAfterIntro(), QString());   // default: the final image stays
        settings.setLcdAfterIntro(QStringLiteral("clock"));
        QVERIFY(settings.save());
    }
    {
        Settings settings(path);
        QCOMPARE(settings.lcdAfterIntro(), QStringLiteral("clock"));
        // Only pages of the rotation.
        for (const QString &page : {QStringLiteral("intro"), QStringLiteral("profile"), QStringLiteral("nonsense")}) {
            settings.setLcdAfterIntro(page);
            QCOMPARE(settings.lcdAfterIntro(), QString());
        }
        QVERIFY(settings.save());
    }
    QCOMPARE(Settings(path).lcdAfterIntro(), QString());
}

// A duration from a script (D-Bus) that does not fit into the timer's
// milliseconds is limited instead of ending the text at once.
void TestLcdController::textWithHugeDuration()
{
    auto lcd = controller({QStringLiteral("status"), QStringLiteral("text")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));
    lcd->setText(QStringLiteral("hello"), std::numeric_limits<int>::max());
    QCOMPARE(lcd->currentPage(), QStringLiteral("text"));
    QTest::qWait(50);
    QCOMPARE(lcd->currentPage(), QStringLiteral("text"));
}

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

// A change of the profile shows the profile page for a few seconds; the first
// profile after connecting and a change of the layer do not.
void TestLcdController::profilePageOnChange()
{
    auto lcd = controller({QStringLiteral("profile"), QStringLiteral("status"), QStringLiteral("clock")});
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));   // not part of the rotation
    LcdPages::Status status{QStringLiteral("Logitech G13"), QStringLiteral("Default"), 1, false};
    lcd->setStatus(status);
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));
    status.mode = 2;
    lcd->setStatus(status);
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));

    lcd->showPage(QStringLiteral("clock"));
    status.profile = QStringLiteral("Raid");
    lcd->setStatus(status);
    QCOMPARE(lcd->currentPage(), QStringLiteral("profile"));
    LcdFrame expected(160, 43);
    LcdPages::renderProfile(expected, QStringLiteral("Raid"));
    QVERIFY(lcd->frame() == expected);
    QTRY_COMPARE_WITH_TIMEOUT(lcd->currentPage(), QStringLiteral("clock"), (LcdController::ProfileSeconds + 2) * 1000);
}

void TestLcdController::profilePageDisabled()
{
    auto lcd = controller({QStringLiteral("status"), QStringLiteral("clock")});
    LcdPages::Status status{QStringLiteral("Logitech G13"), QStringLiteral("Default"), 1, false};
    lcd->setStatus(status);
    status.profile = QStringLiteral("Raid");
    lcd->setStatus(status);
    QCOMPARE(lcd->currentPage(), QStringLiteral("status"));
}

QTEST_GUILESS_MAIN(TestLcdController)
#include "tst_lcdcontroller.moc"
