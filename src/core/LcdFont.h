// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

// 5x7 bitmap font for the monochrome LCD.
namespace LcdFont {

constexpr int Width = 5;
constexpr int Height = 7;
constexpr int Advance = 6;   // character width including one column of space

// Code of the degree sign in the font (after the printable ASCII range);
// LcdFrame::toAscii maps U+00B0 to it.
constexpr char32_t DegreeSign = 127;

// Five column bytes (bit 0 = top row) for an ASCII character or DegreeSign;
// '?' otherwise.
const unsigned char *glyph(char32_t c);

}
