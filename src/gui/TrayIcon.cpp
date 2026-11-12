// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "TrayIcon.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

// The motif of the application icon as a silhouette: the keypad filled, the
// key cap cut out of it, and the "G" filled again inside the cap.
QPixmap silhouette(int size, const QColor &colour)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const qreal s = size / 128.0;   // coordinates of the application icon (128 x 128)
    QPainterPath keypad;
    keypad.addRoundedRect(QRectF(8 * s, 8 * s, 112 * s, 112 * s), 24 * s, 24 * s);
    QPainterPath cap;
    const QRectF capRect(22 * s, 18 * s, 84 * s, 78 * s);
    cap.addRoundedRect(capRect, 14 * s, 14 * s);
    QPainterPath keys;
    for (const int x : {34, 55, 76})
        keys.addRoundedRect(QRectF(x * s, 102 * s, 18 * s, 8 * s), 3 * s, 3 * s);

    QFont font;
    font.setBold(true);
    font.setPixelSize(qMax(8, qRound(70 * s)));
    QPainterPath letter;
    letter.addText(0, 0, font, QStringLiteral("G"));
    const QRectF bounds = letter.boundingRect();
    letter.translate(capRect.center() - bounds.center());

    // The small keys are too fine for the sizes of a panel.
    QPainterPath shape = keypad.subtracted(cap).united(letter);
    if (size >= 48)
        shape = shape.subtracted(keys);
    painter.fillPath(shape, colour);
    return pixmap;
}

}

namespace TrayIcon {

Style styleFromName(const QString &name)
{
    if (name == u"light")
        return Style::Light;
    if (name == u"dark")
        return Style::Dark;
    return Style::Colour;
}

QString styleName(Style style)
{
    switch (style) {
    case Style::Light:
        return QStringLiteral("light");
    case Style::Dark:
        return QStringLiteral("dark");
    case Style::Colour:
        break;
    }
    return QStringLiteral("colour");
}

QIcon icon(Style style, const QIcon &colourIcon)
{
    if (style == Style::Colour)
        return colourIcon;
    // The colours GNOME's and KDE's own one-colour icons have on such panels.
    const QColor colour = style == Style::Light ? QColor(0xf2, 0xf2, 0xf2) : QColor(0x2e, 0x34, 0x36);
    QIcon result;
    for (const int size : {16, 22, 24, 32, 48, 64, 128})
        result.addPixmap(silhouette(size, colour));
    return result;
}

}
