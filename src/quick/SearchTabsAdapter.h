/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QWidget;

namespace Konsole {
class SearchTabsModel;
class ViewManager;

/**
 * The tab search (searchtabs/SearchTabs) for a compat window's QML window: opens when the
 * window's tab widget asks for it, filters the tab titles with the widget's fuzzy rule,
 * keeps its place (SearchTabs::updateViewGeometry(), in the window) and switches tabs
 * through the ViewManager.
 */
class KONSOLEPRIVATE_EXPORT SearchTabsAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY entriesChanged)
    // {name, view}, in tab order, those matching the text.
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText CONSTANT)
    Q_PROPERTY(QString toolTip READ toolTip CONSTANT)

public:
    explicit SearchTabsAdapter(QObject* parent = nullptr);
    ~SearchTabsAdapter() override;

    QObject* window() const;
    void setWindow(QObject* window);
    bool isOpen() const;
    QString text() const;
    void setText(const QString& text);
    QVariantList entries() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    QRect geometry() const;
    QString placeholderText() const;
    QString toolTip() const;

    // Switches to the current entry's tab and closes.
    Q_INVOKABLE void activate();
    Q_INVOKABLE void close();

Q_SIGNALS:
    void windowChanged();
    void openChanged();
    void entriesChanged();
    void currentIndexChanged();
    void geometryChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ViewManager* viewManager() const;
    void openSearch();
    void filter();
    void updateGeometry();

    QPointer<QWidget> _window;
    SearchTabsModel* _model;
    QString _text;
    QVariantList _entries;
    int _currentIndex = -1;
    QRect _geometry;
    bool _open = false;
};
}
