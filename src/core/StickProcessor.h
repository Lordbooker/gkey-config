#pragma once

#include "StickSettings.h"

#include <QObject>

class BindingEngine;
class KeyOutput;
class QTimer;

// Turns the stick's absolute axes into the action of the profile's stick mode
// (docs/CONCEPT.md 7.1):
//   keys    - the four directions act as keys STICK_UP/DOWN/LEFT/RIGHT; a
//             diagonal presses two of them
//   mouse   - relative pointer movement
//   scroll  - wheel steps
//   passthrough - nothing (the stick is not grabbed; see Device)
//
// The dead zone is centred on the stick's rest position, which is not 128 on
// every device (about 138/139 on the test G13).
class StickProcessor : public QObject {
    Q_OBJECT
public:
    static constexpr int TickMs = 10;            // update rate of mouse and scroll modes
    static constexpr double Hysteresis = 0.05;   // keys mode: release below deadzone - hysteresis

    StickProcessor(BindingEngine &engine, KeyOutput &output, int minimum, int maximum, QObject *parent = nullptr);

    void setSettings(const StickSettings &settings);
    const StickSettings &settings() const { return m_settings; }

    void setCenter(int x, int y);
    int centerX() const { return m_centerX; }
    int centerY() const { return m_centerY; }

    // Last raw position.
    int x() const { return m_x; }
    int y() const { return m_y; }

    void setPosition(int x, int y);
    void setX(int x) { setPosition(x, m_y); }
    void setY(int y) { setPosition(m_x, y); }

    // Forgets pressed directions without sending anything (the engine has
    // released all keys already, e.g. on a profile change).
    void reset();

    // -1..1 relative to the centre (no dead zone applied).
    double normalizedX() const;
    double normalizedY() const;

private:
    void updateKeys();
    void onTick();
    double normalize(int value, int center) const;
    double shaped(double value) const;   // dead zone removed and curved, -1..1

    BindingEngine &m_engine;
    KeyOutput &m_output;
    StickSettings m_settings;
    int m_minimum;
    int m_maximum;
    int m_centerX;
    int m_centerY;
    int m_x;
    int m_y;
    bool m_up = false;
    bool m_down = false;
    bool m_left = false;
    bool m_right = false;
    double m_restX = 0;   // fractional movement not sent yet
    double m_restY = 0;
    QTimer *m_timer = nullptr;
};
