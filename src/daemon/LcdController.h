// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "LcdFrame.h"
#include "LcdIntro.h"
#include "LcdPages.h"
#include "LcdWriter.h"
#include "NvidiaStats.h"
#include "SystemStats.h"

#include <QObject>
#include <QStringList>

#include <memory>

class QTimer;

// The LCD of one device (written through its hidraw node by an LcdWriter in a
// thread of its own). Shows one of
// the enabled pages (status, system, cpu, amdgpu, nvidia, clock, text); text
// sent over D-Bus is shown on the text page, optionally only for some seconds.
// When the profile changes, the profile page (if enabled) shows its name for
// a few seconds.
// The clock and the pages with system values are redrawn every second while
// they are shown.
class LcdController : public QObject {
    Q_OBJECT
public:
    LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent = nullptr);
    ~LcdController() override;

    // False if the model has no supported LCD (whether the hidraw node can be
    // written is only known when the writer tries).
    bool available() const { return m_available; }

    void setPages(const QStringList &pages);
    // First content after the device connected: the start animation if it is
    // enabled (its final image stays until the page is changed), else the
    // first page. Call after setPages().
    void start();
    void setStatus(const LcdPages::Status &status);

    // "next", "previous" or a page name; "intro" plays the start animation again.
    void showPage(const QString &page);
    const QString &currentPage() const { return m_page; }

    // Shows text on the text page; seconds > 0 returns to the previous page afterwards.
    void setText(const QString &text, int seconds);

    // How long the profile page is shown after a change of the profile.
    static constexpr int ProfileSeconds = 3;

    void refresh();

    // Current frame, e.g. for tests.
    const LcdFrame &frame() const { return m_frame; }

private:
    void write();
    void playIntro();
    void showTemporarily(const QString &page, int milliseconds);
    void returnFromTemporaryPage();

    DeviceModel m_model;
    bool m_available = false;
    std::unique_ptr<LcdWriter> m_writer;   // only for a supported LCD
    LcdFrame m_frame;
    QStringList m_pages;
    QString m_page;
    QString m_returnPage;   // page shown again after a temporary page
    LcdPages::Status m_status;
    QStringList m_textLines;
    SystemStats m_stats;
    NvidiaStats m_nvidia;
    QTimer *m_clockTimer = nullptr;   // ticks while the clock or a page with system values is shown
    QTimer *m_returnTimer = nullptr;   // runs while a temporary page is shown
    std::unique_ptr<LcdIntro> m_intro;   // start animation (kept for its final image)
    QTimer *m_introTimer = nullptr;      // next animation frame
};
