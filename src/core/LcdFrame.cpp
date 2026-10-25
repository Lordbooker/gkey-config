// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdFrame.h"

#include "LcdFont.h"

#include <QHash>

#include <algorithm>

LcdFrame::LcdFrame(int width, int height)
    : m_width(width), m_height(height), m_pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0)
{
}

void LcdFrame::clear()
{
    std::ranges::fill(m_pixels, 0);
}

void LcdFrame::setPixel(int x, int y, bool on)
{
    if (x >= 0 && y >= 0 && x < m_width && y < m_height)
        m_pixels[index(x, y)] = on ? 1 : 0;
}

bool LcdFrame::pixel(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
        return false;
    return m_pixels[index(x, y)] != 0;
}

void LcdFrame::fillRect(int x, int y, int width, int height, bool on)
{
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col)
            setPixel(col, row, on);
}

void LcdFrame::invertRect(int x, int y, int width, int height)
{
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col)
            setPixel(col, row, !pixel(col, row));
}

void LcdFrame::drawRect(int x, int y, int width, int height)
{
    fillRect(x, y, width, 1);
    fillRect(x, y + height - 1, width, 1);
    fillRect(x, y, 1, height);
    fillRect(x + width - 1, y, 1, height);
}

QString LcdFrame::toAscii(const QString &text)
{
    static const QHash<QChar, QString> replacements = {
        {QChar(0xe4), QStringLiteral("ae")}, {QChar(0xf6), QStringLiteral("oe")}, {QChar(0xfc), QStringLiteral("ue")},
        {QChar(0xc4), QStringLiteral("Ae")}, {QChar(0xd6), QStringLiteral("Oe")}, {QChar(0xdc), QStringLiteral("Ue")},
        {QChar(0xdf), QStringLiteral("ss")}, {QChar(0x2013), QStringLiteral("-")}, {QChar(0x2014), QStringLiteral("-")},
        {QChar(0x201e), QStringLiteral("\"")}, {QChar(0x201c), QStringLiteral("\"")}, {QChar(0x201d), QStringLiteral("\"")},
        {QChar(0xab), QStringLiteral("\"")}, {QChar(0xbb), QStringLiteral("\"")}, {QChar(0x2018), QStringLiteral("'")},
        {QChar(0x2019), QStringLiteral("'")}, {QChar(0x2026), QStringLiteral("...")}, {QChar(0xa0), QStringLiteral(" ")},
        {QChar(0x202f), QStringLiteral(" ")}, {QChar(0xc6), QStringLiteral("AE")}, {QChar(0xe6), QStringLiteral("ae")},
        {QChar(0x152), QStringLiteral("OE")}, {QChar(0x153), QStringLiteral("oe")}, {QChar(0xaa), QStringLiteral("a")},
        {QChar(0xba), QStringLiteral("o")},
    };
    QString result;
    for (const QChar c : text) {
        if (c.unicode() >= 32 && c.unicode() <= 126) {
            result.append(c);
            continue;
        }
        // Degree sign: own glyph in the font. Its code (127) is kept as it is,
        // so text that was already converted (e.g. by elided()) stays the same.
        if (c.unicode() == 0xb0 || c.unicode() == LcdFont::DegreeSign) {
            result.append(QChar(static_cast<char16_t>(LcdFont::DegreeSign)));
            continue;
        }
        const auto it = replacements.constFind(c);
        if (it != replacements.constEnd()) {
            result.append(it.value());
            continue;
        }
        // Accented letters: use the base letter (é -> e).
        const QString decomposed = QString(c).normalized(QString::NormalizationForm_D);
        const QChar base = decomposed.isEmpty() ? QChar() : decomposed.at(0);
        result.append(base.unicode() >= 32 && base.unicode() <= 126 ? base : QChar(u'?'));
    }
    return result;
}

int LcdFrame::textWidth(const QString &text, int scale)
{
    const qsizetype length = toAscii(text).size();
    return length == 0 ? 0 : static_cast<int>(length * LcdFont::Advance * scale - scale);
}

QString LcdFrame::elided(const QString &text, int maxWidth, int scale)
{
    QString ascii = toAscii(text);
    if (textWidth(ascii, scale) <= maxWidth)
        return ascii;
    while (!ascii.isEmpty() && textWidth(ascii + u'~', scale) > maxWidth)
        ascii.chop(1);
    return ascii + u'~';
}

int LcdFrame::drawText(int x, int y, const QString &text, int scale, bool on)
{
    for (const QChar c : toAscii(text)) {
        const unsigned char *columns = LcdFont::glyph(c.unicode());
        for (int col = 0; col < LcdFont::Width; ++col)
            for (int row = 0; row < LcdFont::Height; ++row)
                if ((static_cast<unsigned>(columns[col]) >> static_cast<unsigned>(row)) & 1U)
                    fillRect(x + col * scale, y + row * scale, scale, scale, on);
        x += LcdFont::Advance * scale;
    }
    return x;
}

QByteArray LcdFrame::toG13Report() const
{
    constexpr int Width = 160;
    constexpr int Bands = 6;             // 48 rows; the panel shows 43 of them
    constexpr int Header = 32;
    QByteArray report(Header + Width * Bands, '\0');
    report[0] = 0x03;
    for (int band = 0; band < Bands; ++band) {
        for (int x = 0; x < Width; ++x) {
            unsigned char bits = 0;
            for (int bit = 0; bit < 8; ++bit)
                if (pixel(x, band * 8 + bit))
                    bits |= static_cast<unsigned char>(1U << static_cast<unsigned>(bit));
            report[Header + band * Width + x] = static_cast<char>(bits);
        }
    }
    return report;
}
