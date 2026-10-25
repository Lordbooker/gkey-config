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
                               "switches to the next page. The start animation plays when the device connects "
                               "and stays until you switch to the next page."),
                            this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setMaximumHeight(190);
    for (const QString &page : LcdPages::allPages())
        addItem(page, true);
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
    m_introTimer = new QTimer(this);
    m_introTimer->setInterval(LcdIntro::FrameMs);
    connect(m_introTimer, &QTimer::timeout, this, [this] {
        if (m_intro)
            m_intro->step();
        updatePreview();
    });

    connect(m_list, &QListWidget::currentItemChanged, this, [this] {
        m_intro.reset();   // selecting the start animation plays it from the beginning
        updatePreview();
    });
    connect(m_list, &QListWidget::itemChanged, this, [this] {
        if (m_updating)
            return;
        // At least one page besides the start animation stays enabled, so the
        // round button always has a page to go to.
        if (LcdPages::rotation(enabledPages()).isEmpty()) {
            m_updating = true;
            for (int i = 0; i < m_list->count(); ++i) {
                if (m_list->item(i)->data(Qt::UserRole).toString() != LcdPages::IntroPage) {
                    m_list->item(i)->setCheckState(Qt::Checked);
                    break;
                }
            }
            m_updating = false;
        }
        emit pagesChanged(enabledPages());
    });
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        if (m_updating)
            return;
        keepIntroFirst();
        emit pagesChanged(enabledPages());
    });
    connect(m_show, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_list->currentItem())
            emit showOnDevice(item->data(Qt::UserRole).toString());
    });

    m_list->setCurrentRow(0);
}

QListWidgetItem *LcdPage::addItem(const QString &page, bool enabled)
{
    auto *item = new QListWidgetItem(title(page), m_list);
    item->setData(Qt::UserRole, page);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    if (page == LcdPages::IntroPage)
        item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);   // always first
    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    return item;
}

// A page dropped above the start animation: move the animation back to the top.
void LcdPage::keepIntroFirst()
{
    for (int i = 1; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == LcdPages::IntroPage) {
            const bool wasUpdating = m_updating;
            m_updating = true;
            m_list->insertItem(0, m_list->takeItem(i));
            m_updating = wasUpdating;
            return;
        }
    }
}

QString LcdPage::title(const QString &page)
{
    if (page == LcdPages::IntroPage)
        return tr("Start animation (flames, when the device connects)");
    if (page == u"status")
        return tr("Status (profile and layer)");
    if (page == u"system")
        return tr("System (CPU, RAM, GPU, VRAM)");
    if (page == u"cpu")
        return tr("CPU (load, temperatures, clock of each core)");
    if (page == u"amdgpu")
        return tr("AMD GPU (load, clocks, temperatures, VRAM, power, fan)");
    if (page == u"nvidia")
        return tr("NVIDIA GPU (load, clocks, temperature, VRAM, power, fan)");
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
    // The start animation first, then the enabled pages in their order, then the others.
    QStringList order = {LcdPages::IntroPage};
    for (const QString &page : enabled + LcdPages::allPages()) {
        if (!order.contains(page))
            order.append(page);
    }
    for (const QString &page : order) {
        QListWidgetItem *item = addItem(page, enabled.contains(page));
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

void LcdPage::setModelId(const QString &modelId)
{
    if (modelId == m_modelId)
        return;
    m_modelId = modelId;
    m_intro.reset();
    updatePreview();
}

void LcdPage::updatePreview()
{
    const QString page =
        m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QStringLiteral("status");
    LcdFrame frame(160, 43);
    if (page == LcdPages::IntroPage) {
        if (!m_intro)
            m_intro = std::make_unique<LcdIntro>(m_modelId);
        m_intro->render(frame);
    } else if (page == u"clock")
        LcdPages::renderClock(frame, QDateTime::currentDateTime());
    else if (page == u"system")
        LcdPages::renderSystem(frame, m_stats.sample());
    else if (page == u"cpu")
        LcdPages::renderCpu(frame, m_stats.cpuDetail());
    else if (page == u"amdgpu")
        LcdPages::renderGpu(frame, QStringLiteral("AMD"), m_stats.amdGpuDetail(),
                            QStringLiteral("no AMD GPU (amdgpu)"));
    else if (page == u"nvidia") {
        const auto gpu = m_nvidia.sample();
        LcdPages::renderGpu(frame, QStringLiteral("NV"), gpu, m_nvidia.problem());
    } else if (page == u"text")
        LcdPages::renderText(frame, {tr("Text from scripts"), tr("appears here.")});
    else
        LcdPages::renderStatus(frame, m_status);

    static const QStringList livePages = {QStringLiteral("clock"), QStringLiteral("system"), QStringLiteral("cpu"),
                                          QStringLiteral("amdgpu"), QStringLiteral("nvidia")};
    if (livePages.contains(page) && isVisible())
        m_clock->start();
    else
        m_clock->stop();
    if (page == LcdPages::IntroPage && m_intro && !m_intro->finished() && isVisible()) {
        if (!m_introTimer->isActive())
            m_introTimer->start();
    } else {
        m_introTimer->stop();
    }

    QImage image(frame.width() * PreviewScale + 8, frame.height() * PreviewScale + 8, QImage::Format_RGB32);
    image.fill(QColor(0x2b, 0x2d, 0x31));
    QPainter painter(&image);
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            painter.fillRect(4 + x * PreviewScale, 4 + y * PreviewScale, PreviewScale, PreviewScale,
                             frame.pixel(x, y) ? QColor(0x1d, 0x2a, 0x1d) : QColor(0x9c, 0xc8, 0x8c));
    m_preview->setPixmap(QPixmap::fromImage(image));
}

void LcdPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updatePreview();
}
