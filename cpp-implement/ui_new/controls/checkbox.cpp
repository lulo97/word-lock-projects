#include "controls/checkbox.h"

#include "dom/node.h"
#include "paint/canvas.h"

namespace ui {

void Checkbox::init(Element& el) {
    checked = el.hasAttr(L"checked") || el.attr(L"checked") == L"true";
}

void Checkbox::paint(Canvas& canvas, Element& el, float scale) {
    (void)scale;

    RectF r = el.box.contentRect();
    float size = std::min(r.w, r.h);

    if (size <= 0)
        return;

    RectF box { r.x, r.y + (r.h - size) / 2, size, size };
    Color fill = checked
        ? Color::rgb(59, 130, 246)
        : Color::rgb(255, 255, 255);
    Color border = Color::rgb(107, 114, 128);

    canvas.fillRoundRect(box, { 3, 3, 3, 3 }, fill);
    Edges widths { 1, 1, 1, 1 };
    Color colors[4] = { border, border, border, border };
    canvas.strokeBorders(box, { 3, 3, 3, 3 }, widths, colors);

    if (checked) {
        canvas.drawLine(
            { box.x + size * 0.22f, box.y + size * 0.52f },
            { box.x + size * 0.42f, box.y + size * 0.72f },
            std::max(1.f, size * 0.10f),
            Color::rgb(255, 255, 255)
        );
        canvas.drawLine(
            { box.x + size * 0.42f, box.y + size * 0.72f },
            { box.x + size * 0.78f, box.y + size * 0.28f },
            std::max(1.f, size * 0.10f),
            Color::rgb(255, 255, 255)
        );
    }
}

bool Checkbox::onMouse(Element& el, const MouseEvent& e) {
    (void)el;

    if (e.button == 0 && e.up) {
        setChecked(!checked);
        return true;
    }

    return false;
}

bool Checkbox::onKey(Element& el, const KeyEvent& e) {
    (void)el;

    if (e.vk == VK_SPACE) {
        setChecked(!checked);
        return true;
    }

    return false;
}

void Checkbox::setChecked(bool value) {
    if (checked == value)
        return;

    checked = value;

    if (onChange)
        onChange(checked);
}

} // namespace ui
