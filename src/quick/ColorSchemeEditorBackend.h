/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QQuickItem;

namespace Konsole {
class ColorScheme;

/**
 * The Colour Scheme Editor's edits (ColorSchemeEditor without widgets) of a copy of a
 * scheme: its description, colour table, transparency, blur, randomisation and wallpaper.
 * OK and Apply ask for the copy to be saved (saveRequested()).
 */
class KONSOLEPRIVATE_EXPORT ColorSchemeEditorBackend : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by ProfileEditor::editColorScheme()")
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY changed)
    Q_PROPERTY(QStringList colorNames READ colorNames CONSTANT)
    Q_PROPERTY(int transparency READ transparency WRITE setTransparency NOTIFY changed)
    Q_PROPERTY(bool blur READ blur WRITE setBlur NOTIFY changed)
    Q_PROPERTY(bool randomizedBackground READ randomizedBackground WRITE setRandomizedBackground NOTIFY changed)
    Q_PROPERTY(bool supportsTransparency READ supportsTransparency CONSTANT)
    Q_PROPERTY(QString wallpaperPath READ wallpaperPath WRITE setWallpaperPath NOTIFY changed)
    Q_PROPERTY(bool wallpaperEnabled READ wallpaperEnabled NOTIFY changed)
    Q_PROPERTY(int wallpaperTransparency READ wallpaperTransparency WRITE setWallpaperTransparency NOTIFY changed)
    Q_PROPERTY(int wallpaperScaling READ wallpaperScaling WRITE setWallpaperScaling NOTIFY changed)
    Q_PROPERTY(int wallpaperFlip READ wallpaperFlip WRITE setWallpaperFlip NOTIFY changed)
    Q_PROPERTY(int horizontalAnchor READ horizontalAnchor WRITE setHorizontalAnchor NOTIFY changed)
    Q_PROPERTY(int verticalAnchor READ verticalAnchor WRITE setVerticalAnchor NOTIFY changed)

public:
    ColorSchemeEditorBackend(const std::shared_ptr<const ColorScheme>& scheme, bool isNewScheme, bool supportsTransparency, QObject* parent = nullptr);
    ~ColorSchemeEditorBackend() override;

    QString title() const;
    QString description() const;
    void setDescription(const QString& description);
    // The table's rows: the colour names.
    QStringList colorNames() const;
    // Percent, as the sliders show it (100 - opacity).
    int transparency() const;
    void setTransparency(int percent);
    bool blur() const;
    void setBlur(bool blur);
    bool randomizedBackground() const;
    void setRandomizedBackground(bool randomized);
    bool supportsTransparency() const;
    QString wallpaperPath() const;
    // A path that isn't an existing readable file is shown but not used, as the widget did.
    void setWallpaperPath(const QString& path);
    bool wallpaperEnabled() const;
    int wallpaperTransparency() const;
    void setWallpaperTransparency(int percent);
    // ColorSchemeWallpaper::FillStyle and FlipType.
    int wallpaperScaling() const;
    void setWallpaperScaling(int style);
    int wallpaperFlip() const;
    void setWallpaperFlip(int flip);
    // 0, 1, 2: left/top, centre, right/bottom.
    int horizontalAnchor() const;
    void setHorizontalAnchor(int position);
    int verticalAnchor() const;
    void setVerticalAnchor(int position);

    // Column 0 is the colour, 1 the intense one, 2 the faint one.
    Q_INVOKABLE QColor color(int row, int column) const;
    Q_INVOKABLE void setColor(int row, int column, const QColor& color);
    // The background image file dialog, transient for from's window.
    Q_INVOKABLE void chooseWallpaper(QQuickItem* from);
    // OK and Apply.
    Q_INVOKABLE void save();

    const ColorScheme& colorScheme() const;
    bool isNewScheme() const;

Q_SIGNALS:
    void changed();
    void saveRequested(const Konsole::ColorScheme& scheme, bool isNewScheme);

private:
    void setWallpaper(const QString& path, const QString& style, const QPointF& anchor, qreal opacity, const QString& flipType);

    std::shared_ptr<ColorScheme> _colors;
    bool _isNewScheme;
    bool _supportsTransparency;
    QString _shownWallpaperPath;
    bool _wallpaperEnabled;
};
}
