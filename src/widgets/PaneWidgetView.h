/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"
#include "model/PaneModel.h"

#include <QHash>
#include <QObject>
#include <QPointer>

namespace Konsole {
class TabbedViewContainer;
class ViewSplitter;

/**
 * The widget view of a PaneModel: applies its operations to a TabbedViewContainer's real
 * QTabWidget pages and ViewSplitters, and answers the model's layout questions from
 * them, so the widget window and the KPart behave exactly as before the model existed.
 * Tab order and the current tab come back from the real tab bar.
 */
class KONSOLEPRIVATE_EXPORT PaneWidgetView : public QObject, public PaneLayout {
    Q_OBJECT

public:
    PaneWidgetView(PaneModel* model, TabbedViewContainer* container);
    ~PaneWidgetView() override;

    static PaneWidgetView* forContainer(const TabbedViewContainer* container);
    /** The node a ViewSplitter shows, in whichever view. */
    static PaneNode* nodeOf(const ViewSplitter* splitter);
    static ViewSplitter* splitterOf(const PaneNode* node);

    PaneModel* model() const;
    QWidget* widget(const PaneNode* node) const;

    QList<int> sizes(const PaneNode* node) const override;
    void setSizes(PaneNode* node, const QList<int>& sizes) override;
    void refresh(PaneNode* node) override;
    int extent(const PaneNode* node) const override;
    QRect geometry(const PaneNode* node) const override;
    int handleWidth(const PaneNode* node) const override;
    TerminalDisplay* focusedDisplay(const PaneNode* node) const override;
    PaneNode* focusedSplit(const PaneNode* node) const override;
    QList<TerminalDisplay*> displays(const PaneNode* node) const override;
    TerminalDisplay* firstDisplay(const PaneNode* node) const override;
    QList<PaneNode*> splits(const PaneNode* node) const override;
    TerminalDisplay* displayAt(const PaneNode* top, const QPoint& point) const override;
    QList<TerminalDisplay*> allDisplays() const override;
    bool reportsRemovals() const override;
    bool ownsTabs() const override;
    int tabCount() const override;
    PaneNode* tabAt(int index) const override;
    int currentTabIndex() const override;
    void setCurrentTabIndex(int index) override;
    int insertTab(int index, PaneNode* top) override;

private:
    void splitCreated(PaneNode* split);
    void splitDeleted(PaneNode* split);
    void treeChanged(PaneNode* top);

    // The model belongs to the ViewManager, which may go before the container.
    QPointer<PaneModel> _model;
    QPointer<TabbedViewContainer> _container;
};
}
