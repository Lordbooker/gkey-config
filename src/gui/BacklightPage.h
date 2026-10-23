// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "DeviceModel.h"
#include "Profile.h"

#include <QColor>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

// Backlight of the active profile: a colour (RGB models such as G13, G510) or
// brightness levels for the keys and the LCD (G11, G15 family); plus the hint
// and button for LED access (group "gkey").
//
// The page does not change profiles itself; it reports what the user chose.
class BacklightPage : public QWidget {
    Q_OBJECT
public:
    explicit BacklightPage(QWidget *parent = nullptr);

    void setModel(const DeviceModel &model);
    // nullptr: no profile (everything disabled).
    void setProfile(const Profile *profile);
    // Empty text hides the hint; the button is shown when access can be requested.
    void setLedHint(const QString &text, bool canRequestAccess);

    // Colour the page starts with when the user enables it.
    static constexpr Profile::Rgb DefaultColour{51, 153, 255};

signals:
    // nullopt: the profile no longer sets a colour.
    void colourChanged(const std::optional<Profile::Rgb> &rgb);
    // While the colour dialog is open (live preview on the device), and when it closes.
    void colourPreview(const QColor &color);
    void colourPreviewEnded();
    // nullopt: the profile no longer sets that brightness.
    void levelsChanged(const std::optional<int> &keys, const std::optional<int> &lcd);
    void ledAccessRequested();

private:
    void chooseColour();
    void onLevelsToggled(bool enabled);
    void onLevelChosen();
    void fillLevels(QComboBox *box, const DeviceModel::Led *led, const std::optional<int> &value);

    DeviceModel m_model;
    std::optional<Profile::Rgb> m_colour;

    QWidget *m_colourWidget;
    QCheckBox *m_colourEnabled;
    QPushButton *m_colourButton;
    QWidget *m_levelWidget;
    QCheckBox *m_levelEnabled;
    QComboBox *m_levelBox;
    QComboBox *m_lcdLevelBox;
    QLabel *m_ledHint;
    QPushButton *m_ledButton;
};
