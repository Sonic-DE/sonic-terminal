/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "terminalDisplay/TerminalDisplay.h"
#include "KonsoleSettings.h"

// Qt
#include <QAccessible>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

// Konsole
#include "extras/AutoScrollHandler.h"
#include "extras/CompositeWidgetFocusWatcher.h"

#include "filterHotSpots/HotSpot.h"
#include "filterHotSpots/TerminalImageFilterChain.h"

#include "../quick/TerminalViewAccessible.h"
#include "../widgets/KonsolePrintManager.h"
#include "../widgets/TerminalDisplayAccessible.h"
#include "quick/TerminalView.h"
#include "quick/TerminalViewRegistry.h"
#include "widgets/IncrementalSearchBar.h"

#include "TerminalColor.h"
#include "TerminalFonts.h"
#include "TerminalPainter.h"
#include "TerminalScrollBar.h"

using namespace Konsole;

// The display is implemented by its TerminalView; this class keeps the widget API, signals and
// data layout that existing code and plugins were built against.
static TerminalView* viewOf(const TerminalDisplay* display)
{
    return TerminalViewRegistry::view(display);
}

int TerminalDisplay::loc(int x, int y) const
{
    return viewOf(this)->loc(x, y);
}

void TerminalDisplay::setScreenWindow(ScreenWindow* window)
{
    viewOf(this)->setScreenWindow(window);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Accessibility                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

namespace Konsole
{
#ifndef QT_NO_ACCESSIBILITY
/**
 * This function installs the factory function which lets Qt instantiate the QAccessibleInterface
 * for the TerminalDisplay.
 */
QAccessibleInterface *accessibleInterfaceFactory(const QString &key, QObject *object)
{
    Q_UNUSED(key)
    if (auto *display = qobject_cast<TerminalDisplay *>(object)) {
        return new TerminalDisplayAccessible(display);
    }
    // Asked before Qt Quick's factory, which answers for the class name "QQuickItem".
    if (auto* view = qobject_cast<TerminalView*>(object); view && view->sceneMode()) {
        return new TerminalViewAccessible(view);
    }
    return nullptr;
}

#endif
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

int TerminalDisplay::lastViewId = -1;

TerminalDisplay::TerminalDisplay(QWidget *parent)
    : QWidget(parent)
    , _verticalLayout(new QVBoxLayout(this))
    , _filterChain(new TerminalImageFilterChain(this))
    , _searchBar(new IncrementalSearchBar(this))
    , _headerBar(new TerminalHeaderBar(this))
    , _terminalFont(std::make_unique<TerminalFont>(this))
    , _id(++lastViewId)
{
    auto* view = new TerminalView(this);

    // terminal applications are not designed with Right-To-Left in mind,
    // so the layout is forced to Left-To-Right
    setLayoutDirection(Qt::LeftToRight);

    // Auto uppercase and predictive text doesn't make sense for terminal.
    setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);

    // create scroll bar for scrolling output up and down
    _scrollBar = new TerminalScrollBar(this);
    _scrollBar->setAutoFillBackground(false);
    // set the scroll bar's slider to occupy the whole area of the scroll bar initially
    _scrollBar->setScroll(0, 0);
    _scrollBar->setCursor(Qt::ArrowCursor);
    _headerBar->setCursor(Qt::ArrowCursor);
    connect(_headerBar, &TerminalHeaderBar::requestToggleExpansion, this, &Konsole::TerminalDisplay::requestToggleExpansion);
    connect(_headerBar, &TerminalHeaderBar::requestMoveToNewTab, this, [this] {
        requestMoveToNewTab(this);
    });

    _hoverLinkIndicator = new QLabel(this);
    _hoverLinkIndicator->setStyleSheet(QStringLiteral("background-color:palette(window); padding: 2px;"));
    _hoverLinkIndicator->setVisible(false);

    setMouseTracking(true);

    // Enable drag and drop support
    setAcceptDrops(true);

    setFocusPolicy(Qt::WheelFocus);

    // enable input method support
    setAttribute(Qt::WA_InputMethodEnabled, true);

    // this is an important optimization, it tells Qt
    // that TerminalDisplay will handle repainting its entire area.
    setAttribute(Qt::WA_OpaquePaintEvent);

    setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Add the stretch item once, the KMessageWidgets are inserted at index 0.
    _verticalLayout->addWidget(_headerBar);
    _verticalLayout->addStretch();
    _verticalLayout->setSpacing(0);
    _verticalLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(_verticalLayout);
    new AutoScrollHandler(this);

    _terminalColor = new TerminalColor(this);
    connect(_terminalColor, &TerminalColor::onPalette, _scrollBar, &TerminalScrollBar::updatePalette);

    _terminalPainter = new TerminalPainter(this);

    view->finishSetup();

    // Keep this last
    CompositeWidgetFocusWatcher *focusWatcher = new CompositeWidgetFocusWatcher(this);
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged, this, [this](bool focused) {
        _hasCompositeFocus = focused;
    });
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged, this, &TerminalDisplay::compositeFocusChanged);
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged, _headerBar, &TerminalHeaderBar::setFocusIndicatorState);

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(Konsole::accessibleInterfaceFactory);
#endif

    connect(KonsoleSettings::self(), &KonsoleSettings::configChanged, this, &TerminalDisplay::setupHeaderVisibility);
}

TerminalDisplay::~TerminalDisplay()
{
    delete TerminalViewRegistry::view(this);
    TerminalViewRegistry::remove(this);

    delete _filterChain;
}

void TerminalDisplay::setupHeaderVisibility()
{
    viewOf(this)->setupHeaderVisibility();
}

void TerminalDisplay::hideDragTarget()
{
    viewOf(this)->hideDragTarget();
}

void TerminalDisplay::showDragTarget(const QPoint &cursorPos)
{
    viewOf(this)->showDragTarget(cursorPos);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setKeyboardCursorShape(Enum::CursorShapeEnum shape)
{
    viewOf(this)->setKeyboardCursorShape(shape);
}

void TerminalDisplay::setCursorStyle(Enum::CursorShapeEnum shape, bool isBlinking, bool isAnimating, const QColor &customColor)
{
    viewOf(this)->setCursorStyle(shape, isBlinking, isAnimating, customColor);
}

void TerminalDisplay::resetCursorStyle()
{
    viewOf(this)->resetCursorStyle();
}

void TerminalDisplay::setWallpaper(const ColorSchemeWallpaper::Ptr &p)
{
    viewOf(this)->setWallpaper(p);
}

void TerminalDisplay::scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount)
{
    viewOf(this)->scrollScreenWindow(mode, amount);
}

void TerminalDisplay::processFilters()
{
    viewOf(this)->processFilters();
}

void TerminalDisplay::updateImage()
{
    viewOf(this)->updateImage();
}

void TerminalDisplay::showResizeNotification()
{
    viewOf(this)->showResizeNotification();
}

void TerminalDisplay::showNotification(QString text)
{
    viewOf(this)->showNotification(text);
}

void TerminalDisplay::paintEvent(QPaintEvent *pe)
{
    auto* view = viewOf(this);
    // In a QML scene the item draws everything; the facade's window is never on screen.
    if (!view || view->sceneMode()) {
        return;
    }
    // The scroll bar's column is drawn here, under the scroll bar, with the view's paint code;
    // the rest is the view's and is redrawn there.
    const QRect host = view->hostRect();
    const QRegion outside = pe->region() - host;
    QPainter paint(this);
    // Leave nothing under the host: grab() and render() draw its image over this widget's pixels.
    paint.setCompositionMode(QPainter::CompositionMode_Source);
    paint.fillRect(host & pe->region().boundingRect(), Qt::transparent);
    paint.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (!outside.isEmpty()) {
        paint.setClipRegion(outside);
        view->paintRegion(paint, outside);
    }
    view->update(pe->region() & host);
}

QPoint TerminalDisplay::cursorPosition() const
{
    return viewOf(this)->cursorPosition();
}

void TerminalDisplay::setVisualCursorPosition(int x)
{
    viewOf(this)->setVisualCursorPosition(x);
}

void TerminalDisplay::setHoverLinkIndicator(QString text)
{
    viewOf(this)->setHoverLinkIndicator(text);
}

bool TerminalDisplay::isCursorOnDisplay() const
{
    return viewOf(this)->isCursorOnDisplay();
}

FilterChain *TerminalDisplay::filterChain() const
{
    return _filterChain;
}

void TerminalDisplay::paintFilters(QPainter &painter)
{
    viewOf(this)->paintFilters(painter);
}

QRect TerminalDisplay::imageToWidget(const QRect &imageArea) const
{
    return viewOf(this)->imageToWidget(imageArea);
}

QRect TerminalDisplay::widgetToImage(const QRect &widgetArea) const
{
    return viewOf(this)->widgetToImage(widgetArea);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Blinking Text & Cursor                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setBlinkingCursorEnabled(bool blink)
{
    viewOf(this)->setBlinkingCursorEnabled(blink);
}

void TerminalDisplay::setAnimatingCursorEnabled(bool animate)
{
    viewOf(this)->setAnimatingCursorEnabled(animate);
}

void TerminalDisplay::setBlinkingTextEnabled(bool blink)
{
    viewOf(this)->setBlinkingTextEnabled(blink);
}

void TerminalDisplay::focusOutEvent(QFocusEvent* event)
{
    viewOf(this)->focusOutEvent(event);
}

void TerminalDisplay::focusInEvent(QFocusEvent* event)
{
    viewOf(this)->focusInEvent(event);
}

void TerminalDisplay::blinkTextEvent()
{
    viewOf(this)->blinkTextEvent();
}

void TerminalDisplay::blinkCursorEvent()
{
    viewOf(this)->blinkCursorEvent();
}

void TerminalDisplay::updateCursor()
{
    viewOf(this)->updateCursor();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Geometry & Resizing                              */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    viewOf(this)->widgetResized();
}

void TerminalDisplay::propagateSize()
{
    viewOf(this)->propagateSize();
}

void TerminalDisplay::updateImageSize()
{
    viewOf(this)->updateImageSize();
}

void TerminalDisplay::makeImage()
{
    viewOf(this)->makeImage();
}

void TerminalDisplay::clearImage()
{
    viewOf(this)->clearImage();
}

void TerminalDisplay::calcGeometry()
{
    viewOf(this)->calcGeometry();
}

void TerminalDisplay::setSize(int columns, int lines)
{
    viewOf(this)->setSize(columns, lines);
}

QSize TerminalDisplay::sizeHint() const
{
    return viewOf(this)->sizeHint();
}

void TerminalDisplay::showEvent(QShowEvent *)
{
    viewOf(this)->widgetShown();
}

void TerminalDisplay::hideEvent(QHideEvent *)
{
    viewOf(this)->widgetHidden();
}

void TerminalDisplay::setMargin(int margin)
{
    viewOf(this)->setMargin(margin);
}

void TerminalDisplay::setCenterContents(bool enable)
{
    viewOf(this)->setCenterContents(enable);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                  Mouse                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::mousePressEvent(QMouseEvent* ev)
{
    viewOf(this)->mousePressEvent(ev);
}

QSharedPointer<HotSpot> TerminalDisplay::filterActions(const QPoint& position)
{
    return viewOf(this)->filterActions(position);
}

void TerminalDisplay::mouseMoveEvent(QMouseEvent* ev)
{
    viewOf(this)->mouseMoveEvent(ev);
}

void TerminalDisplay::sendMouseSignal(int button, QPoint pos, int eventType, bool sendInexact)
{
    viewOf(this)->sendMouseSignal(button, pos, eventType, sendInexact);
}

void TerminalDisplay::leaveEvent(QEvent* ev)
{
    viewOf(this)->leave(ev);
}

void TerminalDisplay::extendSelection(const QPoint& position)
{
    viewOf(this)->extendSelection(position);
}

void TerminalDisplay::mouseReleaseEvent(QMouseEvent* ev)
{
    viewOf(this)->mouseReleaseEvent(ev);
}

QPair<int, int> TerminalDisplay::getCharacterPosition(const QPoint& widgetPoint, bool edge) const
{
    return viewOf(this)->getCharacterPosition(widgetPoint, edge);
}

void TerminalDisplay::setExpandedMode(bool expand)
{
    viewOf(this)->setExpandedMode(expand);
}

void TerminalDisplay::processMidButtonClick(QMouseEvent *ev)
{
    viewOf(this)->processMidButtonClick(ev);
}

void TerminalDisplay::mouseDoubleClickEvent(QMouseEvent *ev)
{
    viewOf(this)->mouseDoubleClickEvent(ev);
}

void TerminalDisplay::wheelEvent(QWheelEvent *ev)
{
    viewOf(this)->wheelEvent(ev);
}

void TerminalDisplay::viewScrolledByUser()
{
    viewOf(this)->viewScrolledByUser();
}

QPoint TerminalDisplay::findLineStart(const QPoint &pnt)
{
    return viewOf(this)->findLineStart(pnt);
}

QPoint TerminalDisplay::findLineEnd(const QPoint &pnt)
{
    return viewOf(this)->findLineEnd(pnt);
}

QPoint TerminalDisplay::findWordStart(const QPoint &pnt)
{
    return viewOf(this)->findWordStart(pnt);
}

QPoint TerminalDisplay::findWordEnd(const QPoint &pnt)
{
    return viewOf(this)->findWordEnd(pnt);
}

bool TerminalDisplay::isInTerminalRegion(const QPoint &point) const
{
    return viewOf(this)->isInTerminalRegion(point);
}

Screen::DecodingOptions TerminalDisplay::currentDecodingOptions()
{
    return viewOf(this)->currentDecodingOptions();
}

void TerminalDisplay::notificationClicked(const QString &xdgActivationToken)
{
    viewOf(this)->notificationClicked(xdgActivationToken);
}

void TerminalDisplay::mouseTripleClickEvent(QMouseEvent *ev)
{
    viewOf(this)->mouseTripleClickEvent(ev);
}

void TerminalDisplay::removeLines(int lines)
{
    viewOf(this)->removeLines(lines);
}

void TerminalDisplay::selectLine(QPoint pos, bool entireLine, bool fromTripleClick)
{
    viewOf(this)->selectLine(pos, entireLine, fromTripleClick);
}

void TerminalDisplay::selectCurrentLine()
{
    viewOf(this)->selectCurrentLine();
}

void TerminalDisplay::selectAll()
{
    viewOf(this)->selectAll();
}

bool TerminalDisplay::focusNextPrevChild(bool next)
{
    // for 'Tab', always disable focus switching among widgets
    // for 'Shift+Tab', leave the decision to higher level
    if (next) {
        return false;
    } else {
        return QWidget::focusNextPrevChild(next);
    }
}

uint TerminalDisplay::charClass(const Character &ch) const
{
    return viewOf(this)->charClass(ch);
}

void TerminalDisplay::setWordCharacters(const QString &wc)
{
    viewOf(this)->setWordCharacters(wc);
}

void TerminalDisplay::setUsesMouseTracking(bool on)
{
    viewOf(this)->setUsesMouseTracking(on);
}

void TerminalDisplay::setAllowMouseTracking(bool allow)
{
    viewOf(this)->setAllowMouseTracking(allow);
}

bool TerminalDisplay::allowsMouseTracking() const
{
    return viewOf(this)->allowsMouseTracking();
}

void TerminalDisplay::resetCursor()
{
    viewOf(this)->resetCursor();
}

void TerminalDisplay::setPointerShape(Qt::CursorShape shape)
{
    viewOf(this)->setPointerShape(shape);
}

bool TerminalDisplay::usesMouseTracking() const
{
    return viewOf(this)->usesMouseTracking();
}

void TerminalDisplay::setBracketedPasteMode(bool on)
{
    viewOf(this)->setBracketedPasteMode(on);
}

bool TerminalDisplay::bracketedPasteMode() const
{
    return viewOf(this)->bracketedPasteMode();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Touch & Scroll                              */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::scrollPrepareEvent(QScrollPrepareEvent *event)
{
    viewOf(this)->scrollPrepareEvent(event);
}

void TerminalDisplay::scrollEvent(QScrollEvent *event)
{
    viewOf(this)->scrollEvent(event);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Clipboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::doPaste(QString text, bool appendReturn)
{
    viewOf(this)->doPaste(text, appendReturn);
}

void TerminalDisplay::setAutoCopySelectedText(bool enabled)
{
    viewOf(this)->setAutoCopySelectedText(enabled);
}

void TerminalDisplay::setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode)
{
    viewOf(this)->setMiddleClickPasteMode(mode);
}

void TerminalDisplay::setCopyTextAsHTML(bool enabled)
{
    viewOf(this)->setCopyTextAsHTML(enabled);
}

void TerminalDisplay::copyToX11Selection(bool useSavedText)
{
    viewOf(this)->copyToX11Selection(useSavedText);
}

void TerminalDisplay::copyToClipboard(Screen::DecodingOptions options)
{
    viewOf(this)->copyToClipboard(options);
}

void TerminalDisplay::pasteFromClipboard(bool appendEnter)
{
    viewOf(this)->pasteFromClipboard(appendEnter);
}

void TerminalDisplay::pasteFromX11Selection(bool appendEnter)
{
    viewOf(this)->pasteFromX11Selection(appendEnter);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Input Method                               */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::inputMethodEvent(QInputMethodEvent *event)
{
    viewOf(this)->inputMethodEvent(event);
}

QVariant TerminalDisplay::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImCursorRectangle:
    case Qt::ImFont:
        return viewOf(this)->inputMethodQuery(query);
    default:
        return QWidget::inputMethodQuery(query);
    }
}

QRect TerminalDisplay::preeditRect() const
{
    return viewOf(this)->preeditRect();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Keyboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setFlowControlWarningEnabled(bool enable)
{
    viewOf(this)->setFlowControlWarningEnabled(enable);
}

void TerminalDisplay::outputSuspended(bool suspended)
{
    viewOf(this)->outputSuspended(suspended);
}

KMessageWidget *TerminalDisplay::createMessageWidget(const QString &text)
{
    return viewOf(this)->createMessageWidget(text);
}

void TerminalDisplay::setSelectMode(bool mode)
{
    viewOf(this)->setSelectMode(mode);
}

void TerminalDisplay::updateReadOnlyState(bool readonly)
{
    viewOf(this)->updateReadOnlyState(readonly);
}

void TerminalDisplay::keyPressEvent(QKeyEvent *event)
{
    viewOf(this)->keyPressEvent(event);
}

void TerminalDisplay::keyReleaseEvent(QKeyEvent *event)
{
    const bool readOnly = _readOnly;
    viewOf(this)->keyReleaseEvent(event);
    // The widget accepted releases only while read-only; otherwise they travel on to its parent.
    if (!readOnly) {
        QWidget::keyReleaseEvent(event);
    }
}

bool TerminalDisplay::handleShortcutOverrideEvent(QKeyEvent *keyEvent)
{
    return viewOf(this)->handleShortcutOverrideEvent(keyEvent);
}

bool TerminalDisplay::event(QEvent *event)
{
    bool eventHandled = false;
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        eventHandled = handleShortcutOverrideEvent(static_cast<QKeyEvent *>(event));
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        if (_terminalColor) {
            _terminalColor->onColorsChanged();
        }
        break;
    case QEvent::FocusOut:
    case QEvent::FocusIn:
        if (_screenWindow != nullptr) {
            // force a redraw on focusIn, fixes the
            // black screen bug when the view is focused
            // but doesn't redraws.
            _screenWindow->notifyOutputChanged();
        }
        update();
        break;

    case QEvent::ScrollPrepare:
        scrollPrepareEvent(static_cast<QScrollPrepareEvent *>(event));
        break;
    case QEvent::Scroll:
        scrollEvent(static_cast<QScrollEvent *>(event));
        break;

    default:
        break;
    }
    return eventHandled ? true : QWidget::event(event);
}

void TerminalDisplay::contextMenuEvent(QContextMenuEvent *event)
{
    viewOf(this)->contextMenu(event);
}

/* --------------------------------------------------------------------- */
/*                                                                       */
/*                                  Bell                                 */
/*                                                                       */
/* --------------------------------------------------------------------- */

void TerminalDisplay::bell(const QString &message)
{
    viewOf(this)->bell(message);
}

/* --------------------------------------------------------------------- */
/*                                                                       */
/* Drag & Drop                                                           */
/*                                                                       */
/* --------------------------------------------------------------------- */

void TerminalDisplay::dragEnterEvent(QDragEnterEvent *event)
{
    viewOf(this)->dragEnterEvent(event);
}

void TerminalDisplay::dropEvent(QDropEvent *event)
{
    viewOf(this)->dropEvent(event);
}

void TerminalDisplay::doDrag()
{
    viewOf(this)->doDrag();
}

void TerminalDisplay::setSessionController(SessionController *controller)
{
    viewOf(this)->setSessionController(controller);
}

SessionController *TerminalDisplay::sessionController()
{
    return viewOf(this)->sessionController();
}

Session::Ptr TerminalDisplay::session() const
{
    return viewOf(this)->session();
}

IncrementalSearchBar *TerminalDisplay::searchBar() const
{
    return _searchBar;
}

void TerminalDisplay::applyProfile(const Profile::Ptr &profile)
{
    viewOf(this)->applyProfile(profile);
}

void TerminalDisplay::printScreen()
{
    viewOf(this)->printScreen();
}

Character TerminalDisplay::getCursorCharacter(int column, int line)
{
    return viewOf(this)->getCursorCharacter(column, line);
}

int TerminalDisplay::selectionState() const
{
    return viewOf(this)->selectionState();
}

void TerminalDisplay::clearMouseSelection()
{
    viewOf(this)->clearMouseSelection();
}

int TerminalDisplay::bidiMap(Character *screenline,
                             QString &line,
                             int *log2line,
                             int *line2log,
                             uint16_t *shapemap,
                             int32_t *vis2line,
                             bool &shaped,
                             bool shape,
                             bool bidi) const
{
    return viewOf(this)->bidiMap(screenline, line, log2line, line2log, shapemap, vis2line, shaped, shape, bidi);
}

void TerminalDisplay::clearSelection()
{
    viewOf(this)->clearSelection();
}

#include "moc_TerminalDisplay.cpp"
