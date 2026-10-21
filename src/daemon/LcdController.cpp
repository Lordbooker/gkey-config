#include "LcdController.h"

#include <QDateTime>
#include <QDir>
#include <QLoggingCategory>
#include <QTimer>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcLcd, "gkey.lcd")

LcdController::LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent)
    : QObject(parent), m_model(model),
      m_frame(model.lcd() ? model.lcd()->width : 1, model.lcd() ? model.lcd()->height : 1)
{
    m_pages = LcdPages::allPages();
    m_page = m_pages.first();

    if (!model.lcd() || model.lcd()->type != u"g13-hidraw")
        return;
    const QStringList nodes = QDir(hidSyspath + QStringLiteral("/hidraw")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (nodes.isEmpty()) {
        qCWarning(lcLcd).noquote() << model.name() << ": no hidraw node found, LCD disabled";
        return;
    }
    m_hidrawNode = QStringLiteral("/dev/") + nodes.first();
    m_available = true;

    m_clockTimer = new QTimer(this);
    m_clockTimer->setTimerType(Qt::PreciseTimer);
    connect(m_clockTimer, &QTimer::timeout, this, [this] {
        refresh();
        // Re-align with the start of the next second.
        m_clockTimer->start(1000 - QTime::currentTime().msec());
    });
    m_textTimer = new QTimer(this);
    m_textTimer->setSingleShot(true);
    connect(m_textTimer, &QTimer::timeout, this, [this] { showPage(m_pageBeforeText); });
}

LcdController::~LcdController()
{
    if (m_fd >= 0) {
        m_frame.clear();   // do not leave stale content behind
        write();
        ::close(m_fd);
    }
}

void LcdController::setPages(const QStringList &pages)
{
    m_pages = pages.isEmpty() ? LcdPages::allPages() : pages;
    if (!m_pages.contains(m_page) && !(m_page == u"text" && m_textTimer && m_textTimer->isActive()))
        m_page = m_pages.first();
    refresh();
}

void LcdController::setStatus(const LcdPages::Status &status)
{
    m_status = status;
    if (m_page == u"status")
        refresh();
}

void LcdController::showPage(const QString &page)
{
    QString target = page;
    if (page == u"next" || page == u"previous") {
        const qsizetype index = m_pages.indexOf(m_page);
        const qsizetype count = m_pages.size();
        if (index < 0)
            target = m_pages.first();
        else
            target = m_pages.at((index + (page == u"next" ? 1 : count - 1)) % count);
    }
    if (!LcdPages::allPages().contains(target))
        return;
    if (m_textTimer)
        m_textTimer->stop();
    m_page = target;
    refresh();
}

void LcdController::setText(const QString &text, int seconds)
{
    m_textLines = text.split(u'\n');
    const bool temporaryTextShown = m_textTimer && m_textTimer->isActive();
    if (!temporaryTextShown && m_page != u"text")
        m_pageBeforeText = m_page;   // where to return after a temporary text
    m_page = QStringLiteral("text");
    if (m_textTimer) {
        if (seconds > 0 && !m_pageBeforeText.isEmpty())
            m_textTimer->start(seconds * 1000);
        else
            m_textTimer->stop();
    }
    refresh();
}

void LcdController::refresh()
{
    if (m_page == u"clock")
        LcdPages::renderClock(m_frame, QDateTime::currentDateTime());
    else if (m_page == u"text")
        LcdPages::renderText(m_frame, m_textLines);
    else
        LcdPages::renderStatus(m_frame, m_status);

    if (m_clockTimer) {
        if (m_page == u"clock" && !m_clockTimer->isActive())
            m_clockTimer->start(1000 - QTime::currentTime().msec());
        else if (m_page != u"clock")
            m_clockTimer->stop();
    }
    write();
}

void LcdController::write()
{
    if (!m_available)
        return;
    if (m_fd < 0) {
        m_fd = ::open(m_hidrawNode.toLocal8Bit().constData(), O_WRONLY | O_CLOEXEC);
        if (m_fd < 0) {
            if (!m_errorLogged) {
                m_errorLogged = true;
                const int err = errno;
                qCWarning(lcLcd).noquote() << QStringLiteral("%1: cannot open %2 for the LCD: %3%4")
                                                  .arg(m_model.name(), m_hidrawNode,
                                                       QString::fromLocal8Bit(std::strerror(err)),
                                                       err == EACCES ? QStringLiteral(" - is the udev rule installed?")
                                                                     : QString());
            }
            return;
        }
    }
    const QByteArray report = m_frame.toG13Report();
    if (::write(m_fd, report.constData(), static_cast<size_t>(report.size())) < 0 && !m_errorLogged) {
        m_errorLogged = true;
        qCWarning(lcLcd).noquote() << QStringLiteral("%1: LCD write failed: %2")
                                          .arg(m_model.name(), QString::fromLocal8Bit(std::strerror(errno)));
    }
}
