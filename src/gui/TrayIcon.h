// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <QIcon>
#include <QString>

// The icon of the tray: the application icon in colour, or a variant in one
// colour for panels that show only such icons (e.g. GNOME). Whether the panel
// is dark or light cannot be found out reliably, so the user chooses.
namespace TrayIcon {

enum class Style {
    Colour,   // the application icon
    Light,    // one colour, light: for dark panels
    Dark,     // one colour, dark: for light panels
};

// Names as stored in the settings ("colour", "light", "dark"); an unknown
// name is Colour.
Style styleFromName(const QString &name);
QString styleName(Style style);

// colourIcon is used for Style::Colour.
QIcon icon(Style style, const QIcon &colourIcon);

}
