// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "StickSettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;

// Stick settings of the active profile: mode, dead zone, speeds; WASD preset
// and calibration of the rest position.
class StickPage : public QWidget {
    Q_OBJECT
public:
    explicit StickPage(QWidget *parent = nullptr);

    void setSettings(const StickSettings &settings);
    void setRestPosition(const QString &text);   // e.g. "138 / 139"; empty = unknown
    void setDeviceAvailable(bool available);     // calibration needs the device

signals:
    // Emitted (debounced) when the user changed a setting.
    void settingsChanged(const StickSettings &settings);
    void wasdRequested();
    void calibrateRequested();

private:
    StickSettings current() const;
    void updateEnabled();

    bool m_updating = false;
    QComboBox *m_mode;
    QLabel *m_modeHint;
    QSlider *m_deadzone;
    QLabel *m_deadzoneValue;
    QSlider *m_speed;
    QLabel *m_speedValue;
    QSlider *m_scrollSpeed;
    QLabel *m_scrollValue;
    QPushButton *m_wasd;
    QPushButton *m_calibrate;
    QLabel *m_rest;
    QTimer *m_debounce;
};
