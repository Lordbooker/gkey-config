// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdPages.h"

#include "LcdFont.h"
#include "LcdFrame.h"

#include <QLocale>

#include <algorithm>

namespace LcdPages {

namespace {

constexpr int LineHeight = LcdFont::Height + 1;

int centeredX(const LcdFrame &frame, const QString &text, int scale)
{
    return qMax(0, (frame.width() - LcdFrame::textWidth(text, scale)) / 2);
}

QString degrees(double celsius)
{
    return QStringLiteral("%1\u00b0").arg(qRound(celsius));
}

QString percentText(const std::optional<int> &percent)
{
    return percent ? QStringLiteral("%1%").arg(*percent) : QStringLiteral("--%");
}

// "5.3G" for 5271 MHz
QString gigahertz(int mhz)
{
    return QStringLiteral("%1G").arg(mhz / 1000.0, 0, 'f', 1);
}

// CPU temperatures as short as needed: all with their names if they fit, else
// the first (Tctl, Tdie or package) with its name and the others combined,
// else only the values.
QString cpuTemperatures(const QList<SystemStats::Temperature> &temperatures, int maxWidth)
{
    if (temperatures.isEmpty())
        return {};
    const auto shortLabel = [](QString label) {
        if (label.startsWith(u"Tccd"))
            return QStringLiteral("CCD") + label.mid(4);
        if (label.startsWith(u"Package"))
            return QStringLiteral("Pkg");
        return label;
    };
    QStringList all;
    for (const SystemStats::Temperature &t : temperatures)
        all.append(shortLabel(t.label) + u' ' + degrees(t.celsius));
    const QString full = all.join(u' ');
    if (LcdFrame::textWidth(full) <= maxWidth || temperatures.size() == 1)
        return full;
    QStringList rest;
    for (qsizetype i = 1; i < temperatures.size(); ++i)
        rest.append(QString::number(qRound(temperatures.at(i).celsius)));
    const QString combined = rest.join(u'/') + u'\u00b0';
    const QString named = all.first() + u' ' + combined;
    if (LcdFrame::textWidth(named) <= maxWidth)
        return named;
    return degrees(temperatures.first().celsius) + u' ' + combined;
}

}

const QStringList &allPages()
{
    static const QStringList pages = {IntroPage, QStringLiteral("status"), QStringLiteral("system"),
                                      QStringLiteral("clock"), QStringLiteral("text")};
    return pages;
}

QStringList rotation(const QStringList &pages)
{
    QStringList result = pages;
    result.removeAll(IntroPage);
    return result;
}

void renderStatus(LcdFrame &frame, const Status &status)
{
    frame.clear();
    const int width = frame.width();

    // Top line: device name, and "REC" while a macro is being recorded.
    frame.drawText(1, 1, LcdFrame::elided(status.deviceName, status.recording ? width - 26 : width - 2));
    if (status.recording) {
        frame.fillRect(width - 21, 0, 21, 9);
        frame.drawText(width - 19, 1, QStringLiteral("REC"), 1, false);
    }
    frame.fillRect(0, 10, width, 1);

    // Profile name, as large as it fits.
    const QString profile = status.profile.isEmpty() ? QStringLiteral("-") : status.profile;
    const int scale = LcdFrame::textWidth(profile, 2) <= width - 2 ? 2 : 1;
    const QString shown = LcdFrame::elided(profile, width - 2, scale);
    frame.drawText(centeredX(frame, shown, scale), scale == 2 ? 14 : 18, shown, scale);

    // Layer indicator: three boxes, the active one filled.
    const int boxWidth = 22;
    const int gap = 6;
    const int left = (width - (3 * boxWidth + 2 * gap)) / 2;
    for (int m = 1; m <= 3; ++m) {
        const int x = left + (m - 1) * (boxWidth + gap);
        const QString label = QStringLiteral("M%1").arg(m);
        const int textX = x + (boxWidth - LcdFrame::textWidth(label)) / 2;
        if (m == status.mode) {
            frame.fillRect(x, 33, boxWidth, 10);
            frame.drawText(textX, 35, label, 1, false);
        } else {
            frame.drawRect(x, 33, boxWidth, 10);
            frame.drawText(textX, 35, label);
        }
    }
}

void renderClock(LcdFrame &frame, const QDateTime &time)
{
    frame.clear();
    const QLocale locale;
    const QString hhmm = locale.toString(time, QStringLiteral("HH:mm"));
    const int scale = 4;
    const int x = centeredX(frame, hhmm, scale) - 8;   // room for the seconds on the right
    const int end = frame.drawText(x, 2, hhmm, scale);
    frame.drawText(end + 2, 2 + 7 * scale - LcdFont::Height, locale.toString(time, QStringLiteral("ss")));
    const QString date = locale.toString(time.date(), QLocale::ShortFormat);
    const QString day = locale.toString(time.date(), QStringLiteral("dddd"));
    const QString line = LcdFrame::elided(day + QStringLiteral(", ") + date, frame.width());
    frame.drawText(centeredX(frame, line, 1), 35, line);
}

void renderText(LcdFrame &frame, const QStringList &lines)
{
    frame.clear();
    const int maxLines = frame.height() / LineHeight;
    for (int i = 0; i < qMin<int>(maxLines, static_cast<int>(lines.size())); ++i)
        frame.drawText(1, 1 + i * LineHeight, LcdFrame::elided(lines.at(i), frame.width() - 2));
}

QString formatSize(qint64 bytes)
{
    const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gib >= 99.95)
        return QStringLiteral("%1G").arg(qRound(gib));
    return QStringLiteral("%1G").arg(gib, 0, 'f', 1);
}

void renderSystem(LcdFrame &frame, const SystemStats::Sample &sample)
{
    struct Row {
        QString label;
        std::optional<int> percent;
        QString extra;
    };
    const auto temperature = [](const std::optional<double> &celsius) {
        return celsius ? QStringLiteral("%1\u00b0C").arg(qRound(*celsius)) : QString();
    };
    const auto memoryRow = [](const QString &label, const std::optional<SystemStats::Memory> &memory) {
        return memory ? Row{label, memory->percent(), formatSize(memory->used)} : Row{label, std::nullopt, QString()};
    };

    QList<Row> rows;
    rows.append(Row{QStringLiteral("CPU"), sample.cpuPercent, temperature(sample.cpuTemperature)});
    rows.append(memoryRow(QStringLiteral("RAM"), sample.ram));
    if (sample.gpu) {
        rows.append(Row{QStringLiteral("GPU"), sample.gpu->busyPercent, temperature(sample.gpu->temperature)});
        rows.append(memoryRow(QStringLiteral("VRM"), sample.gpu->vram));
    } else if (sample.swap) {
        rows.append(memoryRow(QStringLiteral("SWP"), sample.swap));
    }

    // Rows of label, bar (22..91), percent (96..119) and right-aligned extra.
    frame.clear();
    constexpr int BarX = 22, BarWidth = 70, BarHeight = 9, PercentX = 96;
    const int pitch = rows.size() > 3 ? 11 : 14;
    const int top = rows.size() > 3 ? 0 : 2;
    for (qsizetype i = 0; i < rows.size(); ++i) {
        const Row &row = rows.at(i);
        const int y = top + int(i) * pitch;
        frame.drawText(1, y + 1, row.label);
        frame.drawRect(BarX, y, BarWidth, BarHeight);
        if (row.percent)
            frame.fillRect(BarX + 1, y + 1, (BarWidth - 2) * qBound(0, *row.percent, 100) / 100, BarHeight - 2);
        frame.drawText(PercentX, y + 1,
                       row.percent ? QStringLiteral("%1%").arg(*row.percent, 3) : QStringLiteral(" --%"));
        if (!row.extra.isEmpty())
            frame.drawText(frame.width() - LcdFrame::textWidth(row.extra) - 1, y + 1, row.extra);
    }
}

void renderCpu(LcdFrame &frame, const SystemStats::CpuDetail &cpu)
{
    frame.clear();
    const QString load = QStringLiteral("CPU ") + percentText(cpu.percent);
    frame.drawText(1, 1, load);
    // textWidth() leaves out the gap after the last character: + 1 for it
    const int tempX = 1 + LcdFrame::textWidth(load + u' ') + 1;
    frame.drawText(tempX, 1,
                   LcdFrame::elided(cpuTemperatures(cpu.temperatures, frame.width() - tempX - 1), frame.width() - tempX - 1));

    // Bars: x 28..158, y 10..42; scale from the lowest to the highest clock
    constexpr int Left = 28, Top = 10;
    const int width = frame.width() - Left - 1;
    const int height = frame.height() - Top;
    if (cpu.coreMHz.isEmpty() || cpu.maxMHz <= cpu.minMHz) {
        frame.drawText(Left, Top + height / 2 - 4, QStringLiteral("no clock (cpufreq)"));
        return;
    }
    frame.drawText(1, Top, gigahertz(cpu.maxMHz));
    frame.drawText(1, frame.height() - LcdFont::Height, gigahertz(cpu.minMHz));

    // More cores than columns: one column shows the highest clock of the
    // cores it stands for.
    const int cores = static_cast<int>(cpu.coreMHz.size());
    const int count = qMin(cores, width);
    const int pitch = width / count;
    const int gap = pitch >= 3 ? 1 : 0;
    const int start = Left + (width - pitch * count) / 2;   // centre the group
    for (int i = 0; i < count; ++i) {
        const int first = i * cores / count;
        const int last = (i + 1) * cores / count;
        const int mhz = *std::max_element(cpu.coreMHz.cbegin() + first, cpu.coreMHz.cbegin() + last);
        const double share = std::clamp(double(mhz - cpu.minMHz) / (cpu.maxMHz - cpu.minMHz), 0.0, 1.0);
        const int barHeight = qMax(1, qRound(share * height));
        frame.fillRect(start + i * pitch, frame.height() - barHeight, pitch - gap, barHeight);
    }
}

}
