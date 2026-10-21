#pragma once

#include "Action.h"
#include "Macro.h"

#include <QObject>
#include <QSet>

class KeyOutput;
class QTimer;

// One playback of a macro. Runs on the event loop with timers instead of a
// thread: waits can be cancelled at once and all output stays on one thread.
class MacroRun : public QObject {
    Q_OBJECT
public:
    MacroRun(const Macro &macro, Action::Repeat repeat, int count, KeyOutput &output, QObject *parent = nullptr);
    ~MacroRun() override;

    void start();

    // Stops at once; keys pressed by the macro are released.
    void stop();

    // The bound key was released: "while held" finishes the current pass.
    void keyReleased();

    bool isRunning() const { return m_running; }
    const QString &macroName() const { return m_macro.name(); }

signals:
    void finished();

private:
    void advance();
    bool startNextPass();
    void finish(bool notify = true);

    Macro m_macro;
    Action::Repeat m_repeat;
    int m_count;
    KeyOutput &m_output;
    QTimer *m_timer = nullptr;
    qsizetype m_index = 0;
    int m_pass = 0;
    int m_pendingTap = 0;          // key of a tap waiting for its release
    bool m_keyReleased = false;
    bool m_running = false;
    QSet<int> m_held;              // keys pressed by this run
    int m_passGap = 0;             // ms between passes
};
