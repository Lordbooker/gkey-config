// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "LcdFrame.h"
#include "LcdPages.h"

#include <QObject>
#include <QStringList>

class QTimer;

// The LCD of one device (G13: written through its hidraw node). Shows one of
// the enabled pages (status, clock, text); text sent over D-Bus is shown on
// the text page, optionally only for some seconds.
class LcdController : public QObject {
    Q_OBJECT
public:
    LcdController(const DeviceModel &model, const QString &hidSyspath, QObject *parent = nullptr);
    ~LcdController() override;

    // False if the model has no supported LCD.
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
    QString m_hidrawNode;
    int m_fd = -1;
    bool m_errorLogged = false;
    LcdFrame m_frame;
    QStringList m_pages;
    QString m_page;
    QString m_pageBeforeText;
    LcdPages::Status m_status;
    QStringList m_textLines;
    QTimer *m_clockTimer = nullptr;
    QTimer *m_textTimer = nullptr;
};
