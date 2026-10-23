// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

// Monochrome frame for a small LCD with simple drawing (no QtGui: the daemon
// links Qt Core only).
class LcdFrame {
public:
    LcdFrame(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }

    void clear();
    void setPixel(int x, int y, bool on = true);
    bool pixel(int x, int y) const;
    void fillRect(int x, int y, int width, int height, bool on = true);
    void invertRect(int x, int y, int width, int height);
    void drawRect(int x, int y, int width, int height);

    // Draws text with the 5x7 font, scaled by an integer factor. Characters
    // outside ASCII are transliterated (ä -> ae, ...) or shown as '?'.
    // on = false draws the text with cleared pixels (e.g. on a filled box).
    // Returns the x position after the text.
    int drawText(int x, int y, const QString &text, int scale = 1, bool on = true);
    static int textWidth(const QString &text, int scale = 1);

    // Text cut to fit maxWidth pixels (with "~" marking the cut).
    static QString elided(const QString &text, int maxWidth, int scale = 1);

    // ASCII form of a text as drawn.
    static QString toAscii(const QString &text);

    // Output report 3 of the G13 (written to its hidraw node): report ID, 31
    // zero bytes, then 160 x 48 pixels in bands of 8 rows, one byte per column
    // and band, bit 0 = top row of the band. Validated on hardware by the g13d
    // project and identical to the layout linux-g13-driver sent over USB.
    QByteArray toG13Report() const;

    bool operator==(const LcdFrame &other) const = default;

private:
    // Index of a pixel that is inside the frame.
    size_t index(int x, int y) const
    {
        return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
    }
    int m_width;
    int m_height;
    std::vector<unsigned char> m_pixels;   // one byte per pixel, row-major
};
