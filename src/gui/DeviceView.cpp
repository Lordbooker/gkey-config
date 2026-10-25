// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceView.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <ranges>

DeviceView::DeviceView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setMinimumSize(250, 250);
}

void DeviceView::setModel(const DeviceModel &model)
{
    m_model = model;
    const QList<QString> ids = model.bindableKeyIds();
    m_bindable = QSet<QString>(ids.begin(), ids.end());
    m_bound.clear();
    m_pressed.clear();
    m_selected.clear();
    m_hover.clear();
    updateGeometry();
    update();
}

void DeviceView::setBoundKeys(const QSet<QString> &keys)
{
    m_bound = keys;
    update();
}

void DeviceView::setSelectedKey(const QString &key)
{
    m_selected = key;
    update();
}

void DeviceView::setKeyPressed(const QString &key, bool pressed)
{
    if (pressed)
        m_pressed.insert(key);
    else
        m_pressed.remove(key);
    update();
}

void DeviceView::setAccentColor(const QColor &color)
{
    m_accent = color;
    update();
}

QSize DeviceView::sizeHint() const
{
    const auto &view = m_model.view();
    if (view.width <= 0)
        return {400, 500};
    return {qRound(view.width), qRound(view.height)};
}

int DeviceView::heightForWidth(int width) const
{
    const auto &view = m_model.view();
    return view.width > 0 ? qRound(width * view.height / view.width) : width;
}

QTransform DeviceView::viewToWidget() const
{
    const auto &view = m_model.view();
    if (view.width <= 0 || view.height <= 0)
        return {};
    const qreal scale = qMin(width() / view.width, height() / view.height);
    const qreal dx = (width() - view.width * scale) / 2.0;
    const qreal dy = (height() - view.height * scale) / 2.0;
    return QTransform::fromTranslate(dx, dy).scale(scale, scale);
}

QPainterPath DeviceView::pathFor(const DeviceModel::Shape &shape)
{
    QPainterPath path;
    const QRectF rect(shape.x, shape.y, shape.width, shape.height);
    if (shape.shape == DeviceModel::AreaShape::Ellipse)
        path.addEllipse(rect);
    else
        path.addRoundedRect(rect, shape.radius, shape.radius);
    return path;
}

bool DeviceView::isBindable(const QString &key) const
{
    return m_bindable.contains(key);
}

QString DeviceView::keyAt(const QPointF &widgetPos) const
{
    const QPointF pos = viewToWidget().inverted().map(widgetPos);
    const auto &areas = m_model.view().areas;
    // Last drawn is on top (e.g. the stick press over the stick directions).
    for (const auto &area : std::views::reverse(areas)) {
        if (pathFor(area).contains(pos))
            return area.key;
    }
    return {};
}

void DeviceView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setTransform(viewToWidget());

    const QPalette pal = palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 128;
    const QColor bodyFill = dark ? QColor(0x2b, 0x2d, 0x31) : QColor(0xdc, 0xdd, 0xe0);
    const QColor bodyLine = dark ? QColor(0x55, 0x58, 0x5e) : QColor(0x9a, 0x9c, 0xa1);
    const QColor keyFill = dark ? QColor(0x3a, 0x3d, 0x42) : QColor(0xf6, 0xf6, 0xf7);
    const QColor keyLine = dark ? QColor(0x6a, 0x6e, 0x75) : QColor(0x8a, 0x8c, 0x91);
    const QColor fixedFill = dark ? QColor(0x30, 0x32, 0x36) : QColor(0xe4, 0xe5, 0xe8);
    const QColor text = pal.color(QPalette::WindowText);
    const QColor highlight = pal.color(QPalette::Highlight);
    const QColor accent = m_accent.isValid() ? m_accent : highlight;

    // Decorations: body parts first, then details on top.
    QPainterPath body;
    for (const auto &decoration : m_model.view().decorations) {
        if (decoration.role == u"body")
            body = body.united(pathFor(decoration));
    }
    painter.setPen(QPen(bodyLine, 2));
    painter.setBrush(bodyFill);
    painter.drawPath(body.simplified());

    for (const auto &decoration : m_model.view().decorations) {
        const QPainterPath path = pathFor(decoration);
        if (decoration.role == u"lcd") {
            painter.setPen(QPen(bodyLine, 1.5));
            painter.setBrush(QColor(0x1d, 0x22, 0x1f));
            painter.drawPath(path);
            QColor glow = accent;
            glow.setAlpha(60);
            painter.setBrush(glow);
            painter.setPen(Qt::NoPen);
            painter.drawPath(path);
        } else if (decoration.role != u"body") {
            painter.setPen(QPen(bodyLine, 1.5));
            painter.setBrush(dark ? bodyFill.darker(115) : bodyFill.darker(108));
            painter.drawPath(path);
        }
    }

    // Keys
    QFont font = painter.font();
    font.setPixelSize(11);
    font.setBold(true);
    painter.setFont(font);
    for (const auto &area : m_model.view().areas) {
        const QPainterPath path = pathFor(area);
        const bool bindable = isBindable(area.key);
        const bool pressed = m_pressed.contains(area.key);
        const bool selected = area.key == m_selected;
        const bool hover = area.key == m_hover && bindable;

        QColor fill = bindable ? keyFill : fixedFill;
        if (hover)
            fill = fill.lighter(dark ? 125 : 97);
        if (pressed) {
            fill = accent;
            fill.setAlpha(200);
        }
        painter.setBrush(fill);
        painter.setPen(QPen(selected ? highlight : keyLine, selected ? 3 : 1.2));
        painter.drawPath(path);

        if (bindable && m_bound.contains(area.key)) {
            // Small dot in the corner: the key has a binding in this mode.
            const QRectF r = path.boundingRect();
            const qreal size = qMin<qreal>(7, qMin(r.width(), r.height()) / 3);
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            painter.drawEllipse(QPointF(r.right() - size, r.top() + size), size / 2, size / 2);
        }

        if (!area.label.isEmpty()) {
            QColor labelColor = text;
            if (!bindable)
                labelColor.setAlpha(120);
            painter.setPen(labelColor);
            painter.drawText(path.boundingRect(), Qt::AlignCenter, area.label);
        }
    }
}

void DeviceView::mouseMoveEvent(QMouseEvent *event)
{
    const QString key = keyAt(event->position());
    if (key != m_hover) {
        m_hover = key;
        setCursor(isBindable(key) ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void DeviceView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QString key = keyAt(event->position());
    if (isBindable(key)) {
        setSelectedKey(key);
        emit keyClicked(key);
    }
}

void DeviceView::leaveEvent(QEvent *)
{
    m_hover.clear();
    update();
}

bool DeviceView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QString key = keyAt(help->pos());
        if (key.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(help->globalPos(), isBindable(key) ? key : tr("%1 (fixed function)").arg(key), this);
        return true;
    }
    return QWidget::event(event);
}
