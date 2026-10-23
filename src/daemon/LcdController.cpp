// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdController.h"

#include <QDateTime>
#include <QTimer>

LcdController::LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent)
    : QObject(parent), m_model(model),
      m_frame(model.lcd() ? model.lcd()->width : 1, model.lcd() ? model.lcd()->height : 1)
{
    m_pages = LcdPages::allPages();
    m_page = m_pages.first();

    // G13, G15 and G510 take the same 992-byte frame (output report 3) on their
    // hidraw node; "g13-hidraw" is the name the format got first.
    if (!model.lcd() || (model.lcd()->type != u"g13-hidraw" && model.lcd()->type != u"g15-hidraw"))
        return;
    m_available = true;
    m_writer = std::make_unique<LcdWriter>(model.name(), hidSyspath);

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
    if (m_writer) {
        m_frame.clear();   // do not leave stale content behind
        write();           // the writer writes it before its thread ends
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
    else if (m_page == u"system")
        LcdPages::renderSystem(m_frame, m_stats.sample());
    else if (m_page == u"text")
        LcdPages::renderText(m_frame, m_textLines);
    else
        LcdPages::renderStatus(m_frame, m_status);

    if (m_clockTimer) {
        const bool ticking = m_page == u"clock" || m_page == u"system";
        if (ticking && !m_clockTimer->isActive())
            m_clockTimer->start(1000 - QTime::currentTime().msec());
        else if (!ticking)
            m_clockTimer->stop();
    }
    write();
}

void LcdController::write()
{
    if (m_writer)
        m_writer->submit(m_frame.toG13Report());
}
