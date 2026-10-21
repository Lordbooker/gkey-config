#pragma once

#include <QHash>

// Raw output of key events (implemented by the daemon's virtual uinput device
// and by test doubles).
class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void writeKey(int code, bool pressed) = 0;
    virtual void sync() = 0;
};

// Reference-counted key output shared by all devices: a key goes down with the
// first press and up with the last release, so two G keys bound to the same
// key (or two devices) do not release each other's key.
class KeyOutput {
public:
    explicit KeyOutput(OutputSink &sink) : m_sink(sink) {}

    void press(int code);
    void release(int code);
    void sync() { m_sink.sync(); }

    // Number of active presses of a key.
    int pressCount(int code) const { return m_counts.value(code); }

private:
    OutputSink &m_sink;
    QHash<int, int> m_counts;
};
