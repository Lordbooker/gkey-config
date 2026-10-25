// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "SystemStats.h"

#include <QLibrary>
#include <QString>

#include <optional>

// Values of an NVIDIA GPU through NVML (libnvidia-ml.so.1, part of the
// proprietary driver; readable without special rights). The library is loaded
// at runtime when it exists, so there is no build or package dependency.
// Untested on hardware (no NVIDIA test GPU).
class NvidiaStats {
public:
    // libraryName: for tests (a library that does not exist).
    explicit NvidiaStats(const QString &libraryName = QStringLiteral("nvidia-ml"));
    ~NvidiaStats();
    NvidiaStats(const NvidiaStats &) = delete;
    NvidiaStats &operator=(const NvidiaStats &) = delete;

    // The first NVIDIA GPU; nullopt without NVML or GPU. Temperatures: the
    // GPU sensor only (NVML does not publish hotspot or memory temperatures
    // for consumer cards). The fan is reported in percent.
    std::optional<SystemStats::GpuDetail> sample();

    // Why there is no sample (for the LCD page), e.g. "no NVML".
    const QString &problem() const { return m_problem; }

private:
    bool load();

    QString m_libraryName;
    QLibrary m_library;
    bool m_tried = false;
    bool m_initialized = false;
    QString m_problem;

    using Device = void *;   // nvmlDevice_t
    struct Utilization {
        unsigned int gpu;
        unsigned int memory;
    };
    struct Memory {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };
    int (*m_init)() = nullptr;
    int (*m_shutdown)() = nullptr;
    int (*m_count)(unsigned int *) = nullptr;
    int (*m_handle)(unsigned int, Device *) = nullptr;
    int (*m_utilization)(Device, Utilization *) = nullptr;
    int (*m_memory)(Device, Memory *) = nullptr;
    int (*m_temperature)(Device, int, unsigned int *) = nullptr;
    int (*m_clock)(Device, int, unsigned int *) = nullptr;
    int (*m_power)(Device, unsigned int *) = nullptr;
    int (*m_fan)(Device, unsigned int *) = nullptr;
};
