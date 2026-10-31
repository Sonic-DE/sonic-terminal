/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QImage>
#include <QKeySequence>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QRegion>
#include <QScrollerProperties>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "../characters/Character.h"
#include "Enumeration.h"
#include "ScreenWindow.h"
#include "ScrollState.h"
#include "colorscheme/ColorScheme.h"
#include "konsoleprivate_export.h"
#include "terminalDisplay/TerminalDisplay.h"

class QDrag;
class QLabel;
class QQuickWidget;
class QTimer;
class QVBoxLayout;
class KMessageWidget;
struct UBiDi;

namespace Konsole {
class FilterChain;
class HotSpot;
class IncrementalSearchBar;
class KonsolePrintManager;
class SessionController;
class TerminalColor;
class TerminalFont;
class TerminalHeaderBar;
class TerminalImageFilterChain;
class TerminalPainter;
class TerminalScrollBar;

// The terminal display as a Qt Quick item. TerminalDisplay is the frozen facade that old code
// and plugins call; the members its inline accessors read live in the facade and are shared.
class KONSOLEPRIVATE_EXPORT TerminalView : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("created with its session")
    // Active focus in the pane's find bar or header bar, which count as the terminal's.
    Q_PROPERTY(bool partHasFocus READ partHasFocus WRITE setPartHasFocus NOTIFY partHasFocusChanged)
    // In a QML window the facade's splitters and tabs own its size and visibility and the
    // item follows them; otherwise (a bare pane) the facade follows the item.
    Q_PROPERTY(bool followsFacade READ followsFacade WRITE setFollowsFacade NOTIFY followsFacadeChanged)

public:
    // Views created while one is alive go into a QML scene (the QML window) instead of
    // a QQuickWidget inside their facade (widget window, KPart).
    class KONSOLEPRIVATE_EXPORT SceneModeGuard {
    public:
        SceneModeGuard();
        ~SceneModeGuard();
        SceneModeGuard(const SceneModeGuard&) = delete;
        SceneModeGuard& operator=(const SceneModeGuard&) = delete;

    private:
        bool _previous;
    };

    explicit TerminalView(TerminalDisplay* display, QQuickItem* parent = nullptr);
    ~TerminalView() override;

    TerminalDisplay* display() const;
    bool sceneMode() const;
    bool partHasFocus() const;
    void setPartHasFocus(bool focused);
    bool followsFacade() const;
    void setFollowsFacade(bool follows);
    // The facade's labels (the resize label exists after the first size notification).
    QLabel* hoverLinkLabel() const;
    QLabel* resizeLabel() const;
    // Hosts this item inside the widget facade until the widget window and the KPart go.
    QQuickWidget* hostWidget() const;

    void showDragTarget(const QPoint& cursorPos);
    void hideDragTarget();
    void applyProfile(const QExplicitlySharedDataPointer<Profile>& profile);
    void setWallpaper(const ColorSchemeWallpaper::Ptr& p);
    FilterChain* filterChain() const;
    void processFilters();
    QSharedPointer<HotSpot> filterActions(const QPoint& position);
    void setBlinkingCursorEnabled(bool blink);
    void setAnimatingCursorEnabled(bool animate);
    void setBlinkingTextEnabled(bool blink);
    void setSessionController(SessionController* controller);
    SessionController* sessionController();
    Session::Ptr session() const;
    void setKeyboardCursorShape(Enum::CursorShapeEnum shape);
    void setCursorStyle(Enum::CursorShapeEnum shape, bool isBlinking, bool isAnimating, const QColor& customColor);
    void resetCursorStyle();
    void setSize(int columns, int lines);
    void propagateSize();
    QSize sizeHint() const;
    void setWordCharacters(const QString& wc);
    void resetCursor();
    void setPointerShape(Qt::CursorShape shape);
    void setScreenWindow(ScreenWindow* window);
    void selectCurrentLine();
    void selectAll();
    bool bracketedPasteMode() const;
    bool usesMouseTracking() const;
    void setAllowMouseTracking(bool allow);
    bool allowsMouseTracking() const;
    QRect imageToWidget(const QRect& imageArea) const;
    QRect widgetToImage(const QRect& widgetArea) const;
    QPair<int, int> getCharacterPosition(const QPoint& widgetPoint, bool edge) const;
    void setExpandedMode(bool expand);
    bool isCursorOnDisplay() const;
    QPoint cursorPosition() const;
    int selectionState() const;
    IncrementalSearchBar* searchBar() const;
    Character getCursorCharacter(int column, int line);
    int loc(int x, int y) const;
    void scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount);
    void outputSuspended(bool suspended);
    void updateReadOnlyState(bool readonly);
    void setSelectMode(bool mode);
    int bidiMap(Character* screenline,
        QString& line,
        int* log2line,
        int* line2log,
        uint16_t* shapemap,
        int32_t* vis2line,
        bool& shaped,
        bool shape = true,
        bool bidi = true) const;
    void showNotification(QString text);
    void clearMouseSelection();
    void removeLines(int lines);
    void setVisualCursorPosition(int x);
    void setHoverLinkIndicator(QString text);

    void updateImage();
    void copyToX11Selection(bool useSavedText = false);
    void copyToClipboard(Screen::DecodingOptions options = Screen::PlainText);
    void pasteFromClipboard(bool appendEnter = false);
    void pasteFromX11Selection(bool appendEnter = false);
    void setFlowControlWarningEnabled(bool enable);
    void setUsesMouseTracking(bool on);
    void setBracketedPasteMode(bool on);
    void bell(const QString& message);
    void printScreen();
    void notificationClicked(const QString& xdgActivationToken);
    void viewScrolledByUser();
    void setupHeaderVisibility();

    // Moves already drawn text, like QWidget::scroll() did for the widget.
    void scrollSnapshot(int dy, const QRect& area);

    // The facade focus bridge (setFocus() on the facade). A view not in a scene yet has the
    // focus at once, as a widget's focus-in, and takes active focus once it is in one;
    // dropFacadeFocus() withdraws that when another view takes the focus first.
    void takeFacadeFocus();
    void dropFacadeFocus();
    // Whether the terminal has the focus, as QWidget::hasFocus() answered for the widget (the
    // cursor is drawn filled then).
    bool terminalHasFocus() const;

    // Repaints that region of the text area; hides QQuickItem::update().
    void update();
    void update(const QRegion& region);

    void paint(QPainter* painter) override;

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void updatePolish() override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void touchEvent(QTouchEvent* event) override;

Q_SIGNALS:
    void partHasFocusChanged();
    void followsFacadeChanged();
    // The hover-link or resize label's text changed (QLabel tells nobody).
    void labelsChanged();

private:
    friend class TerminalDisplay;

    // What the facade widget answered in the widget window; the item in a QML scene.
    bool viewHasFocus() const;
    void viewSetFocus(Qt::FocusReason reason);
    QPoint viewMapFromGlobal(const QPoint& global) const;
    bool translucent() const;
    void setViewCursor(Qt::CursorShape shape);
    void setCompositeFocus(bool focused);
    // KCursor's auto-hide rule, for the item (kcursor.cpp: hide on keys, show on mouse).
    void autoHideCursorEvent(QEvent* event);
    // AutoScrollHandler's rule: keep extending a selection dragged past the edge.
    void autoScrollMove(QMouseEvent* event);

    // The kinetic scrolling the widget always had; the scroller works on the facade in a
    // widget, on the item in a QML scene.
    static QScrollerProperties kineticScrollerProperties();
    QObject* scrollerTarget();

    // Called by the facade once its scroll bar, colours and painter exist.
    void finishSetup();
    void widgetResized();
    void widgetShown();
    void widgetHidden();
    void contextMenu(QContextMenuEvent* event);
    void scrollPrepareEvent(QScrollPrepareEvent* event);
    void scrollEvent(QScrollEvent* event);
    void leave(QEvent* event);
    void paintRegion(QPainter& paint, const QRegion& region);
    // The facade's area the host widget covers: all but the scroll bar's column, which the
    // facade paints itself so the scroll bar blends onto the background as it always did.
    QRect hostRect() const;
    void updateHostGeometry();

    void extendSelection(const QPoint& position);
    void doDrag();
    uint charClass(const Character& ch) const;
    void clearImage();
    void mouseTripleClickEvent(QMouseEvent* ev);
    void selectLine(QPoint pos, bool entireLine, bool fromTripleClick = false);
    void blinkTextEvent();
    void blinkCursorEvent();
    QRect preeditRect() const;
    void showResizeNotification();
    void calcGeometry();
    void updateImageSize();
    void makeImage();
    void paintFilters(QPainter& painter);
    void updateCursor();
    bool handleShortcutOverrideEvent(QKeyEvent* keyEvent);
    void doPaste(QString text, bool appendReturn);
    void processMidButtonClick(QMouseEvent* ev);
    QPoint findLineStart(const QPoint& pnt);
    QPoint findLineEnd(const QPoint& pnt);
    QPoint findWordStart(const QPoint& pnt);
    QPoint findWordEnd(const QPoint& pnt);
    bool isInTerminalRegion(const QPoint& pnt) const;
    Screen::DecodingOptions currentDecodingOptions();
    KMessageWidget* createMessageWidget(const QString& text);
    void setAutoCopySelectedText(bool enabled);
    void setCopyTextAsHTML(bool enabled);
    void setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode);
    void setMargin(int margin);
    void setCenterContents(bool enable);
    void clearSelection();
    void sendMouseSignal(int button, QPoint pos, int eventType, bool sendInexact);

    QPointer<TerminalDisplay> _display;
    const bool _sceneMode;
    QQuickWidget* _hostWidget = nullptr;
    bool _partHasFocus = false;
    bool _focusWhenInScene = false;
    QPointF _lastHoverPosition;
    bool _followsFacade = false;
    // Scene mode: the current touch (touchEvent()).
    bool _touchDragged = false;
    QPointF _touchPressPosition;
    // Scene mode: Qt Quick delivers the second press of a double click too, widgets don't.
    ulong _lastPressTimestamp = 0;
    QPointF _lastPressPosition;
    Qt::MouseButton _lastPressButton = Qt::NoButton;
    QTimer* _autoHideTimer = nullptr;
    bool _cursorHidden = false;
    Qt::CursorShape _cursorShapeBeforeHiding = Qt::IBeamCursor;
    QTimer* _autoScrollTimer = nullptr;
    QImage _snapshot;
    QRegion _pendingRegion;

    // Shared with the facade: its inline accessors and TerminalDisplayAccessible read these.
    int& _lines;
    int& _columns;
    int& _usedLines;
    int& _usedColumns;
    QRect& _contentRect;
    bool& _openLinksByDirectClick;
    QPointer<ScreenWindow>& _screenWindow;
    bool& _flowControlWarningEnabled;
    bool& _hasCompositeFocus;
    std::shared_ptr<const ColorScheme>& _colorScheme;
    bool& _cursorBlinking;
    bool& _cursorAnimating;
    bool& _textBlinking;
    Enum::CursorShapeEnum& _cursorShape;
    bool& _bidiEnabled;
    bool& _semanticUpDown;
    ColorSchemeWallpaper::Ptr& _wallpaper;
    bool& _trimTrailingSpaces;
    Qt::Edge& _overlayEdge;
    bool& _readOnly;
    bool& _selectMode;

    // Owned by the facade: its child widgets and helpers.
    TerminalHeaderBar*& _headerBar;
    TerminalScrollBar*& _scrollBar;
    TerminalColor*& _terminalColor;
    std::unique_ptr<TerminalFont>& _terminalFont;
    IncrementalSearchBar*& _searchBar;
    QLabel*& _hoverLinkIndicator;
    QVBoxLayout*& _verticalLayout;
    TerminalImageFilterChain*& _filterChain;
    TerminalPainter*& _terminalPainter;
    std::unique_ptr<KonsolePrintManager>& _printManager;
    QLabel*& _resizeWidget;
    KMessageWidget*& _outputSuspendedMessageWidget;
    KMessageWidget*& _readOnlyMessageWidget;

    enum DragState {
        diNone,
        diPending,
        diDragging,
    };
    struct DragInfo {
        DragState state = diNone;
        QPoint start;
        QDrag* dragObject = nullptr;
    } _dragInfo;

    int _linesAtLastOsd = 1;
    int _columnsAtLastOsd = 1;
    int _prevCharacterLine = -1;
    int _prevCharacterColumn = -1;
    Character* _image = nullptr;
    int _imageSize = 0;
    QVector<LineProperty> _lineProperties;
    bool _resizing = false;
    bool _showTerminalSizeHint = true;
    bool _bidiLineLTR = true;
    bool _bidiTableDirOverride = true;
    bool _usesMouseTracking = false;
    bool _allowMouseTracking = true;
    bool _bracketedPasteMode = false;
    QPoint _iPntSel = QPoint(-1, -1);
    QPoint _pntSel = QPoint(-1, -1);
    QPoint _tripleSelBegin = QPoint(-1, -1);
    int _actSel = 0;
    bool _wordSelectionMode = false;
    bool _lineSelectionMode = false;
    bool _preserveLineBreaks = true;
    bool _columnSelectionMode = false;
    bool _autoCopySelectedText = false;
    bool _copyTextAsHTML = true;
    Enum::MiddleClickPasteModeEnum _middleClickPasteMode = Enum::PasteFromX11Selection;
    QString _wordCharacters = QStringLiteral(":@-./_~");
    TerminalBell _bell{Enum::NotifyBell};
    bool _allowBlinkingText = true;
    bool _allowBlinkingCursor = false;
    bool _allowAnimatingCursor = false;
    bool _hasTextBlinker = false;
    QTimer* _blinkTextTimer = nullptr;
    QTimer* _blinkCursorTimer = nullptr;
    bool _ctrlRequiredForDrag = true;
    bool _dropUrlsAsText = false;
    Enum::TripleClickModeEnum _tripleClickMode = Enum::SelectWholeLine;
    bool _possibleTripleClick = false;
    QPoint _tripleClickPos = QPoint(0, 0);
    QString _doubleClickSelectedText;
    QString _doubleClickSelectedHtml;
    QTimer* _resizeTimer = nullptr;
    QSize _size = QSize(0, 0);
    bool _filterUpdateRequired = true;
    TerminalDisplay::InputMethodData _inputMethodData;
    static const int TEXT_BLINK_DELAY = 500;
    static const int SIZE_HINT_DURATION = 1000;
    SessionController* _sessionController = nullptr;
    bool _trimLeadingSpaces = false;
    bool _mouseWheelZoom = false;
    int _margin = 1;
    bool _centerContents = false;
    bool _dimWhenInactive = false;
    int _dimValue = 0;
    bool _borderWhenActive = false;
    QColor _focusBorderColor;
    ScrollState _scrollWheelState;
    QRect _searchResultRect;
    bool _drawOverlay = false;
    bool _displayVerticalLine = false;
    int _displayVerticalLineAtChar = 80;
    QKeySequence _peekPrimaryShortcut;
    bool _semanticInputClick = false;
    UBiDi* ubidi = nullptr;
    QPoint _visualCursorPosition = {0, 0};
    int _selModeModifiers = 0;
    bool _selModeByModifiers = false;
    QRectF currentCursorRect;
    QRectF targetCursorRect;
    QTimer* animationTimer = nullptr;
};
}
