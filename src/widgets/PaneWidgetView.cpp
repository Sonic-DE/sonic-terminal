/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PaneWidgetView.h"

#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/TerminalHeaderBar.h"
#include "widgets/ViewContainer.h"
#include "widgets/ViewSplitter.h"

namespace Konsole {
namespace {
    QHash<const TabbedViewContainer*, PaneWidgetView*>& views()
    {
        static QHash<const TabbedViewContainer*, PaneWidgetView*> map;
        return map;
    }

    QHash<const ViewSplitter*, PaneNode*>& nodes()
    {
        static QHash<const ViewSplitter*, PaneNode*> map;
        return map;
    }

    QHash<const PaneNode*, ViewSplitter*>& splitters()
    {
        static QHash<const PaneNode*, ViewSplitter*> map;
        return map;
    }

    TerminalDisplay* terminalFromWidget(QWidget* widget)
    {
        if (auto* terminal = qobject_cast<TerminalDisplay*>(widget)) {
            return terminal;
        }
        return widget != nullptr ? widget->findChild<TerminalDisplay*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    }
}

PaneWidgetView::PaneWidgetView(PaneModel* model, TabbedViewContainer* container)
    : QObject(container)
    , _model(model)
    , _container(container)
{
    views().insert(container, this);
    model->setLayout(this);

    connect(model, &PaneModel::splitCreated, this, &PaneWidgetView::splitCreated);
    connect(model, &PaneModel::splitDeleted, this, &PaneWidgetView::splitDeleted);
    // itemReparented needs nothing here: QSplitter::insertWidget (itemInserted) reparents and
    // shows the widget itself, as ViewSplitter's own code did.
    connect(model, &PaneModel::itemInserted, this, [this](PaneNode* parent, int index, PaneNode* item) {
        if (ViewSplitter* splitter = splitterOf(parent); splitter && widget(item)) {
            splitter->insertWidget(index, widget(item));
        }
    });
    connect(model, &PaneModel::itemRemoved, this, [this](PaneNode*, PaneNode* item) {
        if (QWidget* child = widget(item)) {
            child->setParent(nullptr);
        }
    });
    connect(model, &PaneModel::itemReplaced, this, [this](PaneNode* parent, int index, PaneNode*, PaneNode* item) {
        if (ViewSplitter* splitter = splitterOf(parent); splitter && widget(item)) {
            splitter->replaceWidget(index, widget(item));
        }
    });
    connect(model, &PaneModel::orientationChanged, this, [](PaneNode* split) {
        if (ViewSplitter* splitter = splitterOf(split)) {
            splitter->setOrientation(split->orientation());
        }
    });
    connect(model, &PaneModel::hiddenChanged, this, [this](PaneNode* node) {
        if (QWidget* child = widget(node)) {
            child->setVisible(!node->isHidden());
        }
        // Maximizing hides nodes; ViewSplitter::terminalMaximized() is inline.
        if (ViewSplitter* top = splitterOf(node->toplevel())) {
            top->m_terminalMaximized = node->toplevel()->terminalMaximized();
        }
    });
    connect(model, &PaneModel::tabTaken, this, [](int, PaneNode* top) {
        if (ViewSplitter* page = splitterOf(top)) {
            page->setParent(nullptr);
        }
    });
    connect(model, &PaneModel::treeChanged, this, &PaneWidgetView::treeChanged);
    connect(model, &PaneModel::focusRequested, this, [this](PaneNode* node) {
        if (QWidget* target = widget(node)) {
            target->setFocus();
        }
    });
}

bool PaneWidgetView::reportsRemovals() const
{
    return true;
}

bool PaneWidgetView::ownsTabs() const
{
    return _container != nullptr;
}

int PaneWidgetView::tabCount() const
{
    return _container ? _container->count() : 0;
}

PaneNode* PaneWidgetView::tabAt(int index) const
{
    return _container ? nodeOf(qobject_cast<ViewSplitter*>(_container->widget(index))) : nullptr;
}

int PaneWidgetView::currentTabIndex() const
{
    return _container ? _container->currentIndex() : -1;
}

void PaneWidgetView::setCurrentTabIndex(int index)
{
    if (_container) {
        _container->setCurrentIndex(index);
    }
}

int PaneWidgetView::insertTab(int index, PaneNode* top)
{
    return _container ? _container->insertTab(index, splitterOf(top), QString()) : -1;
}

PaneWidgetView::~PaneWidgetView()
{
    views().remove(_container);
    if (_model && _model->layout() == this) {
        _model->setLayout(nullptr);
    }
}

PaneWidgetView* PaneWidgetView::forContainer(const TabbedViewContainer* container)
{
    return views().value(container);
}

PaneNode* PaneWidgetView::nodeOf(const ViewSplitter* splitter)
{
    return nodes().value(splitter);
}

ViewSplitter* PaneWidgetView::splitterOf(const PaneNode* node)
{
    return splitters().value(node);
}

PaneModel* PaneWidgetView::model() const
{
    return _model;
}

QWidget* PaneWidgetView::widget(const PaneNode* node) const
{
    if (!node) {
        return nullptr;
    }
    if (node->isLeaf()) {
        return node->display();
    }
    return splitterOf(node);
}

void PaneWidgetView::splitCreated(PaneNode* split)
{
    auto* splitter = new ViewSplitter(split->id(), nullptr);
    nodes().insert(splitter, split);
    splitters().insert(split, splitter);
    // When the model goes first (the ViewManager is deleted before its container), the
    // splitter falls back to its own rules instead of reaching a deleted node.
    connect(split, &QObject::destroyed, splitter, [splitter, split] {
        nodes().remove(splitter);
        splitters().remove(split);
    });
    connect(splitter, &QObject::destroyed, split, [splitter, split] {
        nodes().remove(splitter);
        splitters().remove(split);
    });
}

void PaneWidgetView::splitDeleted(PaneNode* split)
{
    if (ViewSplitter* splitter = splitters().take(split)) {
        nodes().remove(splitter);
        delete splitter;
    }
}

void PaneWidgetView::treeChanged(PaneNode* top)
{
    // ViewSplitter::childEvent did this after every change.
    const auto terminals = top->displays();
    for (TerminalDisplay* terminal : terminals) {
        terminal->headerBar()->applyVisibilitySettings();
    }
}

QList<int> PaneWidgetView::sizes(const PaneNode* node) const
{
    ViewSplitter* splitter = splitterOf(node);
    return splitter ? splitter->sizes() : QList<int>();
}

void PaneWidgetView::setSizes(PaneNode* node, const QList<int>& sizes)
{
    if (ViewSplitter* splitter = splitterOf(node)) {
        splitter->setSizes(sizes);
    }
}

void PaneWidgetView::refresh(PaneNode* node)
{
    if (ViewSplitter* splitter = splitterOf(node)) {
        splitter->refresh();
    }
}

int PaneWidgetView::extent(const PaneNode* node) const
{
    ViewSplitter* splitter = splitterOf(node);
    if (!splitter) {
        return 0;
    }
    return splitter->orientation() == Qt::Horizontal ? splitter->width() : splitter->height();
}

QRect PaneWidgetView::geometry(const PaneNode* node) const
{
    QWidget* child = widget(node);
    if (!child || !node) {
        return {};
    }
    PaneNode* top = const_cast<PaneNode*>(node)->toplevel();
    QWidget* topWidget = widget(top);
    if (child == topWidget || !topWidget) {
        return QRect(QPoint(0, 0), child->size());
    }
    return QRect(child->mapTo(topWidget, QPoint(0, 0)), child->size());
}

int PaneWidgetView::handleWidth(const PaneNode* node) const
{
    ViewSplitter* splitter = splitterOf(node);
    return splitter ? splitter->handleWidth() : 0;
}

TerminalDisplay* PaneWidgetView::focusedDisplay(const PaneNode* node) const
{
    QWidget* root = widget(node);
    if (!root) {
        return nullptr;
    }
    // ViewSplitter::activeTerminalDisplay(): a display's child may be the focus widget.
    QWidget* focused = root->focusWidget();
    auto* terminal = qobject_cast<TerminalDisplay*>(focused);
    while (terminal == nullptr && focused != nullptr && focused != root) {
        focused = focused->parentWidget();
        terminal = qobject_cast<TerminalDisplay*>(focused);
    }
    return terminal;
}

PaneNode* PaneWidgetView::focusedSplit(const PaneNode* node) const
{
    QWidget* root = widget(node);
    if (!root) {
        return nullptr;
    }
    QWidget* current = root->focusWidget() != nullptr ? root->focusWidget() : root;
    ViewSplitter* splitter = nullptr;
    while (splitter == nullptr && current != nullptr) {
        splitter = qobject_cast<ViewSplitter*>(current);
        current = current->parentWidget();
    }
    return splitter ? nodeOf(splitter) : nullptr;
}

QList<TerminalDisplay*> PaneWidgetView::displays(const PaneNode* node) const
{
    QWidget* root = widget(node);
    return root ? root->findChildren<TerminalDisplay*>() : QList<TerminalDisplay*>();
}

TerminalDisplay* PaneWidgetView::firstDisplay(const PaneNode* node) const
{
    QWidget* root = widget(node);
    return root ? root->findChild<TerminalDisplay*>() : nullptr;
}

QList<PaneNode*> PaneWidgetView::splits(const PaneNode* node) const
{
    QList<PaneNode*> result;
    QWidget* root = widget(node);
    if (!root) {
        return result;
    }
    const auto all = root->findChildren<ViewSplitter*>();
    for (ViewSplitter* splitter : all) {
        if (PaneNode* split = nodeOf(splitter)) {
            result.append(split);
        }
    }
    return result;
}

QList<TerminalDisplay*> PaneWidgetView::allDisplays() const
{
    return _container ? _container->findChildren<TerminalDisplay*>() : QList<TerminalDisplay*>();
}

TerminalDisplay* PaneWidgetView::displayAt(const PaneNode* top, const QPoint& point) const
{
    QWidget* topWidget = widget(top);
    if (!topWidget) {
        return nullptr;
    }
    // ViewSplitter::handleFocusDirection()'s hit test.
    QWidget* child = topWidget->childAt(point);
    TerminalDisplay* terminal = terminalFromWidget(child);
    if (terminal == nullptr && qobject_cast<QSplitterHandle*>(child) != nullptr) {
        auto* target = qobject_cast<QSplitter*>(child->parent());
        terminal = terminalFromWidget(target->widget(0));
    } else if (terminal == nullptr && child != nullptr) {
        while (child != nullptr && terminal == nullptr) {
            terminal = qobject_cast<TerminalDisplay*>(child->parentWidget());
            child = child->parentWidget();
        }
    }
    return terminal;
}
}

#include "moc_PaneWidgetView.cpp"
