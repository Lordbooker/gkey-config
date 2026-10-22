// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// System values for the LCD page "system", read from fake /proc and /sys trees,
// and the page itself.

#include "LcdFont.h"
#include "LcdFrame.h"
#include "LcdPages.h"
#include "SystemStats.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstring>
#include <memory>

class TestSystemStats : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cpuLoad();
    void memory();
    void amdGpuAndTemperatures();
    void withoutGpu();
    void formatSize();
    void degreeSign();
    void renderFourRows();
    void renderThreeRows();
    void realSystem();

private:
    void write(const QString &path, const QByteArray &content);
    QString proc() const { return m_dir->filePath(QStringLiteral("proc")); }
    QString sys() const { return m_dir->filePath(QStringLiteral("sys")); }
    void addAmdGpu();

    std::unique_ptr<QTemporaryDir> m_dir;
};

void TestSystemStats::write(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

void TestSystemStats::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    write(proc() + QStringLiteral("/stat"), "cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 1 2 3 4\n");
    write(proc() + QStringLiteral("/meminfo"),
          "MemTotal:       32000000 kB\nMemFree:         1000000 kB\nMemAvailable:   16000000 kB\n"
          "SwapTotal:       8000000 kB\nSwapFree:        6000000 kB\n");
    // CPU sensor (AMD) and other hwmon devices
    write(sys() + QStringLiteral("/class/hwmon/hwmon0/name"), "nvme\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon0/temp1_input"), "43850\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon4/name"), "k10temp\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon4/temp1_label"), "Tctl\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon4/temp1_input"), "47750\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon4/temp3_label"), "Tccd1\n");
    write(sys() + QStringLiteral("/class/hwmon/hwmon4/temp3_input"), "31750\n");
}

void TestSystemStats::addAmdGpu()
{
    // Integrated graphics (small) and graphics card (large); a connector entry to skip.
    const QString igpu = sys() + QStringLiteral("/class/drm/card0/device");
    write(igpu + QStringLiteral("/gpu_busy_percent"), "3\n");
    write(igpu + QStringLiteral("/mem_info_vram_total"), "536870912\n");
    write(igpu + QStringLiteral("/mem_info_vram_used"), "100000000\n");
    const QString card = sys() + QStringLiteral("/class/drm/card1/device");
    write(card + QStringLiteral("/gpu_busy_percent"), "84\n");
    write(card + QStringLiteral("/mem_info_vram_total"), "25753026560\n");
    write(card + QStringLiteral("/mem_info_vram_used"), "16226447360\n");
    write(card + QStringLiteral("/hwmon/hwmon1/name"), "amdgpu\n");
    write(card + QStringLiteral("/hwmon/hwmon1/temp1_label"), "edge\n");
    write(card + QStringLiteral("/hwmon/hwmon1/temp1_input"), "72000\n");
    write(card + QStringLiteral("/hwmon/hwmon1/temp2_label"), "junction\n");
    write(card + QStringLiteral("/hwmon/hwmon1/temp2_input"), "80000\n");
    QVERIFY(QDir().mkpath(sys() + QStringLiteral("/class/drm/card1-DP-1")));
}

void TestSystemStats::cpuLoad()
{
    SystemStats stats(proc(), sys());
    QVERIFY(!stats.sample().cpuPercent);   // first sample: no previous one
    // +300 busy (user, system) and +100 idle: 75 %
    write(proc() + QStringLiteral("/stat"), "cpu  250 0 250 900 0 0 0 0 0 0\n");
    QCOMPARE(stats.sample().cpuPercent, std::optional<int>(75));
    // nothing happened: no value instead of a division by zero
    QVERIFY(!stats.sample().cpuPercent);
}

void TestSystemStats::memory()
{
    SystemStats stats(proc(), sys());
    const auto sample = stats.sample();
    QVERIFY(sample.ram);
    QCOMPARE(sample.ram->used, qint64(16000000) * 1024);
    QCOMPARE(sample.ram->percent(), 50);
    QVERIFY(sample.swap);
    QCOMPARE(sample.swap->percent(), 25);
}

void TestSystemStats::amdGpuAndTemperatures()
{
    addAmdGpu();
    SystemStats stats(proc(), sys());
    const auto sample = stats.sample();
    QCOMPARE(sample.cpuTemperature, std::optional<double>(47.75));   // Tctl, not Tccd1 or nvme
    QVERIFY(sample.gpu);
    QCOMPARE(sample.gpu->busyPercent, std::optional<int>(84));       // the card, not the iGPU
    QCOMPARE(sample.gpu->temperature, std::optional<double>(72.0));  // edge
    QVERIFY(sample.gpu->vram);
    QCOMPARE(sample.gpu->vram->percent(), 63);
}

void TestSystemStats::withoutGpu()
{
    SystemStats stats(proc(), sys());
    QVERIFY(!stats.sample().gpu);
}

void TestSystemStats::formatSize()
{
    constexpr qint64 GiB = 1024LL * 1024 * 1024;
    QCOMPARE(LcdPages::formatSize(16226447360), QStringLiteral("15.1G"));
    QCOMPARE(LcdPages::formatSize(GiB / 2), QStringLiteral("0.5G"));
    QCOMPARE(LcdPages::formatSize(128 * GiB), QStringLiteral("128G"));
}

void TestSystemStats::degreeSign()
{
    QCOMPARE(LcdFrame::toAscii(QStringLiteral("61°C")),
             QStringLiteral("61") + QChar(char16_t(LcdFont::DegreeSign)) + QStringLiteral("C"));
    const unsigned char *degree = LcdFont::glyph(LcdFont::DegreeSign);
    const unsigned char *question = LcdFont::glyph(U'?');
    QVERIFY(std::memcmp(degree, question, LcdFont::Width) != 0);
}

void TestSystemStats::renderFourRows()
{
    addAmdGpu();
    SystemStats stats(proc(), sys());
    stats.sample();
    write(proc() + QStringLiteral("/stat"), "cpu  250 0 250 900 0 0 0 0 0 0\n");
    LcdFrame frame(160, 43);
    LcdPages::renderSystem(frame, stats.sample());
    // Four bars 11 px apart; bar frame at x = 22, filled part inside.
    for (int row = 0; row < 4; ++row) {
        QVERIFY2(frame.pixel(22, row * 11 + 4), qPrintable(QString::number(row)));
        QVERIFY(frame.pixel(24, row * 11 + 4));      // every value > 0 %
    }
    QVERIFY(frame.pixel(75, 11 * 2 + 4));   // GPU 84 %: filled up to x = 79
    QVERIFY(!frame.pixel(75, 11 + 4));      // RAM 50 %: filled up to x = 56
}

void TestSystemStats::renderThreeRows()
{
    SystemStats stats(proc(), sys());
    LcdFrame frame(160, 43);
    LcdPages::renderSystem(frame, stats.sample());   // CPU without value, RAM, swap
    QVERIFY(frame.pixel(22, 2 + 2 * 14 + 4));      // third bar (swap) 14 px apart
    QVERIFY(!frame.pixel(24, 2 + 4));              // CPU: no value yet, empty bar
}

// The real /proc behaves differently from regular files (size 0): RAM and
// CPU load must be readable there.
void TestSystemStats::realSystem()
{
    if (!QFile::exists(QStringLiteral("/proc/meminfo")))
        QSKIP("no /proc");
    SystemStats stats;
    const auto first = stats.sample();
    QVERIFY(first.ram);
    QVERIFY(first.ram->total > 0);
    QVERIFY(first.ram->used > 0);
    QTest::qWait(100);
    QVERIFY(stats.sample().cpuPercent);
}

QTEST_GUILESS_MAIN(TestSystemStats)
#include "tst_systemstats.moc"
