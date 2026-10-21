#include "MacroRun.h"

#include "KeyOutput.h"

#include <QTimer>

MacroRun::MacroRun(const Macro &macro, Action::Repeat repeat, int count, KeyOutput &output, QObject *parent)
    : QObject(parent), m_macro(macro), m_repeat(repeat), m_count(count), m_output(output)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setTimerType(Qt::PreciseTimer);
    // A repeating macro without any wait would flood the output and keep the
    // event loop busy: give such macros a short pause between passes.
    int totalWait = 0;
    for (const Macro::Step &step : m_macro.steps())
        totalWait += step.duration;
    m_passGap = totalWait > 0 ? 0 : 10;

    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_pendingTap) {
            m_held.remove(m_pendingTap);
            m_output.release(m_pendingTap);
            m_output.sync();
            m_pendingTap = 0;
        }
        advance();
    });
}

MacroRun::~MacroRun()
{
    if (m_running)
        finish(false);   // no signal from a half-destroyed object
}

void MacroRun::start()
{
    if (m_running)
        return;
    m_running = true;
    m_index = 0;
    m_pass = 0;
    m_keyReleased = false;
    if (m_macro.steps().isEmpty()) {
        finish();
        return;
    }
    advance();
}

void MacroRun::stop()
{
    if (!m_running)
        return;
    m_timer->stop();
    finish();
}

void MacroRun::keyReleased()
{
    m_keyReleased = true;
}

void MacroRun::advance()
{
    while (m_running) {
        if (m_index >= m_macro.steps().size()) {
            m_output.sync();
            if (!startNextPass()) {
                finish();
                return;
            }
            // Give the event loop a turn between passes, so a macro without
            // delays cannot block the daemon.
            m_timer->start(m_passGap);
            return;
        }

        const Macro::Step &step = m_macro.steps().at(m_index++);
        switch (step.type) {
        case Macro::Step::Type::Press:
            if (!m_held.contains(step.code)) {
                m_held.insert(step.code);
                m_output.press(step.code);
            }
            break;
        case Macro::Step::Type::Release:
            if (m_held.remove(step.code))
                m_output.release(step.code);
            break;
        case Macro::Step::Type::Tap:
            if (m_held.contains(step.code)) {   // already held by a "press" step
                m_held.remove(step.code);
                m_output.release(step.code);
            }
            m_held.insert(step.code);
            m_output.press(step.code);
            m_output.sync();
            m_pendingTap = step.code;
            m_timer->start(step.duration);
            return;
        case Macro::Step::Type::Delay:
            m_output.sync();
            m_timer->start(step.duration);
            return;
        }
    }
}

bool MacroRun::startNextPass()
{
    ++m_pass;
    bool again = false;
    switch (m_repeat) {
    case Action::Repeat::Once:
        again = false;
        break;
    case Action::Repeat::Count:
        again = m_pass < m_count;
        break;
    case Action::Repeat::WhileHeld:
        again = !m_keyReleased;
        break;
    case Action::Repeat::Toggle:
        again = true;
        break;
    }
    if (again)
        m_index = 0;
    return again;
}

void MacroRun::finish(bool notify)
{
    m_running = false;
    m_pendingTap = 0;
    for (int code : std::as_const(m_held))
        m_output.release(code);
    m_held.clear();
    m_output.sync();
    if (notify)
        emit finished();
}
