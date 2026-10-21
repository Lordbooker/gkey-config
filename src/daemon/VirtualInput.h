#pragma once

#include "KeyOutput.h"

#include <QString>

struct libevdev;
struct libevdev_uinput;

// The virtual uinput device all output goes through (docs/CONCEPT.md 9.3).
// It has its own (non-Logitech) identity so that device discovery and udev
// rules never mistake it for real hardware.
class VirtualInput : public OutputSink {
public:
    static constexpr const char *DeviceName = "gkey-config virtual input";

    VirtualInput() = default;
    ~VirtualInput() override;
    VirtualInput(const VirtualInput &) = delete;
    VirtualInput &operator=(const VirtualInput &) = delete;

    // Creates the device. Returns false and sets an error text on failure.
    bool create(QString *error);

    void writeKey(int code, bool pressed) override;
    void sync() override;

private:
    libevdev *m_device = nullptr;
    libevdev_uinput *m_uinput = nullptr;
};
