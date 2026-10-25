// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdIntro.h"

#include "LcdFrame.h"
#include "LcdIntroImages.h"

#include <algorithm>
#include <cmath>

static_assert(LcdIntro::Width == LcdIntroImages::Width && LcdIntro::Height == LcdIntroImages::Height,
              "bitmaps do not match the animation size");

namespace {

// Schedule in seconds (as in linux-g13-driver)
constexpr double THold = 1.6;     // start image is shown
constexpr double TRise = 2.4;     // flame wall rises
constexpr double TCover = 0.6;    // display fully covered, the image is swapped
constexpr double TRecede = 2.6;   // wall recedes
constexpr double TSettle = 1.6;   // embers die out
constexpr double TRiseEnd = THold + TRise;
constexpr double TCoverEnd = TRiseEnd + TCover;
constexpr double TRecedeEnd = TCoverEnd + TRecede;
constexpr double TTotal = TRecedeEnd + TSettle;

constexpr std::uint32_t Seed = 1313;

// 4x4 Bayer matrix for dithering
constexpr double bayer(int x, int y)
{
    constexpr int m[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return (m[(static_cast<unsigned>(y) & 3U) * 4 + (static_cast<unsigned>(x) & 3U)] + 0.5) / 16.0;
}

// Pixel of a 1-bit image with 8 pixels per byte, the leftmost in the highest bit.
template <typename Bitmap>
bool bitmapPixel(const Bitmap &bitmap, int x, int y)
{
    const auto byte = static_cast<unsigned>(bitmap[y][x / 8]);
    return ((byte >> (7U - static_cast<unsigned>(x % 8))) & 1U) != 0;
}

double ease(double t)
{
    return t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
}

std::size_t index(int x, int y)
{
    return static_cast<std::size_t>(y) * LcdIntro::Width + static_cast<std::size_t>(x);
}

}

LcdIntro::LcdIntro(const QString &modelId)
    : m_rng(Seed), m_heat(static_cast<std::size_t>(Width) * FieldHeight, 0),
      m_burned(static_cast<std::size_t>(Width) * Height, 0), m_top(Width, 0.0), m_cool(Width, 0.0)
{
    if (modelId == u"g13") {
        m_start = &LcdIntroImages::g13Firmware;
        m_final = &LcdIntroImages::g13Final;
    } else if (modelId == u"g15") {
        m_final = &LcdIntroImages::g15Final;
    } else if (modelId == u"g15v2") {
        m_final = &LcdIntroImages::g15v2Final;
    } else if (modelId == u"g510") {
        m_final = &LcdIntroImages::g510Final;
    }
}

int LcdIntro::frameCount()
{
    return static_cast<int>(std::lround(TTotal * Fps));
}

bool LcdIntro::startPixel(int x, int y) const
{
    if (!m_start)
        return true;   // dark display
    return bitmapPixel(*m_start, x, y);
}

bool LcdIntro::finalPixel(int x, int y) const
{
    if (!m_final)
        return x >= 40 || bitmapPixel(LcdIntroImages::g13Final, x, y);   // Tux only
    return bitmapPixel(*m_final, x, y);
}

// mulberry32: the same sequence as in linux-g13-driver
double LcdIntro::random()
{
    m_rng += 0x6D2B79F5U;
    std::uint32_t t = (m_rng ^ (m_rng >> 15U)) * (1U | m_rng);
    t = (t + ((t ^ (t >> 7U)) * (61U | t))) ^ t;
    return static_cast<double>(t ^ (t >> 14U)) / 4294967296.0;
}

// Height of the hot wall body in rows from the bottom; <= 0: no wall.
double LcdIntro::wallLevel(double t) const
{
    const double full = FieldHeight + 6;
    if (t < THold)
        return -1;
    if (t < TRiseEnd)
        return ease((t - THold) / TRise) * full;
    if (t < TCoverEnd)
        return full;
    if (t < TRecedeEnd)
        return (1 - ease((t - TCoverEnd) / TRecede)) * (full + 3) - 3;
    return -1;
}

void LcdIntro::step()
{
    if (finished())
        return;
    const double t = static_cast<double>(m_frame) / Fps;
    const double level = wallLevel(t);
    const double phase = m_frame * 0.3;

    // Wall body: maximum heat below the (slightly waving) top edge
    for (int x = 0; x < Width; ++x) {
        const auto column = static_cast<std::size_t>(x);
        if (level > 0) {
            const double wave = std::sin(x * 0.09 + phase) * 1.6 + std::sin(x * 0.23 - phase * 1.4) * 1.0;
            m_top[column] = FieldHeight - level + wave;
        } else {
            m_top[column] = FieldHeight + 99;
        }
        for (int y = std::max(0, static_cast<int>(std::ceil(m_top[column]))); y < FieldHeight; ++y)
            m_heat[index(x, y)] = MaxHeat - static_cast<int>(random() * 6);
        if (m_top[column] >= FieldHeight) {
            // The bottom row is never fed from below: let it cool down without a wall
            int &bottom = m_heat[index(x, FieldHeight - 1)];
            bottom = bottom > 6 ? bottom - 6 : 0;
        }
    }

    // Cooling per column: moving patterns produce high and low flame tongues
    for (int x = 0; x < Width; ++x) {
        const double n = 0.5 + 0.5 * (std::sin(x * 0.42 + phase * 1.9) * 0.55 + std::sin(x * 0.97 - phase * 2.7) * 0.3 +
                                      std::sin(x * 0.17 + phase * 0.8) * 0.15);
        m_cool[static_cast<std::size_t>(x)] = 2 + n * n * 11;
    }

    // Spread heat upwards (with sideways offset and random cooling). Three
    // substeps per frame, so the tongues grow faster than the wall rises.
    for (int sub = 0; sub < 3; ++sub) {
        for (int y = 1; y < FieldHeight; ++y) {
            for (int x = 0; x < Width; ++x) {
                const int p = m_heat[index(x, y)];
                const int r = static_cast<int>(random() * 3);
                const int dx = std::clamp(x - r + 1, 0, Width - 1);
                if (y - 1 >= m_top[static_cast<std::size_t>(dx)])
                    continue;   // target is inside the wall body
                int &target = m_heat[index(dx, y - 1)];
                if (p == 0) {
                    target = 0;
                    continue;
                }
                const int c = static_cast<int>(random() * m_cool[static_cast<std::size_t>(dx)]);
                target = p > c ? p - c : 0;
            }
        }
    }

    // The start image burns where the flames are hot enough
    for (std::size_t i = 0; i < m_burned.size(); ++i) {
        if (m_heat[i] > MaxHeat * 0.35)
            m_burned[i] = 1;
    }
    if (!m_swapped && t >= TRiseEnd + TCover * 0.5)
        m_swapped = true;

    ++m_frame;
}

// Bright flames on a dark background (the look of the G13 firmware)
void LcdIntro::render(LcdFrame &frame) const
{
    if (finished()) {
        renderFinal(frame);
        return;
    }
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            const double h = static_cast<double>(m_heat[index(x, y)]) / MaxHeat;
            bool on;
            if (h > 0.1) {
                const double density = std::min(0.97, std::max(0.0, (h - 0.2) / 0.62));
                on = density <= bayer(x, y);
            } else if (m_swapped) {
                on = finalPixel(x, y);
            } else {
                on = m_burned[index(x, y)] || startPixel(x, y);
            }
            frame.setPixel(x, y, on);
        }
    }
}

void LcdIntro::renderFinal(LcdFrame &frame) const
{
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x)
            frame.setPixel(x, y, finalPixel(x, y));
}
