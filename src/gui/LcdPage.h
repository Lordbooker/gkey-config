// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "LcdPages.h"

#include <QStringList>
#include <QWidget>

#include "SystemStats.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

// LCD settings: which pages are shown (in order) and a preview of each page.
class LcdPage : public QWidget {
    Q_OBJECT
public:
    explicit LcdPage(QWidget *parent = nullptr);

    void setPages(const QStringList &enabled);
    void setStatus(const LcdPages::Status &status);

protected:
    // Restarts the live preview (clock, system) when the tab is shown again.
    void showEvent(QShowEvent *event) override;

signals:
    void pagesChanged(const QStringList &pages);
    void showOnDevice(const QString &page);

private:
    QStringList enabledPages() const;
    void updatePreview();
    static QString title(const QString &page);

    bool m_updating = false;
    LcdPages::Status m_status;
    QListWidget *m_list;
    QLabel *m_preview;
    QPushButton *m_show;
    QTimer *m_clock;   // redraws the clock and system preview every second
    SystemStats m_stats;   // live values for the system preview
};
