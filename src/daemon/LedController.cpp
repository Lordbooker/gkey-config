// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LedController.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcLeds, "gkey.leds")

LedController::LedController(const DeviceModel &model, const QString &hidSyspath) : m_model(model)
{
    const QDir ledDir(hidSyspath + QStringLiteral("/leds"));
    const QStringList names = ledDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const DeviceModel::Led &led : m_model.leds()) {
        for (const QString &name : names) {
            // e.g. "g13:red:macro_preset_1" ends with ":macro_preset_1";
            // a second device of the same family may get a "_1" suffix appended.
            if (name.endsWith(u':' + led.suffix) || name.contains(u':' + led.suffix + u'_')) {
                m_paths.insert(led.role, ledDir.filePath(name));
                break;
            }
        }
        if (!m_paths.contains(led.role))
            qCWarning(lcLeds).noquote() << QStringLiteral("%1: LED \"%2\" not found in %3")
                                               .arg(m_model.name(), led.suffix, ledDir.path());
    }
}

void LedController::setMode(int mode)
{
    for (int m = 1; m <= Profile::ModeCount; ++m)
        writeOnOff(QStringLiteral("mode%1").arg(m), m == mode);
}

void LedController::setRecording(bool recording)
{
    writeOnOff(QStringLiteral("record"), recording);
}

void LedController::setBacklight(const std::optional<Profile::Rgb> &rgb)
{
    if (!rgb)
        return;
    const DeviceModel::Led *led = m_model.ledByRole(QStringLiteral("backlight"));
    if (!led || !m_paths.contains(led->role))
        return;
    if (led->type != DeviceModel::LedType::Rgb) {
        qCDebug(lcLeds) << m_model.name() << "has no RGB backlight; colour ignored";
        return;
    }
    // The kernel scales each intensity by brightness/max_brightness, so the
    // colour is set with full brightness.
    QFile maxFile(m_paths.value(led->role) + QStringLiteral("/max_brightness"));
    QByteArray maximum = "255";
    if (maxFile.open(QIODevice::ReadOnly))
        maximum = maxFile.readAll().trimmed();
    const QByteArray intensity = QByteArray::number(rgb->red) + ' ' + QByteArray::number(rgb->green) + ' '
                                 + QByteArray::number(rgb->blue);
    if (writeAttribute(led->role, QStringLiteral("multi_intensity"), intensity))
        writeAttribute(led->role, QStringLiteral("brightness"), maximum);
}

void LedController::writeOnOff(const QString &role, bool on)
{
    if (m_paths.contains(role))
        writeAttribute(role, QStringLiteral("brightness"), on ? "1" : "0");
}

bool LedController::writeAttribute(const QString &role, const QString &attribute, const QByteArray &value)
{
    // sysfs attributes are written with a single write(); QFile buffering is avoided.
    const QByteArray path = (m_paths.value(role) + u'/' + attribute).toLocal8Bit();
    const int fd = ::open(path.constData(), O_WRONLY | O_CLOEXEC);
    int err = 0;
    if (fd < 0) {
        err = errno;
    } else {
        if (::write(fd, value.constData(), static_cast<size_t>(value.size())) < 0)
            err = errno;
        ::close(fd);
    }
    if (err == 0)
        return true;

    if (err == EACCES || err == EPERM) {
        m_permissionDenied = true;
        if (!m_permissionLogged) {
            m_permissionLogged = true;
            qCWarning(lcLeds).noquote()
                << QStringLiteral("%1: no permission to change the LEDs. Add your user to the group \"gkey\" "
                                  "(sudo usermod -aG gkey $USER), log out and back in, and replug the device.")
                       .arg(m_model.name());
        }
    } else {
        qCWarning(lcLeds).noquote() << QStringLiteral("%1: cannot write %2: %3")
                                           .arg(m_model.name(), QString::fromLocal8Bit(path),
                                                QString::fromLocal8Bit(std::strerror(err)));
    }
    return false;
}
