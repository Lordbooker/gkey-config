// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QByteArray>
#include <QLoggingCategory>
#include <QString>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

Q_DECLARE_LOGGING_CATEGORY(lcLcd)

// Writes LCD reports to the hidraw node of a device in a thread of its own.
// A hidraw write is a synchronous USB transfer that can block for seconds when
// the device hangs; in the daemon's main thread that would stop all key
// handling. Only the newest report is kept: one that is replaced while a write
// is running is never written.
//
// The hidraw node is looked up when it is needed (it may appear after the
// input devices), and reopened after a failed write.
class LcdWriter {
public:
    // devDir is "/dev"; tests pass a directory of their own.
    LcdWriter(const QString &deviceName, const QString &hidSyspath, const QString &devDir = QStringLiteral("/dev"));
    // Writes a report that is still pending, then stops the thread.
    ~LcdWriter();
    LcdWriter(const LcdWriter &) = delete;
    LcdWriter &operator=(const LcdWriter &) = delete;

    void submit(const QByteArray &report);

private:
    void run();
    void write(const QByteArray &report);   // writer thread only
    bool open();                            // writer thread only

    const QString m_deviceName;
    const QString m_hidSyspath;
    const QString m_devDir;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::optional<QByteArray> m_pending;   // guarded by m_mutex
    bool m_stop = false;                   // guarded by m_mutex

    int m_fd = -1;                   // writer thread only
    bool m_errorLogged = false;      // writer thread only
    std::thread m_thread;            // last member: starts after all others exist
};
