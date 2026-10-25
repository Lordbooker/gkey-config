// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "Device.h"

#include "BindingEngine.h"
#include "InputDevice.h"
#include "LcdController.h"
#include "LedController.h"
#include "StickProcessor.h"

#include <QLoggingCategory>
#include <QPoint>

Q_LOGGING_CATEGORY(lcDevice, "gkey.device")

Device::Device(const DeviceModel &model, const QString &hidSyspath, KeyOutput &output, QObject *parent)
    : QObject(parent), m_model(model), m_hidSyspath(hidSyspath)
{
    m_engine = new BindingEngine(m_model, output, this);
    m_leds = std::make_unique<LedController>(m_model, m_hidSyspath);
    if (m_model.lcd())
        m_lcd = new LcdController(m_model, m_hidSyspath, this);
    if (const auto &stick = m_model.stick())
        m_stick = new StickProcessor(*m_engine, output, stick->minimum, stick->maximum, this);
}

Device::~Device()
{
    // Release held keys before the inputs (and their grabs) go away.
    if (m_stick)
        m_stick->reset();
    m_engine->releaseAll();
}

InputDevice *Device::input(const QString &role) const
{
    for (InputDevice *input : m_inputs) {
        if (input->role() == role)
            return input;
    }
    return nullptr;
}

bool Device::addInput(const QString &role, const QString &devNode, QString *error)
{
    if (hasInput(devNode))
        return true;
    // The stick's model data if this input is the stick.
    const DeviceModel::Stick *stickModel = nullptr;
    if (m_stick && m_model.stick() && role == m_model.stick()->input)
        stickModel = &*m_model.stick();
    auto *input = new InputDevice(devNode, role, this);
    const bool grab = !stickModel || m_stickSettings.mode != StickSettings::Mode::Passthrough;
    if (!input->open(error, grab)) {
        delete input;
        return false;
    }
    if (stickModel) {
        connect(input, &InputDevice::keyEvent, this, &Device::onStickKey);
        connect(input, &InputDevice::absEvent, this, &Device::onAbs);
        // Rest position: calibrated value, else the position right now if plausible.
        // Right after boot the device has not reported yet and the axes read 0/0
        // (hid-lg-g15 sets no initial position): not plausible, so the middle.
        const auto &stick = *stickModel;
        const int middle = (stick.minimum + stick.maximum + 1) / 2;
        const int x = input->absValue(stick.axisX);
        const int y = input->absValue(stick.axisY);
        if (m_stickCenter) {
            m_stick->setCenter(m_stickCenter->x(), m_stickCenter->y());
        } else if (m_stick->isPlausibleRest(x, y)) {
            m_stick->setCenter(x, y);
        } else {
            m_stick->setCenter(middle, middle);
            qCInfo(lcDevice).noquote() << m_model.name()
                                       << QStringLiteral(": no stick position yet (%1/%2), using the middle as rest position")
                                              .arg(x)
                                              .arg(y);
        }
        // The values read now are not used as a position: 0/0 before the first
        // report would hold "left" and "up" (e.g. A and W) until the stick moves.
        // Directions only follow real reports of the device.
        m_stick->assumeRest();
    } else {
        connect(input, &InputDevice::keyEvent, m_engine, &BindingEngine::handleKey);
    }
    connect(input, &InputDevice::failed, this, [this](const QString &node) { removeInput(node); });
    m_inputs.append(input);
    return true;
}

bool Device::removeInput(const QString &devNode)
{
    for (int i = 0; i < m_inputs.size(); ++i) {
        if (m_inputs.at(i)->devNode() != devNode)
            continue;
        if (m_stick)
            m_stick->reset();
        m_engine->releaseAll();
        m_inputs.takeAt(i)->deleteLater();
        if (m_inputs.isEmpty())
            emit disconnected();
        return true;
    }
    return false;
}

bool Device::hasInput(const QString &devNode) const
{
    for (const InputDevice *input : m_inputs) {
        if (input->devNode() == devNode)
            return true;
    }
    return false;
}

void Device::setStickSettings(const StickSettings &settings)
{
    m_stickSettings = settings;
    if (!m_stick)
        return;
    m_stick->setSettings(settings);
    applyStickGrab();
}

void Device::applyStickGrab()
{
    if (!m_model.stick())
        return;
    InputDevice *stickInput = input(m_model.stick()->input);
    if (!stickInput)
        return;
    QString error;
    if (!stickInput->setGrabbed(m_stickSettings.mode != StickSettings::Mode::Passthrough, &error))
        qCWarning(lcDevice).noquote() << error;
}

void Device::setStickCenter(const std::optional<QPoint> &center)
{
    m_stickCenter = center;
    if (m_stick && center)
        m_stick->setCenter(center->x(), center->y());
}

std::optional<QPoint> Device::calibrateStick()
{
    if (!m_stick || !m_model.stick() || !input(m_model.stick()->input))
        return std::nullopt;
    const QPoint center(m_stick->x(), m_stick->y());
    setStickCenter(center);
    return center;
}

void Device::onAbs(const QString &, int code, int value)
{
    if (!m_stick || !m_model.stick())
        return;
    const auto &stick = *m_model.stick();
    if (code == stick.axisX)
        m_stick->setX(value);
    else if (code == stick.axisY)
        m_stick->setY(value);
}

void Device::onStickKey(const QString &role, int code, int value)
{
    // In passthrough mode the stick buttons belong to the system (games).
    if (m_stickSettings.mode != StickSettings::Mode::Passthrough)
        m_engine->handleKey(role, code, value);
}
