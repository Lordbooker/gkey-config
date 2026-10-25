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

// Page names in their default order.
const QStringList &allPages();

// The pages enabled without settings: all but the hardware pages (cpu,
// amdgpu, nvidia), which are only useful with that hardware.
const QStringList &defaultPages();

// The pages that "next" and "previous" go through: the enabled pages without
// the start animation.
QStringList rotation(const QStringList &pages);

struct Status {
    QString deviceName;
    QString profile;
    int mode = 1;
    bool recording = false;
};

void renderStatus(LcdFrame &frame, const Status &status);
void renderClock(LcdFrame &frame, const QDateTime &time);
// Up to five lines; longer lines are cut.
void renderText(LcdFrame &frame, const QStringList &lines);
// CPU, RAM, GPU and VRAM with bar, percent and temperature or amount; without
// a supported GPU CPU, RAM and swap.
void renderSystem(LcdFrame &frame, const SystemStats::Sample &sample);

// CPU load and temperatures in the top row, below one bar per physical core
// for its clock between the CPU's lowest and highest clock. The bars get
// narrower with more cores, so all of them fit.
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
