#pragma once

#include "LcdPages.h"

#include <QStringList>
#include <QWidget>

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
    QTimer *m_clock;
};
