// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdWriter.h"

#include "SystemError.h"

#include <QDir>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcLcd, "gkey.lcd")

LcdWriter::LcdWriter(const QString &deviceName, const QString &hidSyspath, const QString &devDir)
    : m_deviceName(deviceName), m_hidSyspath(hidSyspath), m_devDir(devDir), m_thread([this] { run(); })
{
}

LcdWriter::~LcdWriter()
{
    {
        const std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_one();
    m_thread.join();
    if (m_fd >= 0)
        ::close(m_fd);
}

void LcdWriter::submit(const QByteArray &report)
{
    {
        const std::lock_guard lock(m_mutex);
        m_pending = report;
    }
    m_wake.notify_one();
}

void LcdWriter::run()
{
    for (;;) {
        QByteArray report;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_pending || m_stop; });
            if (!m_pending)
                return;   // stopped, nothing left to write
            report = std::move(*m_pending);
            m_pending.reset();
        }
        write(report);
    }
}

bool LcdWriter::open()
{
    const QStringList nodes =
        QDir(m_hidSyspath + QStringLiteral("/hidraw")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (nodes.isEmpty()) {
        if (!m_errorLogged) {
            m_errorLogged = true;
            qCWarning(lcLcd).noquote() << QStringLiteral("%1: no hidraw node yet, LCD not written").arg(m_deviceName);
        }
        return false;
    }
    const QString node = m_devDir + u'/' + nodes.first();
    m_fd = ::open(node.toLocal8Bit().constData(), O_WRONLY | O_CLOEXEC);
    if (m_fd < 0) {
        const int err = errno;
        if (!m_errorLogged) {
            m_errorLogged = true;
            qCWarning(lcLcd).noquote() << QStringLiteral("%1: cannot open %2 for the LCD: %3%4")
                                              .arg(m_deviceName, node, SystemError::text(err),
                                                   err == EACCES ? QStringLiteral(" - is the udev rule installed?")
                                                                 : QString());
        }
        return false;
    }
    return true;
}

void LcdWriter::write(const QByteArray &report)
{
    if (m_fd < 0 && !open())
        return;
    if (::write(m_fd, report.constData(), static_cast<size_t>(report.size())) >= 0) {
        m_errorLogged = false;   // report the next problem again
        return;
    }
    const int err = errno;
    if (!m_errorLogged) {
        m_errorLogged = true;
        qCWarning(lcLcd).noquote() << QStringLiteral("%1: LCD write failed: %2")
                                          .arg(m_deviceName, SystemError::text(err));
    }
    ::close(m_fd);   // reopen for the next report
    m_fd = -1;
}
