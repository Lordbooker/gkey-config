// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdPages.h"

#include "LcdFont.h"
#include "LcdFrame.h"

#include <QLocale>

namespace LcdPages {

namespace {

constexpr int LineHeight = LcdFont::Height + 1;

int centeredX(const LcdFrame &frame, const QString &text, int scale)
{
    return qMax(0, (frame.width() - LcdFrame::textWidth(text, scale)) / 2);
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

}
