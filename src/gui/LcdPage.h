// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "LcdIntro.h"
#include "LcdPages.h"

#include <QStringList>
#include <QWidget>

#include "SystemStats.h"

#include <memory>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

// LCD settings: which pages are shown (in order) and a preview of each page.
// The start animation is the first entry and cannot be moved: it is only shown
// when the device connects, not while paging.
class LcdPage : public QWidget {
    Q_OBJECT
public:
    explicit LcdPage(QWidget *parent = nullptr);

    void setPages(const QStringList &enabled);
    void setStatus(const LcdPages::Status &status);
    // Model whose start animation the preview shows.
    void setModelId(const QString &modelId);

protected:
    // Restarts the live preview (clock, system) when the tab is shown again.
    void showEvent(QShowEvent *event) override;

signals:
    void pagesChanged(const QStringList &pages);
    void showOnDevice(const QString &page);

private:
    QStringList enabledPages() const;
    void updatePreview();
    void keepIntroFirst();
    QListWidgetItem *addItem(const QString &page, bool enabled);
    static QString title(const QString &page);

    bool m_updating = false;
    LcdPages::Status m_status;
    QListWidget *m_list;
    QLabel *m_preview;
    QPushButton *m_show;
    QTimer *m_clock;   // redraws the clock and system preview every second
    SystemStats m_stats;   // live values for the system preview
    QString m_modelId;
    std::unique_ptr<LcdIntro> m_intro;   // preview of the start animation
    QTimer *m_introTimer;
};
