#pragma once
#include "controls/control.h"

namespace ui {

// <input type="range">. Hardcoded look: 4px track, filled part, 16px circular thumb.
class RangeInput : public Control {
public:
    float minValue = 0, maxValue = 100, value = 50, step = 1;
    bool dragging = false;
    Color trackColor = Color::rgb(0x1e, 0x29, 0x3b);
    Color fillColor  = Color::rgb(0x3b, 0x82, 0xf6);
    Color thumbColor = Color::rgb(0x60, 0xa5, 0xfa);
    std::function<void(float)> onChange;

    void init(Element& el);                                   // read min/max/value/step attrs
    void paint(Canvas& c, Element& el, float scale) override;
    bool onMouse(Element& el, const MouseEvent& e) override; // click/drag sets value
    bool onKey(Element& el, const KeyEvent& e) override;      // arrows
    SizeF intrinsicSize(Element& el, float scale) override;
    CursorKind cursor() const override { return CursorKind::Hand; }

    // Set value clamped to range and step. Fires onChange.
    void setValue(float v);
    // Map mouse x to value. In: x relative to track, track width. Out: value.
    float valueFromX(float x, float trackW) const;
};

} // namespace ui
