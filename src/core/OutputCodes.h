#pragma once

#include <QList>

// Event codes the virtual output device offers.
//
// Joystick, gamepad, digitizer and other button ranges are left out on
// purpose: a device announcing them is classified as a joystick or tablet by
// udev/libinput/SDL, which would confuse games.
namespace OutputCodes {

// Keyboard keys (KEY_ESC .. KEY_MICMUTE and KEY_OK .. KEY_MAX without the
// BTN_TRIGGER_HAPPY range).
bool isKeyboardKey(int code);

// Mouse buttons (BTN_LEFT .. BTN_TASK).
bool isMouseButton(int code);

// All keys and buttons of the virtual device.
QList<int> allKeys();

}
