// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once
#include <QString>

class QKeyEvent;

// Readable names for Linux input key codes (linux/input-event-codes.h).
//
// Based on a table of names by US key position. Character keys that have been
// captured before (key capture field or macro recording) are shown with the
// label of the user's own keyboard layout (e.g. "Z" instead of "Y" on a German
// keyboard). These labels are stored in the gkey-config settings.
namespace KeyNames {

// Name for a key or mouse button code; unknown codes are shown as "Key <code>".
QString nameFor(int code);

// Remembers the label of the key from a key press, if it produces a
// character (without modifiers, not on the keypad).
void learnLabel(int code, const QKeyEvent *event);

}
