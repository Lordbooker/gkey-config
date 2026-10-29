// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "SystemStats.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

class LcdFrame;

// Content of the LCD pages (docs/CONCEPT.md 7.2). Shared by the daemon (output
// to the device) and the GUI (preview).
namespace LcdPages {

// The start animation (LcdIntro): shown first when a device connects, if it is
// enabled; not part of the pages the device pages through.
inline const QString IntroPage = QStringLiteral("intro");

// The name of the profile, large: shown for a few seconds when the profile
// changes, if it is enabled; not part of the pages the device pages through.
inline const QString ProfilePage = QStringLiteral("profile");

// Page names in their default order.
const QStringList &allPages();

// The pages enabled without settings: all but the hardware pages (cpu,
// amdgpu, nvidia), which are only useful with that hardware.
const QStringList &defaultPages();

// The pages that "next" and "previous" go through: the enabled pages without
// the start animation and the profile page.
QStringList rotation(const QStringList &pages);

struct Status {
    QString deviceName;
    QString profile;
    int mode = 1;
    bool recording = false;
};

void renderStatus(LcdFrame &frame, const Status &status);
void renderClock(LcdFrame &frame, const QDateTime &time);
// The profile name as large as it fits: one line in scale 5 down to 2, else two
// lines in scale 2 (broken at a space), else one line in scale 1 (cut after 26
// characters).
void renderProfile(LcdFrame &frame, const QString &name);
// Up to five lines; longer lines are cut.
void renderText(LcdFrame &frame, const QStringList &lines);
// CPU, RAM, GPU and VRAM with bar, percent and temperature or amount; without
// a supported GPU CPU, RAM and swap.
void renderSystem(LcdFrame &frame, const SystemStats::Sample &sample);

// CPU load and temperatures in the top row, below one bar per logical CPU for
// its clock from 0 to the CPU's highest clock. The bars get narrower with more
// CPUs, so all of them fit.
void renderCpu(LcdFrame &frame, const SystemStats::CpuDetail &cpu);

// One GPU (AMD through amdgpu, NVIDIA through NVML): title and load bar,
// core and memory clock, all temperatures, VRAM, power and fan. Values the
// driver does not report are left out or shown as "--". Without a GPU the
// title and the reason (problem) are shown.
void renderGpu(LcdFrame &frame, const QString &title, const std::optional<SystemStats::GpuDetail> &gpu,
               const QString &problem);

// "16.4G": bytes in GiB, at most five characters.
QString formatSize(qint64 bytes);

}
