// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "KeyOutput.h"

void KeyOutput::press(int code)
{
    int &count = m_counts[code];
    if (++count == 1)
        m_sink.writeKey(code, true);
}

void KeyOutput::release(int code)
{
    auto it = m_counts.find(code);
    if (it == m_counts.end())
        return;
    if (--it.value() == 0) {
        m_counts.erase(it);
        m_sink.writeKey(code, false);
    }
}
