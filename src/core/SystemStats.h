// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

// System load and temperatures for the LCD page "system", read from /proc and
// /sys without special rights. Each value is optional: it is missing when the
// system does not provide it (e.g. no supported GPU).
class SystemStats {
public:
    struct Memory {
        qint64 used = 0;    // bytes
        qint64 total = 0;   // bytes
        int percent() const { return total > 0 ? int((used * 100 + total / 2) / total) : 0; }
    };

    struct Gpu {
        std::optional<int> busyPercent;
        std::optional<double> temperature;   // °C
        std::optional<Memory> vram;
    };

    struct Temperature {
        QString label;          // as the sensor names it, e.g. "Tctl", "junction"
        double celsius = 0;
    };

    // Details for the LCD page "cpu".
    struct CpuDetail {
        std::optional<int> percent;        // load since the previous call
        QList<Temperature> temperatures;   // all of the CPU sensor, e.g. Tctl, Tccd1
        QList<int> coreMHz;                // current clock per physical core
        int minMHz = 0;                    // clock range of the CPU (cpufreq)
        int maxMHz = 0;
    };

    // Details for the LCD pages "amdgpu" and "nvidia".
    struct GpuDetail {
        std::optional<int> busyPercent;
        std::optional<int> memBusyPercent;   // memory controller load
        std::optional<int> coreMHz;
        std::optional<int> memMHz;
        QList<Temperature> temperatures;     // AMD: edge, junction, mem
        std::optional<Memory> vram;
        std::optional<double> powerWatts;
        std::optional<int> fanRpm;
        std::optional<int> fanPercent;       // NVIDIA reports the fan in percent
    };

    struct Sample {
        std::optional<int> cpuPercent;       // needs two samples (load since the last one)
        std::optional<double> cpuTemperature;
        std::optional<Memory> ram;
        std::optional<Memory> swap;          // only when swap is configured
        std::optional<Gpu> gpu;              // the GPU with the most VRAM (AMD, amdgpu)
    };

    // procRoot and sysRoot are "/proc" and "/sys"; tests pass fake trees.
    explicit SystemStats(const QString &procRoot = QStringLiteral("/proc"),
                         const QString &sysRoot = QStringLiteral("/sys"));

    // Reads all values. The CPU load is the load since the previous call.
    Sample sample();

    // CPU load, temperatures and the clock of each physical core (the
    // threads of a core share it; the highest of them is taken).
    CpuDetail cpuDetail();

    // The AMD GPU (amdgpu) with the most VRAM; nullopt without one.
    std::optional<GpuDetail> amdGpuDetail();

private:
    void findSensors();
    std::optional<int> cpuPercent();

    QString m_procRoot;
    QString m_sysRoot;
    bool m_sensorsSearched = false;
    QString m_cpuTempFile;   // hwmon temp*_input of the CPU package
    QString m_cpuHwmon;      // hwmon directory of the CPU sensor
    QList<QStringList> m_cores;   // per physical core: cpufreq directories of its threads
    QString m_gpuDevice;     // /sys/class/drm/cardN/device
    QString m_gpuTempFile;
    QString m_gpuHwmon;      // hwmon directory of that GPU
    qint64 m_lastIdle = -1;
    qint64 m_lastTotal = -1;
};
