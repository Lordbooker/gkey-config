#include "StickProcessor.h"

#include "BindingEngine.h"
#include "KeyOutput.h"

#include <QTimer>

#include <cmath>
#include <linux/input-event-codes.h>

StickProcessor::StickProcessor(BindingEngine &engine, KeyOutput &output, int minimum, int maximum, QObject *parent)
    : QObject(parent), m_engine(engine), m_output(output), m_minimum(minimum), m_maximum(maximum)
{
    m_centerX = m_centerY = m_x = m_y = (minimum + maximum + 1) / 2;
    m_timer = new QTimer(this);
    m_timer->setInterval(TickMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &StickProcessor::onTick);
}

void StickProcessor::setSettings(const StickSettings &settings)
{
    m_settings = settings;
    reset();
    setPosition(m_x, m_y);
}

void StickProcessor::setCenter(int x, int y)
{
    m_centerX = qBound(m_minimum + 1, x, m_maximum - 1);
    m_centerY = qBound(m_minimum + 1, y, m_maximum - 1);
}

void StickProcessor::reset()
{
    m_up = m_down = m_left = m_right = false;
    m_restX = m_restY = 0;
    m_timer->stop();
}

double StickProcessor::normalize(int value, int center) const
{
    const double range = value >= center ? m_maximum - center : center - m_minimum;
    return range > 0 ? qBound(-1.0, (value - center) / range, 1.0) : 0.0;
}

double StickProcessor::normalizedX() const
{
    return normalize(m_x, m_centerX);
}

double StickProcessor::normalizedY() const
{
    return normalize(m_y, m_centerY);
}

double StickProcessor::shaped(double value) const
{
    const double magnitude = std::abs(value);
    if (magnitude <= m_settings.deadzone)
        return 0.0;
    const double t = (magnitude - m_settings.deadzone) / (1.0 - m_settings.deadzone);
    return std::copysign(t * t, value);   // fine control near the centre
}

void StickProcessor::setPosition(int x, int y)
{
    m_x = x;
    m_y = y;
    switch (m_settings.mode) {
    case StickSettings::Mode::Keys:
        updateKeys();
        break;
    case StickSettings::Mode::Mouse:
    case StickSettings::Mode::Scroll:
        if (shaped(normalizedX()) != 0.0 || shaped(normalizedY()) != 0.0) {
            if (!m_timer->isActive()) {
                onTick();   // react at once, then every tick
                m_timer->start();
            }
        } else {
            m_timer->stop();
            m_restX = m_restY = 0;
        }
        break;
    case StickSettings::Mode::Passthrough:
        break;
    }
}

void StickProcessor::updateKeys()
{
    const double nx = normalizedX();
    const double ny = normalizedY();
    const double on = m_settings.deadzone;
    const double off = qMax(0.0, m_settings.deadzone - Hysteresis);

    // Hysteresis: a direction goes on above the dead zone and off a little
    // below it, so the stick's jitter at the border does not chatter.
    const auto update = [this](bool &state, bool wanted, const char *keyId) {
        if (state == wanted)
            return;
        state = wanted;
        m_engine.handleKeyId(QLatin1String(keyId), wanted);
    };
    update(m_up, m_up ? ny < -off : ny < -on, "STICK_UP");
    update(m_down, m_down ? ny > off : ny > on, "STICK_DOWN");
    update(m_left, m_left ? nx < -off : nx < -on, "STICK_LEFT");
    update(m_right, m_right ? nx > off : nx > on, "STICK_RIGHT");
}

void StickProcessor::onTick()
{
    const double sx = shaped(normalizedX());
    const double sy = shaped(normalizedY());
    if (m_settings.mode == StickSettings::Mode::Mouse) {
        m_restX += sx * m_settings.speed;
        m_restY += sy * m_settings.speed;
        const int dx = static_cast<int>(m_restX);
        const int dy = static_cast<int>(m_restY);
        m_restX -= dx;
        m_restY -= dy;
        if (dx)
            m_output.moveRelative(REL_X, dx);
        if (dy)
            m_output.moveRelative(REL_Y, dy);
        if (dx || dy)
            m_output.sync();
    } else if (m_settings.mode == StickSettings::Mode::Scroll) {
        // scrollSpeed = wheel steps per second at full deflection
        const double perTick = m_settings.scrollSpeed * TickMs / 1000.0;
        m_restX += sx * perTick;
        m_restY += -sy * perTick;   // stick up = wheel up (positive)
        const int dx = static_cast<int>(m_restX);
        const int dy = static_cast<int>(m_restY);
        m_restX -= dx;
        m_restY -= dy;
        if (dx)
            m_output.moveRelative(REL_HWHEEL, dx);
        if (dy)
            m_output.moveRelative(REL_WHEEL, dy);
        if (dx || dy)
            m_output.sync();
    }
}
