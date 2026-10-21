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
    static const QStringList pages = {QStringLiteral("status"), QStringLiteral("clock"), QStringLiteral("text")};
    return pages;
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

}
