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
//
// NVML is only touched when sysfs shows an NVIDIA graphics device that is
// awake: without one the library is not even loaded, and a GPU that the
// kernel has put to sleep (runtime power management, hybrid laptops) is not
// woken up. Whether an open NVML connection keeps such a GPU from sleeping is
// not known, so on devices with runtime power management only the page
// "nvidia" uses NVML, and pageLeft() closes it again.
class NvidiaStats {
public:
    // libraryName and sysRoot: for tests (a fake library and sysfs tree).
    explicit NvidiaStats(const QString &libraryName = QStringLiteral("nvidia-ml"),
                         const QString &sysRoot = QStringLiteral("/sys"));
    ~NvidiaStats();
    NvidiaStats(const NvidiaStats &) = delete;
    NvidiaStats &operator=(const NvidiaStats &) = delete;

    // The first NVIDIA GPU; nullopt without NVML or GPU. Temperatures: the
    // GPU sensor only (NVML does not publish hotspot or memory temperatures
    // for consumer cards). The fan is reported in percent.
    std::optional<SystemStats::GpuDetail> sample();

    // The rows of the system page: load, GPU temperature and VRAM. nullopt
    // without a usable NVIDIA GPU, and on devices with runtime power
    // management (see above).
    std::optional<SystemStats::Gpu> systemSample();

    // Called when the page "nvidia" is no longer shown: closes NVML on
    // devices with runtime power management.
    void pageLeft();

    // Why there is no sample (for the LCD page), e.g. "no NVML".
    const QString &problem() const { return m_problem; }

private:
    bool load();
    void unload();
    // The first NVIDIA graphics device in sysfs (searched once), or empty.
    const QString &pciDevice();
    QString devicePower(const QString &file) const;
    bool asleep() const;
    bool powerManaged() const;

    QString m_libraryName;
    QString m_sysRoot;
    bool m_pciSearched = false;
    QString m_pciDevice;
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
