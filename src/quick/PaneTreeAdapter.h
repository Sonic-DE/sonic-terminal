/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QWidget;

namespace Konsole {
class TerminalView;

/**
 * The terminals and splitter handles of a compat window's (hidden) central widget, where
 * its tab widget and splitters laid them out, relative to the central widget. As a list
 * model of the terminals (view, geometry, shown), stable while only geometry changes.
 */
class KONSOLEPRIVATE_EXPORT PaneTreeAdapter : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QRect centralGeometry READ centralGeometry NOTIFY layoutChanged)
    // {geometry, orientation (Qt::Orientation), palette}
    Q_PROPERTY(QVariantList handles READ handles NOTIFY layoutChanged)

public:
    enum Roles {
        ViewRole = Qt::UserRole + 1,
        GeometryRole,
        ShownRole,
        // TerminalHeaderBar's rule: a tab bar base line when what is 10 px above its header bar
        // is not a tab bar, a terminal or a splitter.
        HeaderTabBarBaseRole,
    };

    explicit PaneTreeAdapter(QObject* parent = nullptr);

    QObject* window() const;
    void setWindow(QObject* window);
    // The central widget in the window.
    QRect centralGeometry() const;
    QVariantList handles() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void windowChanged();
    void layoutChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Pane {
        QPointer<TerminalView> view;
        QRect geometry;
        bool shown = false;
        bool headerTabBarBase = false;
    };

    QWidget* central() const;
    void watch(QWidget* widget);
    void refresh();

    QPointer<QWidget> _window;
    QList<Pane> _panes;
    QVariantList _handles;
    QRect _centralGeometry;
    QTimer _refresh;
};
}
