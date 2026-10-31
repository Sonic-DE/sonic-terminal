/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The parity runs' theme settings (tools/parity/run.py) for a test, written where the KDE
// platform theme and Kirigami read them (QStandardPaths' test mode).

#include <KConfig>
#include <KConfigGroup>

#include <QFile>
#include <QStandardPaths>

#include <memory>

namespace TestTheme {
// What the colour and style settings pages write: the scheme's groups plus its name.
inline void writeGlobals(const QString& colorScheme, const QString& widgetStyle)
{
    const QString globals = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kdeglobals");
    QFile::remove(globals);
    KConfig scheme(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(colorScheme)),
        KConfig::SimpleConfig);
    std::unique_ptr<KConfig> copy(scheme.copyTo(globals));
    copy->group(QStringLiteral("General")).writeEntry("ColorScheme", colorScheme);
    copy->group(QStringLiteral("KDE")).writeEntry("widgetStyle", widgetStyle);
    copy->group(QStringLiteral("KDE")).writeEntry("AnimationDurationFactor", 0);
    copy->group(QStringLiteral("Icons")).writeEntry("Theme", QStringLiteral("silver"));
    copy->sync();
}

// Before QApplication (a test's initMain()).
inline void setUp()
{
    QStandardPaths::setTestModeEnabled(true);
    writeGlobals(QStringLiteral("SilverLight"), QStringLiteral("Silver"));
}
}
