// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdController.h"

#include <QDateTime>
#include <QTimer>

#include <utility>

LcdController::LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent)
    : QObject(parent), m_model(model),
      m_frame(model.lcd() ? model.lcd()->width : 1, model.lcd() ? model.lcd()->height : 1)
{
    m_pages = LcdPages::defaultPages();
    m_page = LcdPages::rotation(m_pages).first();

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
    m_returnTimer = new QTimer(this);
    m_returnTimer->setSingleShot(true);
    connect(m_returnTimer, &QTimer::timeout, this, &LcdController::returnFromTemporaryPage);
    m_introTimer = new QTimer(this);
    m_introTimer->setTimerType(Qt::PreciseTimer);
    m_introTimer->setInterval(LcdIntro::FrameMs);
    connect(m_introTimer, &QTimer::timeout, this, [this] {
        if (m_intro)
            m_intro->step();
        refresh();
    });
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
    m_pages = pages.isEmpty() ? LcdPages::defaultPages() : pages;
    const QStringList rotation = LcdPages::rotation(m_pages);
    const bool temporary = m_returnTimer && m_returnTimer->isActive();
    if (!m_pages.contains(m_page) && !temporary)
        m_page = rotation.isEmpty() ? LcdPages::IntroPage : rotation.first();
    refresh();
}

void LcdController::start()
{
    if (m_pages.contains(LcdPages::IntroPage)) {
        playIntro();
        return;
    }
    m_page = LcdPages::rotation(m_pages).value(0, QStringLiteral("status"));
    refresh();
}

void LcdController::playIntro()
{
    if (m_returnTimer)
        m_returnTimer->stop();
    m_intro = std::make_unique<LcdIntro>(m_model.id());
    m_page = LcdPages::IntroPage;
    refresh();   // starts the frame timer
}

void LcdController::setStatus(const LcdPages::Status &status)
{
    // A change of the profile (not the first one when the device connects)
    // shows the profile page for a moment.
    const bool profileChanged = !m_status.profile.isEmpty() && status.profile != m_status.profile;
    m_status = status;
    if (profileChanged && m_pages.contains(LcdPages::ProfilePage)) {
        showTemporarily(LcdPages::ProfilePage, ProfileSeconds * 1000);
        return;
    }
    if (m_page == u"status" || m_page == LcdPages::ProfilePage)
        refresh();
}

void LcdController::showPage(const QString &page)
{
    if (page == LcdPages::IntroPage) {
        playIntro();
        return;
    }
    QString target = page;
    if (page == u"next" || page == u"previous") {
        // The start animation is not part of the rotation: from it (and from a
        // page that is not enabled) "next" goes to the first page.
        const QStringList rotation = LcdPages::rotation(m_pages);
        if (rotation.isEmpty())
            return;
        const qsizetype index = rotation.indexOf(m_page);
        const qsizetype count = rotation.size();
        if (index < 0)
            target = page == u"next" ? rotation.first() : rotation.last();
        else
            target = rotation.at((index + (page == u"next" ? 1 : count - 1)) % count);
    }
    if (!LcdPages::allPages().contains(target))
        return;
    if (m_returnTimer)
        m_returnTimer->stop();
    m_page = std::move(target);
    refresh();
}

void LcdController::setText(const QString &text, int seconds)
{
    m_textLines = text.split(u'\n');
    const bool temporaryShown = m_returnTimer && m_returnTimer->isActive();
    if (seconds > 0 && (temporaryShown || m_page != u"text")) {
        showTemporarily(QStringLiteral("text"), seconds * 1000);
        return;
    }
    // Without a duration (or onto the text page shown for good) the text stays.
    if (m_returnTimer)
        m_returnTimer->stop();
    m_page = QStringLiteral("text");
    refresh();
}

void LcdController::showTemporarily(const QString &page, int milliseconds)
{
    // A temporary page that replaces another one keeps the original return page.
    const bool temporaryShown = m_returnTimer && m_returnTimer->isActive();
    if (!temporaryShown)
        m_returnPage = m_page;
    m_page = page;
    if (m_returnTimer)
        m_returnTimer->start(milliseconds);
    refresh();
}

void LcdController::returnFromTemporaryPage()
{
    if (m_returnPage == LcdPages::IntroPage) {
        // Back to the start animation where it was (or its final image), not a replay.
        m_page = LcdPages::IntroPage;
        refresh();
        return;
    }
    showPage(m_returnPage);
}

void LcdController::refresh()
{
    if (m_page == LcdPages::IntroPage) {
        if (!m_intro)
            m_intro = std::make_unique<LcdIntro>(m_model.id());
        m_intro->render(m_frame);
    } else if (m_page == u"clock")
        LcdPages::renderClock(m_frame, QDateTime::currentDateTime());
    else if (m_page == u"system")
        LcdPages::renderSystem(m_frame, m_stats.sample());
    else if (m_page == u"cpu")
        LcdPages::renderCpu(m_frame, m_stats.cpuDetail());
    else if (m_page == u"amdgpu")
        LcdPages::renderGpu(m_frame, QStringLiteral("AMD"), m_stats.amdGpuDetail(),
                            QStringLiteral("no AMD GPU (amdgpu)"));
    else if (m_page == u"nvidia") {
        const auto gpu = m_nvidia.sample();
        LcdPages::renderGpu(m_frame, QStringLiteral("NV"), gpu, m_nvidia.problem());
    } else if (m_page == u"text")
        LcdPages::renderText(m_frame, m_textLines);
    else if (m_page == LcdPages::ProfilePage)
        LcdPages::renderProfile(m_frame, m_status.profile);
    else
        LcdPages::renderStatus(m_frame, m_status);

    if (m_clockTimer) {
        static const QStringList tickingPages = {QStringLiteral("clock"), QStringLiteral("system"),
                                                 QStringLiteral("cpu"), QStringLiteral("amdgpu"),
                                                 QStringLiteral("nvidia")};
        const bool ticking = tickingPages.contains(m_page);
        if (ticking && !m_clockTimer->isActive())
            m_clockTimer->start(1000 - QTime::currentTime().msec());
        else if (!ticking)
            m_clockTimer->stop();
    }
    if (m_introTimer) {
        const bool animating = m_page == LcdPages::IntroPage && m_intro && !m_intro->finished();
        if (animating && !m_introTimer->isActive())
            m_introTimer->start();
        else if (!animating)
            m_introTimer->stop();
    }
    write();
}

void LcdController::write()
{
    if (m_writer)
        m_writer->submit(m_frame.toG13Report());
}
