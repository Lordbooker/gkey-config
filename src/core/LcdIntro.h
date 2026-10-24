// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QString>

#include <cstdint>
#include <vector>

class LcdFrame;

// Start animation of the LCD (from linux-g13-driver): a flame wall rises from
// the bottom, burns the start image, covers the whole display and recedes,
// revealing the final image (Tux and the device name). The flames come from a
// heat field after the classic "Doom fire" and are reduced to 1 bit with a 4x4
// Bayer dither.
//
// Start image: the firmware logo on the G13, a dark display on other devices
// (their firmware logos are not known). The random generator has a fixed seed
// (mulberry32), so the animation is the same on every run. All supported LCDs
// are 160 x 43 pixels.
class LcdIntro {
public:
    static constexpr int Width = 160;
    static constexpr int Height = 43;
    static constexpr int Fps = 20;
    static constexpr int FrameMs = 1000 / Fps;

    // modelId selects the images ("g13", "g15", "g15v2", "g510"); an unknown
    // model gets a dark start and Tux without a name.
    explicit LcdIntro(const QString &modelId);

    // Number of animation frames (about 9 seconds), without the final image.
    static int frameCount();

    // Computes the next frame.
    void step();
    bool finished() const { return m_frame >= frameCount(); }
    int frame() const { return m_frame; }

    // Draws the current frame (the final image once finished).
    void render(LcdFrame &frame) const;
    // Draws the final image only.
    void renderFinal(LcdFrame &frame) const;

private:
    static constexpr int FieldHeight = Height + 4;   // invisible rows below the display
    static constexpr int MaxHeat = 48;

    using Bitmap = std::uint8_t[Height][Width / 8];

    double random();
    double wallLevel(double t) const;
    bool startPixel(int x, int y) const;
    bool finalPixel(int x, int y) const;

    const Bitmap *m_start = nullptr;   // nullptr: dark display
    const Bitmap *m_final = nullptr;
    std::uint32_t m_rng;
    std::vector<int> m_heat;              // Width * FieldHeight
    std::vector<std::uint8_t> m_burned;   // Width * Height: the start image has burned here
    std::vector<double> m_top;            // top edge of the wall body per column
    std::vector<double> m_cool;           // cooling per column (creates the flame tongues)
    bool m_swapped = false;               // final image instead of the start image as background
    int m_frame = 0;
};
