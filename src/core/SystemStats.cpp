// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "SystemStats.h"

#include <QDir>
#include <QFile>
#include <QHash>

#include <algorithm>

namespace {

QString readLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromLatin1(file.readLine()).trimmed();
}

std::optional<qint64> readNumber(const QString &path)
{
    bool ok = false;
    const qint64 value = readLine(path).toLongLong(&ok);
    return ok ? std::optional<qint64>(value) : std::nullopt;
}

std::optional<double> readMilliCelsius(const QString &path)
{
    if (path.isEmpty())
        return std::nullopt;
    const auto value = readNumber(path);
    return value ? std::optional<double>(*value / 1000.0) : std::nullopt;
}

// temp*_input of a hwmon directory whose label is one of the preferred ones,
// else temp1_input.
QString temperatureFile(const QString &hwmon, const QStringList &preferredLabels)
{
    const QDir dir(hwmon);
    for (const QString &label : preferredLabels) {
        for (const QString &entry : dir.entryList({QStringLiteral("temp*_label")}, QDir::Files)) {
            if (readLine(dir.filePath(entry)) == label)
                return dir.filePath(entry.chopped(6) + QStringLiteral("_input"));
        }
    }
    const QString first = dir.filePath(QStringLiteral("temp1_input"));
    return QFile::exists(first) ? first : QString();
}

}

SystemStats::SystemStats(const QString &procRoot, const QString &sysRoot)
    : m_procRoot(procRoot), m_sysRoot(sysRoot)
{
}

void SystemStats::findSensors()
{
    m_sensorsSearched = true;

    // CPU package temperature: AMD (k10temp, zenpower) or Intel (coretemp).
    static const QHash<QString, QStringList> cpuSensors = {
        {QStringLiteral("k10temp"), {QStringLiteral("Tctl"), QStringLiteral("Tdie")}},
        {QStringLiteral("zenpower"), {QStringLiteral("Tdie"), QStringLiteral("Tctl")}},
        {QStringLiteral("coretemp"), {QStringLiteral("Package id 0")}},
    };
    const QDir hwmonDir(m_sysRoot + QStringLiteral("/class/hwmon"));
    for (const QString &entry : hwmonDir.entryList({QStringLiteral("hwmon*")}, QDir::Dirs | QDir::System)) {
        const QString path = hwmonDir.filePath(entry);
        const auto sensor = cpuSensors.constFind(readLine(path + QStringLiteral("/name")));
        if (sensor != cpuSensors.constEnd()) {
            m_cpuTempFile = temperatureFile(path, sensor.value());
            if (!m_cpuTempFile.isEmpty())
                break;
        }
    }

    // GPU: amdgpu card with the most VRAM (a graphics card before the
    // graphics of the processor).
    qint64 largestVram = -1;
    const QDir drmDir(m_sysRoot + QStringLiteral("/class/drm"));
    for (const QString &entry : drmDir.entryList({QStringLiteral("card*")}, QDir::Dirs | QDir::System)) {
        if (entry.contains(u'-'))
            continue;   // connectors such as card1-DP-1
        const QString device = drmDir.filePath(entry) + QStringLiteral("/device");
        if (!QFile::exists(device + QStringLiteral("/gpu_busy_percent")))
            continue;
        const qint64 vram = readNumber(device + QStringLiteral("/mem_info_vram_total")).value_or(0);
        if (vram > largestVram) {
            largestVram = vram;
            m_gpuDevice = device;
        }
    }
    if (!m_gpuDevice.isEmpty()) {
        const QDir hwmon(m_gpuDevice + QStringLiteral("/hwmon"));
        const QStringList entries = hwmon.entryList({QStringLiteral("hwmon*")}, QDir::Dirs | QDir::System);
        if (!entries.isEmpty())
            m_gpuTempFile = temperatureFile(hwmon.filePath(entries.first()), {QStringLiteral("edge")});
    }
}

std::optional<int> SystemStats::cpuPercent()
{
    // First line of /proc/stat: "cpu user nice system idle iowait irq softirq steal ..."
    const QStringList fields = readLine(m_procRoot + QStringLiteral("/stat")).split(u' ', Qt::SkipEmptyParts);
    if (fields.size() < 5 || fields.first() != u"cpu")
        return std::nullopt;
    qint64 total = 0;
    for (qsizetype i = 1; i < qMin<qsizetype>(fields.size(), 9); ++i)   // without guest times (already in user)
        total += fields.at(i).toLongLong();
    const qint64 idle = fields.at(4).toLongLong() + (fields.size() > 5 ? fields.at(5).toLongLong() : 0);

    std::optional<int> percent;
    if (m_lastTotal >= 0 && total > m_lastTotal) {
        const qint64 busy = (total - m_lastTotal) - (idle - m_lastIdle);
        percent = int(std::clamp<qint64>((busy * 100 + (total - m_lastTotal) / 2) / (total - m_lastTotal), 0, 100));
    }
    m_lastTotal = total;
    m_lastIdle = idle;
    return percent;
}

SystemStats::Sample SystemStats::sample()
{
    if (!m_sensorsSearched)
        findSensors();

    Sample sample;
    sample.cpuPercent = cpuPercent();
    sample.cpuTemperature = readMilliCelsius(m_cpuTempFile);

    // /proc/meminfo in kB; used = total - available (as "free" reports it).
    // Files in /proc report size 0, so QFile::atEnd() is true before the
    // first read: read everything at once instead of line by line.
    QFile meminfo(m_procRoot + QStringLiteral("/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QHash<QString, qint64> values;
        const QStringList lines = QString::fromLatin1(meminfo.readAll()).split(u'\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.split(u' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2)
                values.insert(parts.first().chopped(1), parts.at(1).toLongLong() * 1024);
        }
        if (values.contains(QStringLiteral("MemTotal")) && values.contains(QStringLiteral("MemAvailable"))) {
            const qint64 total = values.value(QStringLiteral("MemTotal"));
            sample.ram = Memory{total - values.value(QStringLiteral("MemAvailable")), total};
        }
        const qint64 swapTotal = values.value(QStringLiteral("SwapTotal"));
        if (swapTotal > 0)
            sample.swap = Memory{swapTotal - values.value(QStringLiteral("SwapFree")), swapTotal};
    }

    if (!m_gpuDevice.isEmpty()) {
        Gpu gpu;
        if (const auto busy = readNumber(m_gpuDevice + QStringLiteral("/gpu_busy_percent")))
            gpu.busyPercent = int(std::clamp<qint64>(*busy, 0, 100));
        gpu.temperature = readMilliCelsius(m_gpuTempFile);
        const auto used = readNumber(m_gpuDevice + QStringLiteral("/mem_info_vram_used"));
        const auto total = readNumber(m_gpuDevice + QStringLiteral("/mem_info_vram_total"));
        if (used && total && *total > 0)
            gpu.vram = Memory{*used, *total};
        sample.gpu = gpu;
    }
    return sample;
}
