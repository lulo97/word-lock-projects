#include "controls/button.h"

#include "dom/node.h"
#include "paint/canvas.h"

namespace ui {

void Button::paint(Canvas& canvas, Element& el, float scale) {
    if (!(el.stateFlags & StateFocus))
        return;

    Color ring = Color::rgba(96, 165, 250, 180);
    Edges widths { scale, scale, scale, scale };
    Color colors[4] = { ring, ring, ring, ring };
    canvas.strokeBorders(el.box.borderRect, {}, widths, colors);
}

bool Button::onMouse(Element& el, const MouseEvent& e) {
    (void)el;

    if (e.button != 0)
        return false;

    if (e.down) {
        pressed = true;
        return true;
    }

    if (e.up) {
        pressed = false;
        return true;
    }

    return false;
}

bool Button::onKey(Element& el, const KeyEvent& e) {
    (void)el;

    if (e.vk == VK_RETURN || e.vk == VK_SPACE) {
        if (onClick)
            onClick();

        return true;
    }

    return false;
}

} // namespace ui
