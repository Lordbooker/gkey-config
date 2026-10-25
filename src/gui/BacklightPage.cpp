// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "BacklightPage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

QIcon colorIcon(const QColor &color)
{
    QPixmap pixmap(32, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::gray, 1));
    painter.setBrush(color.isValid() ? color : QColor(Qt::transparent));
    painter.drawRoundedRect(QRectF(0.5, 0.5, 31, 19), 4, 4);
    return {pixmap};
}

QColor toColor(const std::optional<Profile::Rgb> &rgb)
{
    return rgb ? QColor(rgb->red, rgb->green, rgb->blue) : QColor();
}

}

BacklightPage::BacklightPage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    // RGB backlight (G13, G510)
    m_colourWidget = new QWidget(this);
    auto *colourForm = new QFormLayout(m_colourWidget);
    colourForm->setContentsMargins(0, 0, 0, 0);
    m_colourEnabled = new QCheckBox(tr("This profile sets the backlight colour"), m_colourWidget);
    m_colourButton = new QPushButton(tr("Choose colour..."), m_colourWidget);
    colourForm->addRow(m_colourEnabled);
    colourForm->addRow(tr("Colour:"), m_colourButton);
    layout->addWidget(m_colourWidget);
    connect(m_colourEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        emit colourChanged(enabled ? std::optional<Profile::Rgb>(DefaultColour) : std::nullopt);
    });
    connect(m_colourButton, &QPushButton::clicked, this, &BacklightPage::chooseColour);

    // Brightness levels (G11, G15 family): keys and LCD
    m_levelWidget = new QWidget(this);
    auto *levelForm = new QFormLayout(m_levelWidget);
    levelForm->setContentsMargins(0, 0, 0, 0);
    m_levelEnabled = new QCheckBox(tr("This profile sets the backlight brightness"), m_levelWidget);
    m_levelBox = new QComboBox(m_levelWidget);
    m_lcdLevelBox = new QComboBox(m_levelWidget);
    levelForm->addRow(m_levelEnabled);
    levelForm->addRow(tr("Keys:"), m_levelBox);
    levelForm->addRow(tr("LCD:"), m_lcdLevelBox);
    layout->addWidget(m_levelWidget);
    connect(m_levelEnabled, &QCheckBox::toggled, this, &BacklightPage::onLevelsToggled);
    connect(m_levelBox, &QComboBox::activated, this, &BacklightPage::onLevelChosen);
    connect(m_lcdLevelBox, &QComboBox::activated, this, &BacklightPage::onLevelChosen);

    // LED access
    m_ledHint = new QLabel(this);
    m_ledHint->setWordWrap(true);
    layout->addWidget(m_ledHint);
    m_ledButton = new QPushButton(tr("Allow LED control..."), this);
    connect(m_ledButton, &QPushButton::clicked, this, &BacklightPage::ledAccessRequested);
    auto *ledButtonRow = new QHBoxLayout;
    ledButtonRow->addWidget(m_ledButton);
    ledButtonRow->addStretch();
    layout->addLayout(ledButtonRow);
    layout->addStretch();

    setProfile(nullptr);
}

void BacklightPage::setModel(const DeviceModel &model)
{
    m_model = model;
}

void BacklightPage::setProfile(const Profile *profile)
{
    const DeviceModel::Led *keys = m_model.ledByRole(QStringLiteral("backlight"));
    const DeviceModel::Led *lcd = m_model.ledByRole(QStringLiteral("lcd-backlight"));
    const bool levels = keys && keys->type == DeviceModel::LedType::Level;
    m_colourWidget->setVisible(!levels);
    m_levelWidget->setVisible(levels);

    // Colour
    m_colour = profile ? profile->backlight() : std::nullopt;
    {
        const QSignalBlocker blocker(m_colourEnabled);
        m_colourEnabled->setChecked(m_colour.has_value());
    }
    m_colourEnabled->setEnabled(profile && keys);
    m_colourButton->setEnabled(m_colour.has_value());
    m_colourButton->setIcon(colorIcon(toColor(m_colour)));

    // Levels: one selector per LED with the levels of the model
    const bool set = profile && (profile->backlightLevel() || profile->lcdBacklightLevel());
    {
        const QSignalBlocker blocker(m_levelEnabled);
        m_levelEnabled->setChecked(set);
    }
    m_levelEnabled->setEnabled(profile != nullptr);
    fillLevels(m_levelBox, levels ? keys : nullptr, profile ? profile->backlightLevel() : std::nullopt);
    fillLevels(m_lcdLevelBox, lcd, profile ? profile->lcdBacklightLevel() : std::nullopt);
    m_levelBox->setEnabled(set);
    m_lcdLevelBox->setEnabled(set && lcd);
    if (auto *form = qobject_cast<QFormLayout *>(m_levelWidget->layout()))
        form->setRowVisible(m_lcdLevelBox, lcd != nullptr);
}

void BacklightPage::setLedHint(const QString &text, bool canRequestAccess)
{
    m_ledHint->setText(text);
    m_ledHint->setVisible(!text.isEmpty());
    m_ledButton->setVisible(canRequestAccess);
}

void BacklightPage::fillLevels(QComboBox *box, const DeviceModel::Led *led, const std::optional<int> &value)
{
    const QSignalBlocker blocker(box);
    box->clear();
    if (!led)
        return;
    for (int level = 0; level <= led->max; ++level) {
        QString name = QString::number(level);
        if (level == 0)
            name = tr("Off");
        else if (led->max == 2)
            name = level == 1 ? tr("Dim") : tr("Bright");
        box->addItem(name, level);
    }
    box->setCurrentIndex(qMax(0, box->findData(value.value_or(led->max))));
}

void BacklightPage::chooseColour()
{
    if (!m_colour)
        return;
    QColorDialog dialog(toColor(m_colour), this);
    dialog.setWindowTitle(tr("Backlight colour"));
    connect(&dialog, &QColorDialog::currentColorChanged, this, &BacklightPage::colourPreview);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    emit colourPreviewEnded();
    if (!accepted)
        return;
    const QColor color = dialog.selectedColor();
    emit colourChanged(Profile::Rgb{color.red(), color.green(), color.blue()});
}

void BacklightPage::onLevelsToggled(bool enabled)
{
    const DeviceModel::Led *keys = m_model.ledByRole(QStringLiteral("backlight"));
    const DeviceModel::Led *lcd = m_model.ledByRole(QStringLiteral("lcd-backlight"));
    emit levelsChanged(enabled && keys ? std::optional<int>(keys->max) : std::nullopt,
                       enabled && lcd ? std::optional<int>(lcd->max) : std::nullopt);
}

void BacklightPage::onLevelChosen()
{
    const QVariant keys = m_levelBox->currentData();
    const QVariant lcd = m_lcdLevelBox->currentData();
    emit levelsChanged(keys.isValid() ? std::optional<int>(keys.toInt()) : std::nullopt,
                       lcd.isValid() ? std::optional<int>(lcd.toInt()) : std::nullopt);
}
