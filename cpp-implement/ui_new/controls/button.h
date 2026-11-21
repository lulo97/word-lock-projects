#pragma once
#include "controls/control.h"

namespace ui {

// <button>: hover/active/focus states and click. Visuals come from classes, text from children.
class Button : public Control {
public:
    std::function<void()> onClick;
    bool pressed = false;

    void paint(Canvas& c, Element& el, float scale) override;  // focus ring only
    bool onMouse(Element& el, const MouseEvent& e) override;   // press/release => onClick
    bool onKey(Element& el, const KeyEvent& e) override;       // Enter/Space => onClick
    CursorKind cursor() const override { return CursorKind::Hand; }
};

} // namespace ui
