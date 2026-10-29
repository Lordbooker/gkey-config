// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"

#include <QColor>
#include <QHash>
#include <QSet>
#include <QWidget>

// Schematic drawing of a device, built from the model file's "view" section
// (no bitmap). Keys can be clicked; bound, selected and pressed keys are
// highlighted.
class DeviceView : public QWidget {
    Q_OBJECT
public:
    explicit DeviceView(QWidget *parent = nullptr);

    void setModel(const DeviceModel &model);
    void setBoundKeys(const QSet<QString> &keys);
    // Bindable keys that have no effect in the current settings (e.g. the stick
    // buttons in stick mode Joystick): drawn greyed out, not selectable; the
    // text is shown as tooltip.
    void setInactiveKeys(const QHash<QString, QString> &keysWithReason);
    void setSelectedKey(const QString &key);
    void setKeyPressed(const QString &key, bool pressed);
    void setAccentColor(const QColor &color);   // backlight colour of the profile

    const QString &selectedKey() const { return m_selected; }

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

signals:
    void keyClicked(const QString &key);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    QTransform viewToWidget() const;
    QString keyAt(const QPointF &widgetPos) const;
    bool isBindable(const QString &key) const;
    static QPainterPath pathFor(const DeviceModel::Shape &shape);

    DeviceModel m_model;
    QSet<QString> m_bindable;
    QSet<QString> m_bound;
    QHash<QString, QString> m_inactive;   // key -> reason
    QSet<QString> m_pressed;
    QString m_selected;
    QString m_hover;
    QColor m_accent;
};
