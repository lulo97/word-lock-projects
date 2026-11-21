#pragma once
#include "controls/control.h"

namespace ui {

// Optional real checkbox control (settings screen can also fake it with a styled div).
class Checkbox : public Control {
public:
    bool checked = false;
    std::function<void(bool)> onChange;

    void init(Element& el);
    void paint(Canvas& c, Element& el, float scale) override;  // box + check mark path
    bool onMouse(Element& el, const MouseEvent& e) override;   // toggle on click
    bool onKey(Element& el, const KeyEvent& e) override;       // Space toggles
    CursorKind cursor() const override { return CursorKind::Hand; }
    void setChecked(bool v);
};

} // namespace ui
