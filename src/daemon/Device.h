// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "StickSettings.h"

#include <QList>
#include <QObject>
#include <QPoint>
#include <QString>

#include <memory>
#include <optional>

class BindingEngine;
class InputDevice;
class KeyOutput;
class LcdController;
class LedController;
class StickProcessor;

// One connected physical device (one HID device) with its input nodes, binding
// engine, LEDs, LCD and stick processing.
class Device : public QObject {
    Q_OBJECT
public:
    Device(const DeviceModel &model, const QString &hidSyspath, KeyOutput &output, QObject *parent = nullptr);
    ~Device() override;

    const DeviceModel &model() const { return m_model; }
    const QString &hidSyspath() const { return m_hidSyspath; }
    BindingEngine *engine() const { return m_engine; }
    LedController &leds() const { return *m_leds; }
    LcdController *lcd() const { return m_lcd; }
    StickProcessor *stick() const { return m_stick; }

    // Opens an input node; all are grabbed except the stick in passthrough
    // mode. Returns false and sets an error text on failure.
    bool addInput(const QString &role, const QString &devNode, QString *error);

    // Returns true if the node belonged to this device.
    bool removeInput(const QString &devNode);

    bool hasInput(const QString &devNode) const;
    bool hasInputs() const { return !m_inputs.isEmpty(); }

    // Applies the stick settings of the active profile (call after the
    // engine's profile was set).
    void setStickSettings(const StickSettings &settings);

    // Rest position of the stick; nullopt = use the position read on connect.
    void setStickCenter(const std::optional<QPoint> &center);
    // Takes the current stick position as its rest position.
    std::optional<QPoint> calibrateStick();

signals:
    // All input nodes are gone.
    void disconnected();

private:
    InputDevice *input(const QString &role) const;
    void onAbs(const QString &role, int code, int value);
    void onStickKey(const QString &role, int code, int value);
    void applyStickGrab();

    DeviceModel m_model;
    QString m_hidSyspath;
    BindingEngine *m_engine = nullptr;
    std::unique_ptr<LedController> m_leds;
    LcdController *m_lcd = nullptr;
    StickProcessor *m_stick = nullptr;
    StickSettings m_stickSettings;
    std::optional<QPoint> m_stickCenter;
    QList<InputDevice *> m_inputs;
};
