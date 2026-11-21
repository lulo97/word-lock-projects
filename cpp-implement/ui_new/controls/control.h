#pragma once
#include <functional>
#include <memory>
#include <string>
#include "core/types.h"

namespace ui {
class Canvas;
struct Element;

struct MouseEvent {
    float x = 0, y = 0;           // document coords
    int button = 0;               // 0 left, 1 right
    bool down = false, up = false, move = false;
    bool shift = false, ctrl = false;
    int wheelDelta = 0;
};
struct KeyEvent {
    UINT vk = 0;
    bool shift = false, ctrl = false;
};

// Base interface for stateful form controls attached to an Element.
struct Control {
    virtual ~Control() = default;

    // Draw control-specific overlay (text, caret, selection, thumb) over the element box.
    virtual void paint(Canvas& c, Element& el, float scale) = 0;

    // Mouse event. Out: true if consumed (and repaint needed).
    virtual bool onMouse(Element& el, const MouseEvent& e) { return false; }

    // Key down. Out: true if consumed.
    virtual bool onKey(Element& el, const KeyEvent& e) { return false; }

    // Character typed (after WM_CHAR / IME commit). Out: true if consumed.
    virtual bool onChar(Element& el, wchar_t ch) { return false; }

    // Focus changes. Controls start/stop caret blink state.
    virtual void onFocus(Element& el, bool focused) {}

    // Intrinsic content size used when width/height are auto. Out: size in px.
    virtual SizeF intrinsicSize(Element& el, float scale) { return {}; }

    // Cursor to show while hovering. Out: kind.
    virtual CursorKind cursor() const { return CursorKind::Arrow; }

    // Whether the control wants keyboard focus (Tab order).
    virtual bool focusable() const { return true; }

    // Toggle caret blink visibility on timer tick. Out: true if repaint needed.
    virtual bool onBlinkTick() { return false; }
};

// Create the right Control for an element (input/textarea/range/button) based on tag + type.
// In: element. Out: new control or nullptr for non-controls.
std::unique_ptr<Control> createControlFor(Element& el);

} // namespace ui
