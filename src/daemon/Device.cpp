#include "Device.h"

#include "BindingEngine.h"
#include "InputDevice.h"

Device::Device(const DeviceModel &model, const QString &hidSyspath, KeyOutput &output, QObject *parent)
    : QObject(parent), m_model(model), m_hidSyspath(hidSyspath)
{
    m_engine = new BindingEngine(m_model, output, this);
}

Device::~Device()
{
    // Release held keys before the inputs (and their grabs) go away.
    m_engine->releaseAll();
}

bool Device::addInput(const QString &role, const QString &devNode, QString *error)
{
    if (hasInput(devNode))
        return true;
    auto *input = new InputDevice(devNode, role, this);
    if (!input->open(error)) {
        delete input;
        return false;
    }
    connect(input, &InputDevice::keyEvent, m_engine, &BindingEngine::handleKey);
    connect(input, &InputDevice::failed, this, [this](const QString &node) { removeInput(node); });
    m_inputs.append(input);
    return true;
}

bool Device::removeInput(const QString &devNode)
{
    for (int i = 0; i < m_inputs.size(); ++i) {
        if (m_inputs.at(i)->devNode() != devNode)
            continue;
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
