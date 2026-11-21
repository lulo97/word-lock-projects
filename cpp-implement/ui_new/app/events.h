#pragma once
#include "dom/node.h"
#include "controls/control.h"

namespace ui {

// Tracks hover/focus/active elements and dispatches input.
class EventState {
public:
    Element* hovered = nullptr;
    Element* focused = nullptr;
    Element* active = nullptr;     // element under mouse button press
    Element* captured = nullptr;   // drag capture (slider, selection)

    // Topmost element at a point honoring z-order, overflow clip, pointer-events.
    // In: root, x, y (document coords incl. scroll). Out: element or nullptr.
    static Element* hitTest(Element& root, float x, float y);

    // Update hover from mouse move. Out: true if state changed (needs restyle/repaint).
    bool onMouseMove(Element& root, float x, float y, bool leftDown);

    // Mouse down/up: sets focus, active, captured, calls control + click handler.
    // Out: true if repaint needed.
    bool onMouseDown(Element& root, float x, float y);
    bool onMouseUp(Element& root, float x, float y);

    // Wheel: scroll nearest overflow-y-auto ancestor of hovered. Out: true if scrolled.
    bool onWheel(Element& root, float x, float y, int delta);

    // Keyboard routing to the focused control; Tab/Shift+Tab move focus. Out: true if handled.
    bool onKeyDown(Element& root, const KeyEvent& e);
    bool onChar(Element& root, wchar_t ch);

    // Set focus to an element (updates stateFlags, notifies controls). Out: none.
    void setFocus(Element* el);

    // Tab order: all focusable elements in document order.
    std::vector<Element*> focusOrder(Element& root) const;

    // Cursor to show now. Out: kind based on hovered element.
    CursorKind currentCursor() const;

    // Click handlers registered by id/element.
    std::function<void(Element&)> onClickAny;
};

} // namespace ui
