/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>

namespace Konsole {
class PaneModel;
class TerminalDisplay;

/**
 * One node of a tab's split tree: a split (the old ViewSplitter) or a leaf holding a
 * TerminalDisplay. The structure rules are ViewSplitter's and QSplitter's, including the
 * order QObject children had, which D-Bus results and focus cycling depend on.
 */
class KONSOLEPRIVATE_EXPORT PaneNode : public QObject {
    Q_OBJECT

public:
    enum class AddBehavior {
        AddBefore,
        AddAfter,
    };

    bool isLeaf() const;
    int id() const;
    TerminalDisplay* display() const;
    PaneModel* model() const;
    PaneNode* parentNode() const;

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    int count() const;
    PaneNode* item(int index) const;
    int indexOf(const PaneNode* node) const;

    bool isHidden() const;
    void setHidden(bool hidden);

    /** The displays below this node in the order QObject::findChildren() returned them. */
    QList<TerminalDisplay*> displays() const;
    /** The splits below this node, in the same order. */
    QList<PaneNode*> splits() const;

    PaneNode* toplevel();
    PaneNode* childSplit(int id);
    PaneNode* activeSplit();
    TerminalDisplay* activeDisplay() const;

    void addDisplay(TerminalDisplay* display, Qt::Orientation orientation, AddBehavior behavior = AddBehavior::AddAfter);
    void addDisplay(TerminalDisplay* display, int index = -1);
    void addSplit(PaneNode* split, int index = -1);
    /** QSplitter::addWidget(): appends, without ViewSplitter's size rules. */
    void addItem(PaneNode* node);
    /** While set, a child leaving doesn't unwrap this split (ViewSplitter's m_blockPropagatedDeletion). */
    void setBlockPropagatedDeletion(bool block);
    /** setParent(nullptr): takes this node out of its parent split. */
    void detachFromParent();

    QList<int> sizes() const;
    void setSizes(const QList<int>& sizes);
    /** The node's extent along its orientation, as its view laid it out. */
    int extent() const;
    QRect geometry() const;

    void adjustActiveDisplaySize(int percentage);
    void toggleMaximizeCurrentDisplay();
    void toggleZoomMaximizeCurrentDisplay();
    void clearMaximized();
    bool terminalMaximized() const;

    void focusUp();
    void focusDown();
    void focusLeft();
    void focusRight();
    void focusNext(int direction = 1);
    void focusPrevious();

    /** ViewSplitter::getChildWidgetsLayout(): "(id)[a|b]" or "(id){a|b}". */
    QString layoutString() const;

    ~PaneNode() override;

private:
    friend class PaneModel;
    PaneNode(PaneModel* model, int id, TerminalDisplay* display);

    void insertItem(int index, PaneNode* node);
    void replaceItem(int index, PaneNode* node);
    void takeFromParent();
    void detach();
    void itemRemoved();
    void scheduleDeletion();
    void updateSizes();
    void focusDirection(Qt::Orientation orientation, int direction);
    bool hideRecurse(TerminalDisplay* current);
    void minimizeMaximize(bool maximize, bool zoom);

    PaneModel* _model;
    int _id;
    QPointer<TerminalDisplay> _display;
    PaneNode* _parent = nullptr;
    QList<PaneNode*> _items;
    Qt::Orientation _orientation = Qt::Horizontal;
    bool _hidden = false;
    bool _blockPropagatedDeletion = false;
    bool _terminalMaximized = false;
    bool _deletionScheduled = false;
    QPointer<PaneNode> _focusOnDeletion;
    qreal _fontSizeBeforeMaximization = 0;
};

/**
 * What depends on how the nodes are shown: sizes, geometry, focus, and the order of
 * QObject children. The widget view answers from its real QSplitters, so the widget
 * window behaves exactly as before; the QML pane tree answers from PaneModel's own
 * emulation of the same rules (phase 2c).
 */
class KONSOLEPRIVATE_EXPORT PaneLayout {
public:
    virtual ~PaneLayout();
    virtual QList<int> sizes(const PaneNode* node) const = 0;
    virtual void setSizes(PaneNode* node, const QList<int>& sizes) = 0;
    virtual void refresh(PaneNode* node) = 0;
    /** The node's width (horizontal) or height (vertical). */
    virtual int extent(const PaneNode* node) const = 0;
    /** The node's rectangle in its tab's coordinates. */
    virtual QRect geometry(const PaneNode* node) const = 0;
    virtual int handleWidth(const PaneNode* node) const = 0;
    /** QWidget::focusWidget() of the node, as a display, or null. */
    virtual TerminalDisplay* focusedDisplay(const PaneNode* node) const = 0;
    /** The nearest split at or above the node's focusWidget() (or the node), as ViewSplitter::activeSplitter(). */
    virtual PaneNode* focusedSplit(const PaneNode* node) const = 0;
    /** QObject::findChildren<TerminalDisplay *>() of the node. */
    virtual QList<TerminalDisplay*> displays(const PaneNode* node) const = 0;
    /** QObject::findChild<TerminalDisplay *>(): direct children first, unlike findChildren(). */
    virtual TerminalDisplay* firstDisplay(const PaneNode* node) const = 0;
    /** QObject::findChildren<ViewSplitter *>() of the node, as nodes. */
    virtual QList<PaneNode*> splits(const PaneNode* node) const = 0;
    /** findChildren<TerminalDisplay *>() of the whole tab container (its own order). */
    virtual QList<TerminalDisplay*> allDisplays() const = 0;

    // A view with real tabs (QTabWidget) owns their order and the current tab: the
    // model asks it, so it is never out of step inside QTabWidget's own signals.
    /** The view reports displays leaving their split itself (PaneModel::displayRemoved). */
    virtual bool reportsRemovals() const;
    virtual bool ownsTabs() const;
    virtual int tabCount() const;
    virtual PaneNode* tabAt(int index) const;
    virtual int currentTabIndex() const;
    virtual void setCurrentTabIndex(int index);
    /** Inserts the page; returns the index QTabWidget used. */
    virtual int insertTab(int index, PaneNode* top);
    /** The node or display under a point in the tab's coordinates (QWidget::childAt). */
    virtual TerminalDisplay* displayAt(const PaneNode* top, const QPoint& point) const = 0;
};

/**
 * The tabs and split trees of one ViewManager (phase 2 design item 1). TabbedViewContainer
 * and ViewSplitter are views of it.
 */
class KONSOLEPRIVATE_EXPORT PaneModel : public QObject {
    Q_OBJECT

public:
    explicit PaneModel(QObject* parent = nullptr);
    ~PaneModel() override;

    void setLayout(PaneLayout* layout);
    PaneLayout* layout() const;

    /** A new split, taking the next process-wide id (ViewSplitter::lastSplitterId). */
    PaneNode* createSplit();
    /** The next process-wide split id, also for ViewSplitters made outside a model. */
    static int takeSplitId();
    /** Moves a detached tree from another model (a tab dragged or detached to a window). */
    void adopt(PaneNode* top);
    /** Removes a tab without deleting it (QWidget::setParent(nullptr) on the page). */
    void takeTab(PaneNode* top);
    PaneNode* leaf(TerminalDisplay* display);
    PaneNode* leafIfAny(const TerminalDisplay* display) const;

    int count() const;
    PaneNode* tab(int index) const;
    int indexOf(const PaneNode* top) const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    PaneNode* currentTab() const;

    /** Inserts a tab; an invalid index appends. Returns the index used. */
    int insertTab(int index, PaneNode* top);
    void moveTab(int from, int to);

    PaneNode* findSplit(int id) const;
    QList<TerminalDisplay*> displays() const;

    /**
     * A display left its node without the model moving it (destroyed, or reparented
     * by other code): the same follow-up as ViewSplitter::childEvent.
     */
    void displayRemoved(TerminalDisplay* display);

Q_SIGNALS:
    void splitCreated(Konsole::PaneNode* split);
    /** The item moved under `parent` (QWidget::setParent), before it takes its place. */
    void itemReparented(Konsole::PaneNode* parent, Konsole::PaneNode* item);
    /** The item is at `index` of `parent` (QSplitter::insertWidget). */
    void itemInserted(Konsole::PaneNode* parent, int index, Konsole::PaneNode* item);
    void itemRemoved(Konsole::PaneNode* parent, Konsole::PaneNode* item);
    void itemReplaced(Konsole::PaneNode* parent, int index, Konsole::PaneNode* old, Konsole::PaneNode* item);
    void orientationChanged(Konsole::PaneNode* split);
    void hiddenChanged(Konsole::PaneNode* node);
    void splitDeleted(Konsole::PaneNode* split);
    void tabInserted(int index, Konsole::PaneNode* top);
    void tabRemoved(int index, Konsole::PaneNode* top);
    /** A tab's tree was taken out of the tab list but lives on (takeTab). */
    void tabTaken(int index, Konsole::PaneNode* top);
    void tabMoved(int from, int to);
    void currentChanged(int index);
    /** After any change below a tab; header bars re-apply their visibility rule then. */
    void treeChanged(Konsole::PaneNode* top);
    /** QWidget::setFocus() on the node's widget (a split focuses through its focus proxy). */
    void focusRequested(Konsole::PaneNode* node);

private:
    friend class PaneNode;
    void nodeDeleted(PaneNode* node);
    bool tabsInView() const;
    void watchDisplay(TerminalDisplay* display);

    PaneLayout* _layout = nullptr;
    QList<PaneNode*> _tabs;
    QHash<const TerminalDisplay*, PaneNode*> _leaves;
    int _currentIndex = -1;
    static int lastSplitId;
};

namespace PaneModelRegistry {
    KONSOLEPRIVATE_EXPORT void add(const QObject* owner, PaneModel* model);
    KONSOLEPRIVATE_EXPORT void remove(const QObject* owner);
    KONSOLEPRIVATE_EXPORT PaneModel* model(const QObject* owner);
}
}
