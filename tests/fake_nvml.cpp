// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// A stand-in for libnvidia-ml.so.1 (NVML), so that NvidiaStats can be tested
// without an NVIDIA GPU. It has the functions NvidiaStats resolves, with the
// signatures of nvml.h, and returns the values that fakeNvmlSet() put in.
// fakeNvmlCalls() counts nvmlInit_v2 and nvmlShutdown, to see whether NVML
// was touched at all.

namespace {

struct Values {
    unsigned int count = 1;
    unsigned int gpuPercent = 0;
    unsigned int memPercent = 0;
    unsigned long long total = 0;
    unsigned long long used = 0;
    unsigned int temperature = 0;
    unsigned int coreMHz = 0;
    unsigned int memMHz = 0;
    unsigned int milliwatts = 0;
    unsigned int fanPercent = 0;
    bool fanSupported = true;
};

Values values;
int initCalls = 0;
int shutdownCalls = 0;
int device = 0;   // the handle is a pointer to this

constexpr int Success = 0;
constexpr int NotSupported = 3;   // NVML_ERROR_NOT_SUPPORTED
constexpr int InvalidArgument = 2;   // NVML_ERROR_INVALID_ARGUMENT

}

extern "C" {

struct FakeNvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};
struct FakeNvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

__attribute__((visibility("default"))) void fakeNvmlSet(unsigned int count, unsigned int gpuPercent,
                                                        unsigned int memPercent, unsigned long long total,
                                                        unsigned long long used, unsigned int temperature,
                                                        unsigned int coreMHz, unsigned int memMHz,
                                                        unsigned int milliwatts, int fanPercent)
{
    values.count = count;
    values.gpuPercent = gpuPercent;
    values.memPercent = memPercent;
    values.total = total;
    values.used = used;
    values.temperature = temperature;
    values.coreMHz = coreMHz;
    values.memMHz = memMHz;
    values.milliwatts = milliwatts;
    values.fanSupported = fanPercent >= 0;
    values.fanPercent = values.fanSupported ? static_cast<unsigned int>(fanPercent) : 0;
}

__attribute__((visibility("default"))) void fakeNvmlCalls(int *init, int *shutdown)
{
    *init = initCalls;
    *shutdown = shutdownCalls;
}

__attribute__((visibility("default"))) int nvmlInit_v2()
{
    ++initCalls;
    return Success;
}

__attribute__((visibility("default"))) int nvmlShutdown()
{
    ++shutdownCalls;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetCount_v2(unsigned int *count)
{
    *count = values.count;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetHandleByIndex_v2(unsigned int index, void **handle)
{
    if (index >= values.count)
        return InvalidArgument;
    *handle = &device;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetUtilizationRates(void *, FakeNvmlUtilization *utilization)
{
    utilization->gpu = values.gpuPercent;
    utilization->memory = values.memPercent;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetMemoryInfo(void *, FakeNvmlMemory *memory)
{
    memory->total = values.total;
    memory->used = values.used;
    memory->free = values.total - values.used;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetTemperature(void *, int sensor, unsigned int *celsius)
{
    if (sensor != 0)   // NVML_TEMPERATURE_GPU
        return InvalidArgument;
    *celsius = values.temperature;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetClockInfo(void *, int type, unsigned int *mhz)
{
    if (type == 0)   // NVML_CLOCK_GRAPHICS
        *mhz = values.coreMHz;
    else if (type == 2)   // NVML_CLOCK_MEM
        *mhz = values.memMHz;
    else
        return InvalidArgument;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetPowerUsage(void *, unsigned int *milliwatts)
{
    *milliwatts = values.milliwatts;
    return Success;
}

__attribute__((visibility("default"))) int nvmlDeviceGetFanSpeed(void *, unsigned int *percent)
{
    // Cards without a fan of their own (laptops) report "not supported".
    if (!values.fanSupported)
        return NotSupported;
    *percent = values.fanPercent;
    return Success;
}
}
