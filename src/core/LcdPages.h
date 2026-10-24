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

// "16.4G": bytes in GiB, at most five characters.
QString formatSize(qint64 bytes);

}
