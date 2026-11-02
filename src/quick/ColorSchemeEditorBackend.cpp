/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorSchemeEditorBackend.h"

#include "characters/CharacterColor.h"
#include "colorscheme/ColorScheme.h"

#include <KLocalizedString>

#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaEnum>
#include <QQuickItem>
#include <QQuickWindow>

namespace Konsole {
namespace {
    // The table shows a third of the scheme's colours per column: normal, intense, faint.
    const int COLOR_TABLE_ROW_LENGTH = TABLE_COLORS / 3;

    QString styleName(int style)
    {
        return QString::fromLatin1(QMetaEnum::fromType<ColorSchemeWallpaper::FillStyle>().valueToKey(style));
    }

    QString flipName(int flip)
    {
        return QString::fromLatin1(QMetaEnum::fromType<ColorSchemeWallpaper::FlipType>().valueToKey(flip));
    }
}

ColorSchemeEditorBackend::ColorSchemeEditorBackend(const std::shared_ptr<const ColorScheme>& scheme,
    bool isNewScheme,
    bool supportsTransparency,
    QObject* parent)
    : QObject(parent)
    , _colors(std::make_shared<ColorScheme>(*scheme))
    , _isNewScheme(isNewScheme)
    , _supportsTransparency(supportsTransparency)
    , _shownWallpaperPath(scheme->wallpaper()->path())
    , _wallpaperEnabled(!scheme->wallpaper()->isNull())
{
    if (isNewScheme) {
        _colors->setDescription(QStringLiteral("New Color Scheme"));
    }
}

ColorSchemeEditorBackend::~ColorSchemeEditorBackend() = default;

QString ColorSchemeEditorBackend::title() const
{
    return _isNewScheme ? i18nc("@title:window", "New Color Scheme") : i18nc("@title:window", "Edit Color Scheme");
}

QString ColorSchemeEditorBackend::description() const
{
    return _colors->description();
}

void ColorSchemeEditorBackend::setDescription(const QString& description)
{
    _colors->setDescription(description);
    Q_EMIT changed();
}

QStringList ColorSchemeEditorBackend::colorNames() const
{
    QStringList names;
    for (int row = 0; row < COLOR_TABLE_ROW_LENGTH; ++row) {
        names.append(ColorScheme::translatedColorNameForIndex(row));
    }
    return names;
}

int ColorSchemeEditorBackend::transparency() const
{
    return qRound((1 - _colors->opacity()) * 100);
}

void ColorSchemeEditorBackend::setTransparency(int percent)
{
    _colors->setOpacity((100.0 - percent) / 100.0);
    Q_EMIT changed();
}

bool ColorSchemeEditorBackend::blur() const
{
    return _colors->blur();
}

void ColorSchemeEditorBackend::setBlur(bool blur)
{
    _colors->setBlur(blur);
    Q_EMIT changed();
}

bool ColorSchemeEditorBackend::randomizedBackground() const
{
    return _colors->isColorRandomizationEnabled();
}

void ColorSchemeEditorBackend::setRandomizedBackground(bool randomized)
{
    _colors->setColorRandomization(randomized);
    Q_EMIT changed();
}

bool ColorSchemeEditorBackend::supportsTransparency() const
{
    return _supportsTransparency;
}

QString ColorSchemeEditorBackend::wallpaperPath() const
{
    return _shownWallpaperPath;
}

void ColorSchemeEditorBackend::setWallpaperPath(const QString& path)
{
    _shownWallpaperPath = path;
    const auto wallpaper = _colors->wallpaper();
    if (path.isEmpty()) {
        setWallpaper(path, styleName(wallpaper->style()), wallpaper->anchor(), wallpaper->opacity(), flipName(wallpaper->flipType()));
        _wallpaperEnabled = false;
    } else {
        QFileInfo i(path);
        if (i.exists() && i.isFile() && i.isReadable()) {
            setWallpaper(path, styleName(wallpaper->style()), wallpaper->anchor(), wallpaper->opacity(), flipName(wallpaper->flipType()));
            _wallpaperEnabled = true;
        }
    }
    Q_EMIT changed();
}

bool ColorSchemeEditorBackend::wallpaperEnabled() const
{
    return _wallpaperEnabled;
}

int ColorSchemeEditorBackend::wallpaperTransparency() const
{
    return qRound((1 - _colors->wallpaper()->opacity()) * 100);
}

void ColorSchemeEditorBackend::setWallpaperTransparency(int percent)
{
    const auto wallpaper = _colors->wallpaper();
    setWallpaper(wallpaper->path(), styleName(wallpaper->style()), wallpaper->anchor(), (100.0 - percent) / 100.0, flipName(wallpaper->flipType()));
}

int ColorSchemeEditorBackend::wallpaperScaling() const
{
    return _colors->wallpaper()->style();
}

void ColorSchemeEditorBackend::setWallpaperScaling(int style)
{
    const auto wallpaper = _colors->wallpaper();
    setWallpaper(wallpaper->path(), styleName(style), wallpaper->anchor(), wallpaper->opacity(), flipName(wallpaper->flipType()));
}

int ColorSchemeEditorBackend::wallpaperFlip() const
{
    return _colors->wallpaper()->flipType();
}

void ColorSchemeEditorBackend::setWallpaperFlip(int flip)
{
    const auto wallpaper = _colors->wallpaper();
    setWallpaper(wallpaper->path(), styleName(wallpaper->style()), wallpaper->anchor(), wallpaper->opacity(), flipName(flip));
}

int ColorSchemeEditorBackend::horizontalAnchor() const
{
    return qRound(_colors->wallpaper()->anchor().x() * 2.0);
}

void ColorSchemeEditorBackend::setHorizontalAnchor(int position)
{
    const auto wallpaper = _colors->wallpaper();
    setWallpaper(wallpaper->path(),
        styleName(wallpaper->style()),
        QPointF(position / 2.0, wallpaper->anchor().y()),
        wallpaper->opacity(),
        flipName(wallpaper->flipType()));
}

int ColorSchemeEditorBackend::verticalAnchor() const
{
    return qRound(_colors->wallpaper()->anchor().y() * 2.0);
}

void ColorSchemeEditorBackend::setVerticalAnchor(int position)
{
    const auto wallpaper = _colors->wallpaper();
    setWallpaper(wallpaper->path(),
        styleName(wallpaper->style()),
        QPointF(wallpaper->anchor().x(), position / 2.0),
        wallpaper->opacity(),
        flipName(wallpaper->flipType()));
}

void ColorSchemeEditorBackend::setWallpaper(const QString& path, const QString& style, const QPointF& anchor, qreal opacity, const QString& flipType)
{
    _colors->setWallpaper(path, style, anchor, opacity, flipType);
    Q_EMIT changed();
}

QColor ColorSchemeEditorBackend::color(int row, int column) const
{
    QColor table[TABLE_COLORS];
    _colors->getColorTable(table);
    return table[row + column * COLOR_TABLE_ROW_LENGTH];
}

void ColorSchemeEditorBackend::setColor(int row, int column, const QColor& color)
{
    if (color.isValid()) {
        _colors->setColorTableEntry(row + column * COLOR_TABLE_ROW_LENGTH, color);
        Q_EMIT changed();
    }
}

void ColorSchemeEditorBackend::chooseWallpaper(QQuickItem* from)
{
    // Get supported image formats and convert to QString for getOpenFileName()
    const QList<QByteArray> mimeTypes = QImageReader::supportedImageFormats();
    QString fileFormats = QStringLiteral("(");
    for (const QByteArray& mime : mimeTypes) {
        fileFormats += QStringLiteral("*.%1 ").arg(QLatin1String(mime));
    }
    fileFormats += QLatin1String(")");

    // QFileDialog::getOpenFileName(), transient for the QML dialog.
    QFileDialog dialog(nullptr,
        i18nc("@title:window", "Select wallpaper image file"),
        _shownWallpaperPath,
        i18nc("@label:textbox Filter in file open dialog", "Supported Images") + fileFormats);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (from && from->window()) {
        dialog.winId();
        dialog.windowHandle()->setTransientParent(from->window());
    }
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
        setWallpaperPath(dialog.selectedFiles().constFirst());
    }
}

void ColorSchemeEditorBackend::save()
{
    Q_EMIT saveRequested(*_colors, _isNewScheme);
}

const ColorScheme& ColorSchemeEditorBackend::colorScheme() const
{
    return *_colors;
}

bool ColorSchemeEditorBackend::isNewScheme() const
{
    return _isNewScheme;
}
}

#include "moc_ColorSchemeEditorBackend.cpp"
