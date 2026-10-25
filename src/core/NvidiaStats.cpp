// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "NvidiaStats.h"

#include <QLoggingCategory>

#include <algorithm>
#include <type_traits>

namespace {

Q_LOGGING_CATEGORY(lcNvidia, "gkey.nvidia")

// From nvml.h (NVIDIA Management Library documentation)
constexpr int Success = 0;            // NVML_SUCCESS
constexpr int TemperatureGpu = 0;     // NVML_TEMPERATURE_GPU
constexpr int ClockGraphics = 0;      // NVML_CLOCK_GRAPHICS
constexpr int ClockMemory = 2;        // NVML_CLOCK_MEM

}

NvidiaStats::NvidiaStats(const QString &libraryName) : m_libraryName(libraryName) {}

NvidiaStats::~NvidiaStats()
{
    if (m_initialized && m_shutdown)
        m_shutdown();
}

bool NvidiaStats::load()
{
    m_tried = true;
    m_library.setFileNameAndVersion(m_libraryName, 1);   // libnvidia-ml.so.1
    if (!m_library.load()) {
        m_problem = QStringLiteral("no NVIDIA driver (NVML)");
        return false;
    }
    const auto resolve = [this](auto &function, const char *name) {
        function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(m_library.resolve(name));
        return function != nullptr;
    };
    if (!resolve(m_init, "nvmlInit_v2") || !resolve(m_shutdown, "nvmlShutdown")
        || !resolve(m_count, "nvmlDeviceGetCount_v2") || !resolve(m_handle, "nvmlDeviceGetHandleByIndex_v2")
        || !resolve(m_utilization, "nvmlDeviceGetUtilizationRates") || !resolve(m_memory, "nvmlDeviceGetMemoryInfo")
        || !resolve(m_temperature, "nvmlDeviceGetTemperature") || !resolve(m_clock, "nvmlDeviceGetClockInfo")
        || !resolve(m_power, "nvmlDeviceGetPowerUsage") || !resolve(m_fan, "nvmlDeviceGetFanSpeed")) {
        m_problem = QStringLiteral("NVML too old");
        qCWarning(lcNvidia) << "NVML lacks a needed function:" << m_library.errorString();
        return false;
    }
    if (m_init() != Success) {
        m_problem = QStringLiteral("NVML not ready");
        return false;
    }
    m_initialized = true;
    return true;
}

std::optional<SystemStats::GpuDetail> NvidiaStats::sample()
{
    if (!m_tried)
        load();
    if (!m_initialized)
        return std::nullopt;

    unsigned int count = 0;
    Device device = nullptr;
    if (m_count(&count) != Success || count == 0 || m_handle(0, &device) != Success) {
        m_problem = QStringLiteral("no NVIDIA GPU");
        return std::nullopt;
    }
    m_problem.clear();

    SystemStats::GpuDetail detail;
    Utilization utilization{};
    if (m_utilization(device, &utilization) == Success) {
        detail.busyPercent = static_cast<int>(std::min(utilization.gpu, 100U));
        detail.memBusyPercent = static_cast<int>(std::min(utilization.memory, 100U));
    }
    Memory memory{};
    if (m_memory(device, &memory) == Success && memory.total > 0)
        detail.vram = SystemStats::Memory{static_cast<qint64>(memory.used), static_cast<qint64>(memory.total)};
    unsigned int value = 0;
    if (m_temperature(device, TemperatureGpu, &value) == Success)
        detail.temperatures.append({QStringLiteral("GPU"), static_cast<double>(value)});
    if (m_clock(device, ClockGraphics, &value) == Success)
        detail.coreMHz = static_cast<int>(value);
    if (m_clock(device, ClockMemory, &value) == Success)
        detail.memMHz = static_cast<int>(value);
    if (m_power(device, &value) == Success)
        detail.powerWatts = value / 1000.0;   // milliwatts
    if (m_fan(device, &value) == Success)
        detail.fanPercent = static_cast<int>(value);
    return detail;
}
