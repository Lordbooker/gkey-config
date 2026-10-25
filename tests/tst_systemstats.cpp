// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// System values for the LCD page "system", read from fake /proc and /sys trees,
// and the page itself.

#include "LcdFont.h"
#include "LcdFrame.h"
#include "LcdPages.h"
#include "NvidiaStats.h"
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
    void cpuDetail();
    void amdGpuDetail();
    void nvidiaWithoutDriver();
    void renderCpuBars_data();
    void renderCpuBars();
    void renderCpuTemperatures();
    void renderGpu();
    void renderGpuMissingValues();
    void renderGpuWithoutGpu();
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

// 9800X3D-like: 2 physical cores with 2 threads each in the fake tree; one
// clock per thread, in the order of the CPU numbers (cpufreq in kHz).
void TestSystemStats::cpuDetail()
{
    const QString cpu = sys() + QStringLiteral("/devices/system/cpu/cpu%1");
    const int coreIds[] = {0, 1, 0, 1};
    const char *freqs[] = {"4111719\n", "603379\n", "5100000\n", "1200000\n"};
    for (int i = 0; i < 4; ++i) {
        write(cpu.arg(i) + QStringLiteral("/topology/physical_package_id"), "0\n");
        write(cpu.arg(i) + QStringLiteral("/topology/core_id"), QByteArray::number(coreIds[i]) + '\n');
        write(cpu.arg(i) + QStringLiteral("/cpufreq/scaling_cur_freq"), freqs[i]);
        write(cpu.arg(i) + QStringLiteral("/cpufreq/cpuinfo_min_freq"), "603379\n");
        write(cpu.arg(i) + QStringLiteral("/cpufreq/cpuinfo_max_freq"), "5271622\n");
    }
    QVERIFY(QDir().mkpath(sys() + QStringLiteral("/devices/system/cpu/cpufreq")));   // not a CPU

    SystemStats stats(proc(), sys());
    const SystemStats::CpuDetail detail = stats.cpuDetail();
    QCOMPARE(detail.cpuMHz, (QList<int>{4111, 603, 5100, 1200}));
    QCOMPARE(detail.minMHz, 603);
    QCOMPARE(detail.maxMHz, 5271);
    QCOMPARE(detail.temperatures.size(), 2);
    QCOMPARE(detail.temperatures.at(0).label, QStringLiteral("Tctl"));
    QCOMPARE(detail.temperatures.at(1).label, QStringLiteral("Tccd1"));
    QCOMPARE(detail.temperatures.at(1).celsius, 31.75);
    QVERIFY(!detail.percent);   // first call
}

// Values as on an RX 7900 XTX (amdgpu hwmon: clocks in Hz, power in µW).
void TestSystemStats::amdGpuDetail()
{
    SystemStats none(proc(), sys());
    QVERIFY(!none.amdGpuDetail());

    addAmdGpu();
    const QString hwmon = sys() + QStringLiteral("/class/drm/card1/device/hwmon/hwmon1");
    write(sys() + QStringLiteral("/class/drm/card1/device/mem_busy_percent"), "12\n");
    write(hwmon + QStringLiteral("/temp3_label"), "mem\n");
    write(hwmon + QStringLiteral("/temp3_input"), "84000\n");
    write(hwmon + QStringLiteral("/freq1_label"), "sclk\n");
    write(hwmon + QStringLiteral("/freq1_input"), "2650000000\n");
    write(hwmon + QStringLiteral("/freq2_label"), "mclk\n");
    write(hwmon + QStringLiteral("/freq2_input"), "1249000000\n");
    write(hwmon + QStringLiteral("/power1_label"), "PPT\n");
    write(hwmon + QStringLiteral("/power1_average"), "312000000\n");
    write(hwmon + QStringLiteral("/fan1_input"), "1450\n");

    SystemStats stats(proc(), sys());
    const auto detail = stats.amdGpuDetail();
    QVERIFY(detail);
    QCOMPARE(detail->busyPercent, std::optional<int>(84));
    QCOMPARE(detail->memBusyPercent, std::optional<int>(12));
    QCOMPARE(detail->coreMHz, std::optional<int>(2650));
    QCOMPARE(detail->memMHz, std::optional<int>(1249));
    QCOMPARE(detail->temperatures.size(), 3);
    QCOMPARE(detail->temperatures.at(1).label, QStringLiteral("junction"));
    QCOMPARE(detail->temperatures.at(2).celsius, 84.0);
    QCOMPARE(detail->powerWatts, std::optional<double>(312.0));
    QCOMPARE(detail->fanRpm, std::optional<int>(1450));
    QVERIFY(detail->vram);
}

// Without the NVIDIA driver there is no sample and a reason for the page.
void TestSystemStats::nvidiaWithoutDriver()
{
    NvidiaStats nvidia(QStringLiteral("gkey-config-no-such-library"));
    QVERIFY(!nvidia.sample());
    QVERIFY(!nvidia.problem().isEmpty());
    QVERIFY(!nvidia.sample());   // tried once, no second attempt that could log again
}

void TestSystemStats::renderCpuBars_data()
{
    QTest::addColumn<int>("cores");
    QTest::newRow("8 cores") << 8;
    QTest::newRow("16 cores") << 16;
    QTest::newRow("64 cores") << 64;
    QTest::newRow("200 cores") << 200;   // more cores than columns: 1-pixel bars
}

// One bar per core, all inside the frame; a core at the highest clock fills
// the whole height, one at the lowest is a single row.
void TestSystemStats::renderCpuBars()
{
    QFETCH(int, cores);
    SystemStats::CpuDetail cpu;
    cpu.percent = 23;
    cpu.minMHz = 600;
    cpu.maxMHz = 5300;
    for (int i = 0; i < cores; ++i)
        cpu.cpuMHz.append(i % 2 ? 600 : 5300);
    LcdFrame frame(160, 43);
    LcdPages::renderCpu(frame, cpu);

    // Count the bars along the bottom row (runs of lit pixels right of the labels).
    int tall = 0;
    int low = 0;
    bool inBar = false;
    for (int x = 28; x < 160; ++x) {
        const bool lit = frame.pixel(x, 42);
        if (lit && !inBar)
            (frame.pixel(x, 10) ? tall : low) += 1;
        inBar = lit;
    }
    // Nothing between the clock labels and the bars.
    for (int y = 10; y < 43; ++y)
        for (int x = 25; x < 28; ++x)
            QVERIFY2(!frame.pixel(x, y), qPrintable(QStringLiteral("pixel %1,%2").arg(x).arg(y)));
    if (cores <= 130 / 3) {   // with gaps every bar is a separate run
        QCOMPARE(tall + low, cores);
        QCOMPARE(tall, cores / 2);
    } else {
        QVERIFY(tall + low >= 1);
    }
}

void TestSystemStats::renderCpuTemperatures()
{
    SystemStats::CpuDetail cpu;
    cpu.percent = 5;
    cpu.minMHz = 600;
    cpu.maxMHz = 5300;
    cpu.cpuMHz = {4000};
    cpu.temperatures = {{QStringLiteral("Tctl"), 44.5}, {QStringLiteral("Tccd1"), 28.6}};
    LcdFrame expected(160, 43);
    expected.clear();
    expected.drawText(1, 1, QStringLiteral("CPU 5% Tctl 45\u00b0 CCD1 29\u00b0"));
    LcdFrame frame(160, 43);
    LcdPages::renderCpu(frame, cpu);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 160; ++x)
            QCOMPARE(frame.pixel(x, y), expected.pixel(x, y));

    // More sensors: combined after the first one; without its name if needed.
    cpu.temperatures = {{QStringLiteral("Tctl"), 70}, {QStringLiteral("Tccd1"), 61}, {QStringLiteral("Tccd2"), 63},
                        {QStringLiteral("Tccd3"), 60}};
    cpu.percent = 100;
    LcdPages::renderCpu(frame, cpu);
    expected.clear();
    expected.drawText(1, 1, QStringLiteral("CPU 100% 70\u00b0 61/63/60\u00b0"));
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 160; ++x)
            QCOMPARE(frame.pixel(x, y), expected.pixel(x, y));

    cpu.percent = 7;   // shorter load: the first sensor keeps its name
    LcdPages::renderCpu(frame, cpu);
    expected.clear();
    expected.drawText(1, 1, QStringLiteral("CPU 7% Tctl 70\u00b0 61/63/60\u00b0"));
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 160; ++x)
            QCOMPARE(frame.pixel(x, y), expected.pixel(x, y));
}

namespace {

// Rows y0..y0+height of both frames are the same.
bool sameRows(const LcdFrame &a, const LcdFrame &b, int y0, int height)
{
    for (int y = y0; y < y0 + height; ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.pixel(x, y) != b.pixel(x, y))
                return false;
    return true;
}

}

void TestSystemStats::renderGpu()
{
    SystemStats::GpuDetail gpu;
    gpu.busyPercent = 50;
    gpu.coreMHz = 2650;
    gpu.memMHz = 1249;
    gpu.temperatures = {{QStringLiteral("edge"), 62.4}, {QStringLiteral("junction"), 78}, {QStringLiteral("mem"), 84}};
    gpu.vram = SystemStats::Memory{qint64(12) << 30, qint64(24) << 30};
    gpu.powerWatts = 312.4;
    gpu.fanRpm = 1450;
    LcdFrame frame(160, 43);
    LcdPages::renderGpu(frame, QStringLiteral("AMD"), gpu, QString());

    LcdFrame expected(160, 43);
    expected.drawText(1, 1, QStringLiteral("AMD"));
    expected.drawRect(22, 0, 108, 8);
    expected.fillRect(23, 1, 53, 6);
    expected.drawText(160 - LcdFrame::textWidth(QStringLiteral("50%")) - 1, 1, QStringLiteral("50%"));
    expected.drawText(1, 10, QStringLiteral("Core 2650 Mem 1249 MHz"));
    expected.drawText(1, 19, QStringLiteral("Edge 62\u00b0 Junc 78\u00b0 Mem 84\u00b0"));
    const QString vram = QStringLiteral("12.0/24.0G");
    expected.drawText(1, 28, QStringLiteral("VRM"));
    const int vramWidth = 160 - 22 - LcdFrame::textWidth(vram) - 5;
    expected.drawRect(22, 27, vramWidth, 8);
    expected.fillRect(23, 28, (vramWidth - 2) / 2, 6);
    expected.drawText(160 - LcdFrame::textWidth(vram) - 1, 28, vram);
    expected.drawText(1, 36, QStringLiteral("312 W"));
    const QString fan = QStringLiteral("Fan 1450 rpm");
    expected.drawText(160 - LcdFrame::textWidth(fan) - 1, 36, fan);
    QVERIFY(sameRows(frame, expected, 0, 43));
}

// NVIDIA: one temperature, fan in percent; values the driver does not report
// are "--" or left out.
void TestSystemStats::renderGpuMissingValues()
{
    SystemStats::GpuDetail gpu;
    gpu.temperatures = {{QStringLiteral("GPU"), 55}};
    gpu.fanPercent = 40;
    LcdFrame frame(160, 43);
    LcdPages::renderGpu(frame, QStringLiteral("NV"), gpu, QString());

    LcdFrame expected(160, 43);
    expected.drawText(1, 1, QStringLiteral("NV"));
    expected.drawRect(22, 0, 108, 8);
    expected.drawText(160 - LcdFrame::textWidth(QStringLiteral("--%")) - 1, 1, QStringLiteral("--%"));
    expected.drawText(1, 10, QStringLiteral("Core -- Mem -- MHz"));
    expected.drawText(1, 19, QStringLiteral("GPU 55\u00b0"));
    const QString fan = QStringLiteral("Fan 40%");
    expected.drawText(160 - LcdFrame::textWidth(fan) - 1, 36, fan);
    QVERIFY(sameRows(frame, expected, 0, 43));

    // Too many sensors for the names: only the values.
    gpu.temperatures = {{QStringLiteral("edge"), 62}, {QStringLiteral("junction"), 78}, {QStringLiteral("mem"), 84},
                        {QStringLiteral("hotspot"), 90}, {QStringLiteral("vrm"), 70}};
    LcdPages::renderGpu(frame, QStringLiteral("NV"), gpu, QString());
    expected.clear();
    expected.drawText(1, 19, QStringLiteral("62/78/84/90/70\u00b0"));
    QVERIFY(sameRows(frame, expected, 19, 7));
}

void TestSystemStats::renderGpuWithoutGpu()
{
    LcdFrame frame(160, 43);
    LcdPages::renderGpu(frame, QStringLiteral("NV"), std::nullopt, QStringLiteral("no NVML"));
    LcdFrame expected(160, 43);
    expected.drawText(1, 1, QStringLiteral("NV"));
    expected.drawText((160 - LcdFrame::textWidth(QStringLiteral("no NVML"))) / 2, 20, QStringLiteral("no NVML"));
    QVERIFY(sameRows(frame, expected, 0, 43));
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
