#include "OutputCodes.h"

#include <linux/input-event-codes.h>

namespace OutputCodes {

bool isKeyboardKey(int code)
{
    if (code >= KEY_ESC && code <= KEY_MICMUTE)
        return true;
    if (code >= KEY_OK && code <= KEY_MAX)
        return !(code >= BTN_TRIGGER_HAPPY && code <= BTN_TRIGGER_HAPPY40);
    return false;
}

bool isMouseButton(int code)
{
    return code >= BTN_LEFT && code <= BTN_TASK;
}

QList<int> allKeys()
{
    QList<int> codes;
    for (int code = 0; code <= KEY_MAX; ++code) {
        if (isKeyboardKey(code) || isMouseButton(code))
            codes.append(code);
    }
    return codes;
}

}
