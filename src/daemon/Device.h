#pragma once

#include "DeviceModel.h"

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

class BindingEngine;
class InputDevice;
class KeyOutput;
class LedController;

// One connected physical device (one HID device) with its grabbed input
// nodes and its binding engine.
class Device : public QObject {
    Q_OBJECT
public:
    Device(const DeviceModel &model, const QString &hidSyspath, KeyOutput &output, QObject *parent = nullptr);
    ~Device() override;

    const DeviceModel &model() const { return m_model; }
    const QString &hidSyspath() const { return m_hidSyspath; }
    BindingEngine *engine() const { return m_engine; }
    LedController &leds() const { return *m_leds; }

    // Opens and grabs an input node. Returns false and sets an error text on failure.
    bool addInput(const QString &role, const QString &devNode, QString *error);

    // Returns true if the node belonged to this device.
    bool removeInput(const QString &devNode);

    bool hasInput(const QString &devNode) const;
    bool hasInputs() const { return !m_inputs.isEmpty(); }

signals:
    // All input nodes are gone.
    void disconnected();

private:
    DeviceModel m_model;
    QString m_hidSyspath;
    BindingEngine *m_engine = nullptr;
    std::unique_ptr<LedController> m_leds;
    QList<InputDevice *> m_inputs;
};
