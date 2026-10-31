/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The parity rule (tools/parity/compare.py) for two images in a test: exact outside overlays
// and text; in overlays one level of blend rounding is no difference and at most 10 % may
// differ beyond it; in the text mask at most 10 % may differ.

#include <QHash>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

#include <algorithm>

namespace ParityImages {
// Empty when the images match; ignored rectangles are left out (e.g. a focus-dependent cursor).
inline QString compare(const QImage& widgetImage, const QImage& sceneImage, const QList<QRect>& overlays, const QList<QRect>& text, const QList<QRect>& ignored = {})
{
    if (sceneImage.size() != widgetImage.size()) {
        return QStringLiteral("sizes differ");
    }
    if (qEnvironmentVariableIsSet("TERMINALVIEWTEST_IMAGES")) {
        widgetImage.save(qEnvironmentVariable("TERMINALVIEWTEST_IMAGES") + QStringLiteral("/widget.png"));
        sceneImage.save(qEnvironmentVariable("TERMINALVIEWTEST_IMAGES") + QStringLiteral("/scene.png"));
    }
    const auto inside = [](const QList<QRect>& rects, int x, int y) {
        return std::any_of(rects.begin(), rects.end(), [x, y](const QRect& rect) {
            return rect.contains(x, y);
        });
    };
    const auto area = [](const QList<QRect>& rects) {
        int pixels = 0;
        for (const QRect& rect : rects) {
            pixels += rect.width() * rect.height();
        }
        return pixels;
    };
    // compare.py's text_mask(): in each text rectangle, what isn't its background in either
    // image, grown by 1 px.
    QImage textMask(widgetImage.size(), QImage::Format_Grayscale8);
    textMask.fill(0);
    for (const QRect& rect : text) {
        QHash<QRgb, int> counts;
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                ++counts[widgetImage.pixel(x, y)];
            }
        }
        if (counts.isEmpty()) {
            continue;
        }
        const QRgb background = std::max_element(counts.constKeyValueBegin(), counts.constKeyValueEnd(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        })->first;
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                if (widgetImage.pixel(x, y) != background || sceneImage.pixel(x, y) != background) {
                    const QRect grown = QRect(x - 1, y - 1, 3, 3) & textMask.rect();
                    for (int gy = grown.top(); gy <= grown.bottom(); ++gy) {
                        for (int gx = grown.left(); gx <= grown.right(); ++gx) {
                            textMask.scanLine(gy)[gx] = 1;
                        }
                    }
                }
            }
        }
    }
    int textPixels = 0;
    int outside = 0;
    int inOverlays = 0;
    int inText = 0;
    for (int y = 0; y < widgetImage.height(); ++y) {
        for (int x = 0; x < widgetImage.width(); ++x) {
            const bool isText = textMask.scanLine(y)[x] != 0;
            textPixels += isText ? 1 : 0;
            const QRgb widgetPixel = widgetImage.pixel(x, y);
            const QRgb scenePixel = sceneImage.pixel(x, y);
            if (inside(ignored, x, y) || widgetPixel == scenePixel) {
                continue;
            }
            if (isText) {
                ++inText;
            } else if (!inside(overlays, x, y)) {
                ++outside;
            } else if (qAbs(qRed(widgetPixel) - qRed(scenePixel)) > 1 || qAbs(qGreen(widgetPixel) - qGreen(scenePixel)) > 1
                || qAbs(qBlue(widgetPixel) - qBlue(scenePixel)) > 1 || qAbs(qAlpha(widgetPixel) - qAlpha(scenePixel)) > 1) {
                ++inOverlays;
            }
        }
    }
    if (outside != 0) {
        return QStringLiteral("%1 pixels differ outside text and overlays").arg(outside);
    }
    if (inOverlays > area(overlays) / 10) {
        return QStringLiteral("%1 of %2 overlay pixels differ").arg(inOverlays).arg(area(overlays));
    }
    if (inText > textPixels / 10) {
        return QStringLiteral("%1 of %2 text pixels differ").arg(inText).arg(textPixels);
    }
    return {};
}
}
