#include "controls/range_input.h"

#include "core/utf.h"
#include "dom/node.h"
#include "paint/canvas.h"

#include <algorithm>
#include <cmath>

namespace ui {

void RangeInput::init(Element& el) {
    float parsed = 0;

    if (parseFloat(el.attr(L"min"), parsed))
        minValue = parsed;

    if (parseFloat(el.attr(L"max"), parsed))
        maxValue = parsed;

    if (parseFloat(el.attr(L"value"), parsed))
        value = parsed;

    if (parseFloat(el.attr(L"step"), parsed) && parsed > 0)
        step = parsed;

    value = std::max(minValue, std::min(maxValue, value));
}

void RangeInput::paint(Canvas& canvas, Element& el, float scale) {
    RectF r = el.box.contentRect();
    float trackY = r.y + r.h / 2;
    float thumbRadius = 8 * scale;
    float trackW = std::max(0.f, r.w - thumbRadius * 2);
    float t = maxValue > minValue
        ? (value - minValue) / (maxValue - minValue)
        : 0.f;
    t = std::max(0.f, std::min(1.f, t));

    float left = r.x + thumbRadius;
    float right = left + trackW;
    float thumbX = left + trackW * t;

    canvas.drawLine(
        { left, trackY }, { right, trackY },
        std::max(2.f, 4.f * scale), trackColor
    );
    canvas.drawLine(
        { left, trackY }, { thumbX, trackY },
        std::max(2.f, 4.f * scale), fillColor
    );

    canvas.fillEllipse(
        { thumbX - thumbRadius, trackY - thumbRadius,
          thumbRadius * 2, thumbRadius * 2 },
        thumbColor
    );
}

bool RangeInput::onMouse(Element& el, const MouseEvent& e) {
    if (e.button != 0)
        return false;

    if (e.down) {
        dragging = true;
        setValue(valueFromX(e.x - el.box.contentRect().x, el.box.contentRect().w));
        return true;
    }

    if (e.up) {
        bool wasDragging = dragging;
        dragging = false;
        return wasDragging;
    }

    if (e.move && dragging) {
        setValue(valueFromX(e.x - el.box.contentRect().x, el.box.contentRect().w));
        return true;
    }

    return false;
}

bool RangeInput::onKey(Element& el, const KeyEvent& e) {
    (void)el;

    if (e.vk == VK_LEFT || e.vk == VK_DOWN) {
        setValue(value - step);
        return true;
    }

    if (e.vk == VK_RIGHT || e.vk == VK_UP) {
        setValue(value + step);
        return true;
    }

    if (e.vk == VK_HOME) {
        setValue(minValue);
        return true;
    }

    if (e.vk == VK_END) {
        setValue(maxValue);
        return true;
    }

    return false;
}

SizeF RangeInput::intrinsicSize(Element& el, float scale) {
    (void)el;
    return { 160 * scale, 16 * scale };
}

void RangeInput::setValue(float v) {
    v = std::max(minValue, std::min(maxValue, v));

    if (step > 0)
        v = minValue + std::round((v - minValue) / step) * step;

    v = std::max(minValue, std::min(maxValue, v));

    if (std::fabs(v - value) < 0.0001f)
        return;

    value = v;

    if (onChange)
        onChange(value);
}

float RangeInput::valueFromX(float x, float trackW) const {
    float thumbRadius = 8.f;
    float usable = std::max(1.f, trackW - thumbRadius * 2);
    float t = (x - thumbRadius) / usable;
    t = std::max(0.f, std::min(1.f, t));
    return minValue + (maxValue - minValue) * t;
}

} // namespace ui
