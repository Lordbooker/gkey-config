// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "LcdFrame.h"
#include "LcdPages.h"
#include "LcdWriter.h"
#include "SystemStats.h"

#include <QObject>
#include <QStringList>

#include <memory>

class QTimer;

// The LCD of one device (written through its hidraw node by an LcdWriter in a
// thread of its own). Shows one of
// the enabled pages (status, system, clock, text); text sent over D-Bus is
// shown on the text page, optionally only for some seconds. Clock and system
// page are redrawn every second while they are shown.
class LcdController : public QObject {
    Q_OBJECT
public:
    LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent = nullptr);
    ~LcdController() override;

    // False if the model has no supported LCD (whether the hidraw node can be
    // written is only known when the writer tries).
    bool available() const { return m_available; }

    void setPages(const QStringList &pages);
    void setStatus(const LcdPages::Status &status);

    // "next", "previous" or a page name.
    void showPage(const QString &page);
    const QString &currentPage() const { return m_page; }

    // Shows text on the text page; seconds > 0 returns to the previous page afterwards.
    void setText(const QString &text, int seconds);

    void refresh();

    // Current frame, e.g. for tests.
    const LcdFrame &frame() const { return m_frame; }

private:
    void write();

    DeviceModel m_model;
    bool m_available = false;
    std::unique_ptr<LcdWriter> m_writer;   // only for a supported LCD
    LcdFrame m_frame;
    QStringList m_pages;
    QString m_page;
    QString m_pageBeforeText;
    LcdPages::Status m_status;
    QStringList m_textLines;
    SystemStats m_stats;
    QTimer *m_clockTimer = nullptr;   // ticks while the clock or the system page is shown
    QTimer *m_textTimer = nullptr;
};
