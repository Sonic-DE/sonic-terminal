/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PaneModel.h"

#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"
#include "terminalDisplay/TerminalScrollBar.h"
#include "widgets/TerminalHeaderBar.h"

#include <Sonic/Logging/Log>

namespace Konsole {

int PaneModel::lastSplitId = -1;

PaneLayout::~PaneLayout() = default;

bool PaneLayout::reportsRemovals() const
{
    return false;
}

bool PaneLayout::ownsTabs() const
{
    return false;
}

int PaneLayout::tabCount() const
{
    return 0;
}

PaneNode* PaneLayout::tabAt(int) const
{
    return nullptr;
}

int PaneLayout::currentTabIndex() const
{
    return -1;
}

void PaneLayout::setCurrentTabIndex(int)
{
}

int PaneLayout::insertTab(int, PaneNode*)
{
    return -1;
}

PaneNode::PaneNode(PaneModel* model, int id, TerminalDisplay* display)
    : QObject(model)
    , _model(model)
    , _id(id)
    , _display(display)
{
}

bool PaneNode::isLeaf() const
{
    return _id < 0;
}

int PaneNode::id() const
{
    return _id;
}

TerminalDisplay* PaneNode::display() const
{
    return _display;
}

PaneModel* PaneNode::model() const
{
    return _model;
}

PaneNode* PaneNode::parentNode() const
{
    return _parent;
}

Qt::Orientation PaneNode::orientation() const
{
    return _orientation;
}

void PaneNode::setOrientation(Qt::Orientation orientation)
{
    if (_orientation != orientation) {
        _orientation = orientation;
        Q_EMIT _model->orientationChanged(this);
    }
}

int PaneNode::count() const
{
    return _items.size();
}

PaneNode* PaneNode::item(int index) const
{
    return _items.value(index);
}

int PaneNode::indexOf(const PaneNode* node) const
{
    return _items.indexOf(const_cast<PaneNode*>(node));
}

bool PaneNode::isHidden() const
{
    return _hidden;
}

// Always passed on: ViewSplitter called show() on a parentless new split, which matters
// for its layout even though nothing was hidden.
void PaneNode::setHidden(bool hidden)
{
    _hidden = hidden;
    Q_EMIT _model->hiddenChanged(this);
}

QList<TerminalDisplay*> PaneNode::displays() const
{
    return _model->_layout ? _model->_layout->displays(this) : QList<TerminalDisplay*>();
}

QList<PaneNode*> PaneNode::splits() const
{
    return _model->_layout ? _model->_layout->splits(this) : QList<PaneNode*>();
}

PaneNode* PaneNode::toplevel()
{
    PaneNode* current = this;
    while (current->_parent) {
        current = current->_parent;
    }
    return current;
}

PaneNode* PaneNode::childSplit(int id)
{
    const auto all = splits();
    for (PaneNode* split : all) {
        if (split->id() == id) {
            return split;
        }
    }
    return nullptr;
}

// ViewSplitter::activeSplitter(): the split holding the focused widget, else this one.
PaneNode* PaneNode::activeSplit()
{
    PaneNode* node = _model->_layout ? _model->_layout->focusedSplit(this) : nullptr;
    return node ? node : this;
}

// ViewSplitter::activeTerminalDisplay(): the focused display, else the first one.
TerminalDisplay* PaneNode::activeDisplay() const
{
    if (!_model->_layout) {
        return nullptr;
    }
    if (TerminalDisplay* focused = _model->_layout->focusedDisplay(this)) {
        return focused;
    }
    return _model->_layout->firstDisplay(this);
}

QList<int> PaneNode::sizes() const
{
    return _model->_layout ? _model->_layout->sizes(this) : QList<int>();
}

void PaneNode::setSizes(const QList<int>& sizes)
{
    if (_model->_layout) {
        _model->_layout->setSizes(this, sizes);
    }
}

int PaneNode::extent() const
{
    return _model->_layout ? _model->_layout->extent(this) : 0;
}

QRect PaneNode::geometry() const
{
    return _model->_layout ? _model->_layout->geometry(this) : QRect();
}

void PaneNode::updateSizes()
{
    if (count() == 0) {
        return;
    }
    const int space = extent() / count();
    setSizes(QList<int>(count(), space));
}

// QSplitter::insertWidget(): a new child is reparented, moved within the list otherwise.
void PaneNode::insertItem(int index, PaneNode* node)
{
    if (node->_parent != this) {
        PaneNode* old = node->_parent;
        node->takeFromParent();
        node->_parent = this;
        Q_EMIT _model->itemReparented(this, node);
        if (old) {
            old->itemRemoved();
        }
    }
    int at = _items.indexOf(node);
    int last = _items.size() - (at >= 0 ? 1 : 0);
    if (index < 0 || index > last) {
        index = last;
    }
    if (at >= 0) {
        _items.move(at, index);
    } else {
        _items.insert(index, node);
    }
    Q_EMIT _model->itemInserted(this, index, node);
    Q_EMIT _model->treeChanged(toplevel());
}

void PaneNode::takeFromParent()
{
    if (_parent) {
        _parent->_items.removeAll(this);
        _parent = nullptr;
    }
}

// setParent(nullptr) on a child of a split.
void PaneNode::detach()
{
    PaneNode* old = _parent;
    if (!old) {
        return;
    }
    takeFromParent();
    Q_EMIT _model->itemRemoved(old, this);
    old->itemRemoved();
}

// QSplitter::replaceWidget(): the replaced node is detached, the new one inherits its place.
void PaneNode::replaceItem(int index, PaneNode* node)
{
    PaneNode* current = _items.value(index);
    if (!current || current == node || node->_parent == this) {
        return;
    }
    PaneNode* old = node->_parent;
    const bool wasHidden = current->_hidden;
    if (old) {
        old->_items.removeAll(node);
    }
    _items[index] = node;
    current->_parent = nullptr;
    node->_parent = this;
    node->_hidden = wasHidden;
    Q_EMIT _model->itemReplaced(this, index, current, node);
    // Detaching the replaced node runs this split's child-removed rule once more.
    itemRemoved();
    if (old) {
        old->itemRemoved();
    }
}

// ViewSplitter::childEvent() after a child left: empty splits go, one-child splits unwrap.
void PaneNode::itemRemoved()
{
    if (count() == 0) {
        if (_parent) {
            detach();
        }
        scheduleDeletion();
    } else if (count() == 1 && !_blockPropagatedDeletion) {
        if (PaneNode* parent = _parent) {
            parent->_blockPropagatedDeletion = true;
            if (_model->_layout) {
                _model->_layout->refresh(parent);
            }
            const QList<int> sizes = parent->sizes();
            PaneNode* only = _items.at(0);
            const int index = parent->indexOf(this);
            parent->replaceItem(index, only);
            parent->_blockPropagatedDeletion = false;
            parent->setSizes(sizes);
            _focusOnDeletion = only;
        }
    }
    Q_EMIT _model->treeChanged(toplevel());
}

void PaneNode::scheduleDeletion()
{
    if (_deletionScheduled) {
        return;
    }
    _deletionScheduled = true;
    // deleteLater(), as for the widget: the tab only goes away in the event loop.
    deleteLater();
}

PaneNode::~PaneNode()
{
    if (_deletionScheduled) {
        sonicDebug() << "pane: split" << _id << "deleted";
        _model->nodeDeleted(this);
    }
}

void PaneNode::addDisplay(TerminalDisplay* display, Qt::Orientation orientation, AddBehavior behavior)
{
    PaneNode* splitter = activeSplit();
    TerminalDisplay* active = splitter->activeDisplay();
    const int currentIndex = active == nullptr ? splitter->count() : splitter->indexOf(_model->leafIfAny(active));
    PaneNode* leaf = _model->leaf(display);

    if (splitter->count() < 2) {
        splitter->insertItem(behavior == AddBehavior::AddBefore ? currentIndex : currentIndex + 1, leaf);
        splitter->setOrientation(orientation);
        splitter->updateSizes();
    } else if (orientation == splitter->orientation()) {
        splitter->insertItem(behavior == AddBehavior::AddBefore ? currentIndex : currentIndex + 1, leaf);
        splitter->updateSizes();
    } else {
        const QList<int> sizes = splitter->sizes();
        PaneNode* newSplit = _model->createSplit();
        PaneNode* oldLeaf = _model->leaf(active);
        const int oldIndex = splitter->indexOf(oldLeaf);
        splitter->_blockPropagatedDeletion = true;
        newSplit->insertItem(-1, behavior == AddBehavior::AddBefore ? leaf : oldLeaf);
        newSplit->insertItem(-1, behavior == AddBehavior::AddBefore ? oldLeaf : leaf);
        newSplit->setOrientation(orientation);
        newSplit->setHidden(false);
        splitter->insertItem(oldIndex, newSplit);
        splitter->_blockPropagatedDeletion = false;
        splitter->setSizes(sizes);
        newSplit->updateSizes();
    }
}

void PaneNode::addDisplay(TerminalDisplay* display, int index)
{
    PaneNode* top = toplevel();
    PaneNode* leaf = _model->leaf(display);
    if (index == -1) {
        index = count();
    }
    if (top->count() == 2 && top == _parent && top->indexOf(leaf) != -1) {
        QList<PaneNode*> children = _items;
        children.insert(index, leaf);
        for (PaneNode* child : std::as_const(children)) {
            top->insertItem(-1, child);
        }
    } else {
        insertItem(index, leaf);
    }
    updateSizes();
}

void PaneNode::addItem(PaneNode* node)
{
    insertItem(-1, node);
}

void PaneNode::setBlockPropagatedDeletion(bool block)
{
    _blockPropagatedDeletion = block;
}

void PaneNode::detachFromParent()
{
    detach();
}

void PaneNode::addSplit(PaneNode* split, int index)
{
    if (index == -1) {
        index = count();
    }
    _blockPropagatedDeletion = true;
    if (split->orientation() == orientation()) {
        const QList<PaneNode*> children = split->_items;
        for (int i = 0; i < children.size(); ++i) {
            insertItem(index + i, children.at(i));
        }
    } else {
        insertItem(index, split);
    }
    _blockPropagatedDeletion = false;
    updateSizes();
}

void PaneNode::adjustActiveDisplaySize(int percentage)
{
    TerminalDisplay* focused = activeDisplay();
    PaneNode* leaf = _model->leafIfAny(focused);
    PaneNode* parent = leaf ? leaf->_parent : nullptr;
    if (!parent) {
        return;
    }
    const int containerIndex = parent->indexOf(leaf);
    QList<int> containerSizes = parent->sizes();
    if (containerIndex < 0 || containerIndex >= containerSizes.size()) {
        return;
    }
    const int oldSize = containerSizes[containerIndex];
    const auto newSize = static_cast<int>(oldSize * (1.0 + percentage / 100.0));
    // count() of this (the top split), not of the parent: as ViewSplitter did.
    const int perContainerDelta = (count() == 1) ? 0 : ((newSize - oldSize) / (count() - 1)) * (-1);
    for (int& size : containerSizes) {
        size += perContainerDelta;
    }
    containerSizes[containerIndex] = newSize;
    parent->setSizes(containerSizes);
}

void PaneNode::toggleMaximizeCurrentDisplay()
{
    _terminalMaximized = !_terminalMaximized;
    minimizeMaximize(_terminalMaximized, false);
}

void PaneNode::toggleZoomMaximizeCurrentDisplay()
{
    _terminalMaximized = !_terminalMaximized;
    minimizeMaximize(_terminalMaximized, true);
}

void PaneNode::clearMaximized()
{
    PaneNode* top = toplevel();
    if (top->terminalMaximized()) {
        top->toggleMaximizeCurrentDisplay();
    }
}

bool PaneNode::terminalMaximized() const
{
    return _terminalMaximized;
}

bool PaneNode::hideRecurse(TerminalDisplay* current)
{
    bool allHidden = true;
    for (PaneNode* child : std::as_const(_items)) {
        if (!child->isLeaf()) {
            allHidden = child->hideRecurse(current) && allHidden;
            continue;
        }
        if (child->display() == current) {
            allHidden = false;
        } else {
            child->setHidden(true);
        }
    }
    if (allHidden) {
        setHidden(true);
    }
    return allHidden;
}

void PaneNode::minimizeMaximize(bool maximize, bool zoom)
{
    PaneNode* top = toplevel();
    TerminalDisplay* current = top->activeDisplay();
    if (!current) {
        return;
    }
    current->setExpandedMode(maximize);
    QFont font = current->terminalFont()->getVTFont();

    if (maximize) {
        if (zoom) {
            _fontSizeBeforeMaximization = font.pointSizeF();
            auto* headerBar = current->headerBar();
            const int headerBarHeight = headerBar->isVisible() ? headerBar->height() : 0;
            auto* scrollBar = current->scrollBar();
            const int scrollBarWidth = scrollBar->scrollBarPosition() != Enum::ScrollBarHidden ? scrollBar->width() : 0;
            const QRect topRect = top->geometry();
            // As inexact as ViewSplitter: integer division, then 97 %.
            const auto scaleFactor = std::min((topRect.width() - scrollBarWidth) / (current->width() - scrollBarWidth),
                (topRect.height() - headerBarHeight) / (current->height() - headerBarHeight));
            const auto newSize = int(_fontSizeBeforeMaximization * scaleFactor * 0.97);
            if (newSize > _fontSizeBeforeMaximization) {
                font.setPointSizeF(newSize);
                current->terminalFont()->setVTFont(font);
            }
        } else {
            _fontSizeBeforeMaximization = 0;
        }
        for (PaneNode* child : std::as_const(top->_items)) {
            if (!child->isLeaf()) {
                child->hideRecurse(current);
            } else if (child->display() != current) {
                child->setHidden(true);
            }
        }
    } else {
        if (!qFuzzyIsNull(_fontSizeBeforeMaximization)) {
            font.setPointSizeF(_fontSizeBeforeMaximization);
            current->terminalFont()->setVTFont(font);
        }
        const auto allSplits = top->splits();
        for (PaneNode* split : allSplits) {
            split->setHidden(false);
        }
        const auto allDisplays = top->displays();
        for (TerminalDisplay* display : allDisplays) {
            if (PaneNode* leaf = _model->leafIfAny(display)) {
                leaf->setHidden(false);
            }
        }
    }
}

void PaneNode::focusDirection(Qt::Orientation orientation, int direction)
{
    TerminalDisplay* display = activeDisplay();
    PaneNode* leaf = _model->leafIfAny(display);
    if (!leaf || !leaf->_parent || !_model->_layout) {
        return;
    }
    PaneNode* top = leaf->_parent->toplevel();
    // The theme's splitter width plus some room, as ViewSplitter (KDE bug 411387).
    const int handleWidth = _model->_layout->handleWidth(leaf->_parent) + 3;
    const QRect rect = _model->_layout->geometry(leaf);
    const QPoint start = rect.topLeft();
    const int newX = orientation != Qt::Horizontal ? start.x() + handleWidth
        : direction == 1                           ? start.x() + rect.width() + handleWidth
                                                   : start.x() - handleWidth;
    const int newY = orientation != Qt::Vertical ? start.y() + handleWidth
        : direction == 1                         ? start.y() + rect.height() + handleWidth
                                                 : start.y() - handleWidth;
    if (TerminalDisplay* target = _model->_layout->displayAt(top, QPoint(newX, newY))) {
        target->setFocus(Qt::OtherFocusReason);
    }
}

void PaneNode::focusUp()
{
    focusDirection(Qt::Vertical, -1);
}

void PaneNode::focusDown()
{
    focusDirection(Qt::Vertical, +1);
}

void PaneNode::focusLeft()
{
    focusDirection(Qt::Horizontal, -1);
}

void PaneNode::focusRight()
{
    focusDirection(Qt::Horizontal, +1);
}

void PaneNode::focusNext(int direction)
{
    TerminalDisplay* display = activeDisplay();
    PaneNode* leaf = _model->leafIfAny(display);
    if (!leaf || !leaf->_parent) {
        return;
    }
    const auto terminals = leaf->_parent->toplevel()->displays();
    if (terminals.isEmpty()) {
        return;
    }
    const int id = terminals.indexOf(display);
    int target = (id + direction) % terminals.size();
    if (target < 0) {
        target = terminals.size() + target;
    }
    if (TerminalDisplay* targetDisplay = terminals.at(target)) {
        targetDisplay->setFocus(Qt::OtherFocusReason);
    }
}

void PaneNode::focusPrevious()
{
    focusNext(-1);
}

QString PaneNode::layoutString() const
{
    QString layout;
    for (PaneNode* child : _items) {
        layout += child->isLeaf() ? QString::number(child->display() ? child->display()->id() : -1) : child->layoutString();
        layout += QLatin1Char('|');
    }
    if (!layout.isEmpty()) {
        layout.chop(1);
    }
    layout = _orientation == Qt::Horizontal ? QLatin1Char('[') + layout + QLatin1Char(']') : QLatin1Char('{') + layout + QLatin1Char('}');
    return QStringLiteral("(%1)").arg(_id) + layout;
}

PaneModel::PaneModel(QObject* parent)
    : QObject(parent)
{
}

PaneModel::~PaneModel() = default;

void PaneModel::setLayout(PaneLayout* layout)
{
    _layout = layout;
}

PaneLayout* PaneModel::layout() const
{
    return _layout;
}

int PaneModel::takeSplitId()
{
    return ++lastSplitId;
}

void PaneModel::adopt(PaneNode* top)
{
    if (!top || top->_model == this) {
        return;
    }
    PaneModel* old = top->_model;
    QList<PaneNode*> pending = {top};
    while (!pending.isEmpty()) {
        PaneNode* node = pending.takeFirst();
        if (node->isLeaf() && node->display()) {
            old->_leaves.remove(node->display());
            disconnect(node->display(), &QObject::destroyed, old, nullptr);
            _leaves.insert(node->display(), node);
            watchDisplay(node->display());
        }
        node->_model = this;
        node->setParent(this);
        pending += node->_items;
    }
    sonicDebug() << "pane: split" << top->id() << "moved to another window";
}

void PaneModel::takeTab(PaneNode* top)
{
    const int index = indexOf(top);
    if (index < 0) {
        return;
    }
    _tabs.removeAll(top);
    Q_EMIT tabTaken(index, top);
}

PaneNode* PaneModel::createSplit()
{
    auto* split = new PaneNode(this, takeSplitId(), nullptr);
    sonicDebug() << "pane: split" << split->id() << "created";
    Q_EMIT splitCreated(split);
    return split;
}

PaneNode* PaneModel::leaf(TerminalDisplay* display)
{
    if (!display) {
        return nullptr;
    }
    if (PaneNode* existing = _leaves.value(display)) {
        return existing;
    }
    auto* leaf = new PaneNode(this, -1, display);
    _leaves.insert(display, leaf);
    watchDisplay(display);
    return leaf;
}

void PaneModel::watchDisplay(TerminalDisplay* display)
{
    connect(display, &QObject::destroyed, this, [this, display] {
        if (_layout && _layout->reportsRemovals()) {
            // The widget view reports the removal from the splitter's childEvent, which
            // comes after destroyed(); forget the leaf once that has happened.
            QMetaObject::invokeMethod(
                this,
                [this, display] {
                    delete _leaves.take(display);
                },
                Qt::QueuedConnection);
            return;
        }
        displayRemoved(display);
        delete _leaves.take(display);
    });
}

PaneNode* PaneModel::leafIfAny(const TerminalDisplay* display) const
{
    return _leaves.value(display);
}

void PaneModel::displayRemoved(TerminalDisplay* display)
{
    PaneNode* leaf = _leaves.value(display);
    if (!leaf || !leaf->_parent) {
        return;
    }
    PaneNode* old = leaf->_parent;
    leaf->takeFromParent();
    old->itemRemoved();
}

int PaneModel::count() const
{
    return tabsInView() ? _layout->tabCount() : _tabs.size();
}

PaneNode* PaneModel::tab(int index) const
{
    return tabsInView() ? _layout->tabAt(index) : _tabs.value(index);
}

int PaneModel::indexOf(const PaneNode* top) const
{
    for (int i = 0; i < count(); ++i) {
        if (tab(i) == top) {
            return i;
        }
    }
    return -1;
}

int PaneModel::currentIndex() const
{
    return tabsInView() ? _layout->currentTabIndex() : _currentIndex;
}

void PaneModel::setCurrentIndex(int index)
{
    if (tabsInView()) {
        return _layout->setCurrentTabIndex(index);
    }
    if (index < 0 || index >= _tabs.size() || index == _currentIndex) {
        return;
    }
    _currentIndex = index;
    Q_EMIT currentChanged(index);
}

PaneNode* PaneModel::currentTab() const
{
    return tab(currentIndex());
}

int PaneModel::insertTab(int index, PaneNode* top)
{
    if (tabsInView()) {
        index = _layout->insertTab(index, top);
    } else {
        if (index < 0 || index > _tabs.size()) {
            index = _tabs.size();
        }
        _tabs.insert(index, top);
        Q_EMIT tabInserted(index, top);
    }
    sonicDebug() << "pane: tab" << index << "inserted, split" << top->id();
    return index;
}

void PaneModel::moveTab(int from, int to)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to || tabsInView()) {
        return;
    }
    _tabs.move(from, to);
    Q_EMIT tabMoved(from, to);
}

bool PaneModel::tabsInView() const
{
    return _layout && _layout->ownsTabs();
}

PaneNode* PaneModel::findSplit(int id) const
{
    for (int i = 0; i < count(); ++i) {
        PaneNode* top = tab(i);
        if (top->id() == id) {
            return top;
        }
        if (PaneNode* found = top->childSplit(id)) {
            return found;
        }
    }
    return nullptr;
}

QList<TerminalDisplay*> PaneModel::displays() const
{
    return _layout ? _layout->allDisplays() : QList<TerminalDisplay*>();
}

void PaneModel::nodeDeleted(PaneNode* node)
{
    // With real tabs, the page's deletion removes the tab (TabbedViewContainer::viewDestroyed).
    const int index = _tabs.indexOf(node);
    if (index >= 0) {
        _tabs.removeAt(index);
        sonicDebug() << "pane: tab" << index << "removed";
        Q_EMIT tabRemoved(index, node);
    }
    Q_EMIT splitDeleted(node);
    if (node->_focusOnDeletion) {
        Q_EMIT focusRequested(node->_focusOnDeletion);
    }
}

namespace PaneModelRegistry {
    namespace {
        QHash<const QObject*, PaneModel*>& models()
        {
            static QHash<const QObject*, PaneModel*> map;
            return map;
        }
    }

    void add(const QObject* owner, PaneModel* model)
    {
        models().insert(owner, model);
    }

    void remove(const QObject* owner)
    {
        models().remove(owner);
    }

    PaneModel* model(const QObject* owner)
    {
        return models().value(owner);
    }
}
}

#include "moc_PaneModel.cpp"
