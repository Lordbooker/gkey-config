// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdPage.h"

#include "LcdFrame.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int PreviewScale = 3;

}

LcdPage::LcdPage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(tr("Pages shown on the LCD, in this order. The round button next to the LCD keys "
                               "switches to the next page (unless it is bound to something else)."),
                            this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setMaximumHeight(110);
    for (const QString &page : LcdPages::allPages()) {
        auto *item = new QListWidgetItem(title(page), m_list);
        item->setData(Qt::UserRole, page);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    layout->addWidget(m_list);

    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_preview);

    m_show = new QPushButton(tr("Show this page on the device"), this);
    layout->addWidget(m_show, 0, Qt::AlignLeft);

    auto *textHint = new QLabel(
        tr("Scripts can write to the text page, e.g.:<br><tt>busctl --user call io.github.lordbooker.GKeyConfig "
           "/io/github/lordbooker/GKeyConfig io.github.lordbooker.GKeyConfig1 LcdText si \"Hello\" 5</tt>"),
        this);
    textHint->setWordWrap(true);
    textHint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(textHint);
    layout->addStretch();

    m_clock = new QTimer(this);
    m_clock->setInterval(1000);
    connect(m_clock, &QTimer::timeout, this, &LcdPage::updatePreview);

    connect(m_list, &QListWidget::currentItemChanged, this, &LcdPage::updatePreview);
    connect(m_list, &QListWidget::itemChanged, this, [this] {
        if (m_updating)
            return;
        // At least one page stays enabled.
        if (enabledPages().isEmpty()) {
            m_updating = true;
            m_list->item(0)->setCheckState(Qt::Checked);
            m_updating = false;
        }
        emit pagesChanged(enabledPages());
    });
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        if (!m_updating)
            emit pagesChanged(enabledPages());
    });
    connect(m_show, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_list->currentItem())
            emit showOnDevice(item->data(Qt::UserRole).toString());
    });

    m_list->setCurrentRow(0);
}

QString LcdPage::title(const QString &page)
{
    if (page == u"status")
        return tr("Status (profile and layer)");
    if (page == u"clock")
        return tr("Clock");
    if (page == u"text")
        return tr("Text (from scripts)");
    return page;
}

void LcdPage::setPages(const QStringList &enabled)
{
    m_updating = true;
    const QString current = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
    m_list->clear();
    QStringList order = enabled;
    for (const QString &page : LcdPages::allPages()) {
        if (!order.contains(page))
            order.append(page);
    }
    for (const QString &page : order) {
        auto *item = new QListWidgetItem(title(page), m_list);
        item->setData(Qt::UserRole, page);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(enabled.contains(page) ? Qt::Checked : Qt::Unchecked);
        if (page == current)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem())
        m_list->setCurrentRow(0);
    m_updating = false;
    updatePreview();
}

QStringList LcdPage::enabledPages() const
{
    QStringList pages;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->checkState() == Qt::Checked)
            pages.append(m_list->item(i)->data(Qt::UserRole).toString());
    }
    return pages;
}

void LcdPage::setStatus(const LcdPages::Status &status)
{
    m_status = status;
    updatePreview();
}

void LcdPage::updatePreview()
{
    const QString page = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString()
                                               : QStringLiteral("status");
    LcdFrame frame(160, 43);
    if (page == u"clock")
        LcdPages::renderClock(frame, QDateTime::currentDateTime());
    else if (page == u"text")
        LcdPages::renderText(frame, {tr("Text from scripts"), tr("appears here.")});
    else
        LcdPages::renderStatus(frame, m_status);

    if (page == u"clock" && isVisible())
        m_clock->start();
    else
        m_clock->stop();

    QImage image(frame.width() * PreviewScale + 8, frame.height() * PreviewScale + 8, QImage::Format_RGB32);
    image.fill(QColor(0x2b, 0x2d, 0x31));
    QPainter painter(&image);
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            painter.fillRect(4 + x * PreviewScale, 4 + y * PreviewScale, PreviewScale, PreviewScale,
                             frame.pixel(x, y) ? QColor(0x1d, 0x2a, 0x1d) : QColor(0x9c, 0xc8, 0x8c));
    m_preview->setPixmap(QPixmap::fromImage(image));
}
