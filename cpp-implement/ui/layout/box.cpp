#include "layout/box.h"

#include <algorithm>

namespace ui {

RectF LayoutBox::paddingRect() const {
    return {
        borderRect.x + border.left,
        borderRect.y + border.top,
        std::max(0.f, borderRect.w - border.left - border.right),
        std::max(0.f, borderRect.h - border.top - border.bottom)
    };
}

RectF LayoutBox::contentRect() const {
    RectF p = paddingRect();
    return {
        p.x + padding.left,
        p.y + padding.top,
        std::max(0.f, p.w - padding.left - padding.right),
        std::max(0.f, p.h - padding.top - padding.bottom)
    };
}

float LayoutBox::clientHeight() const {
    return paddingRect().h;
}

float LayoutBox::maxScrollY() const {
    return std::max(0.f, contentHeight - clientHeight());
}

} // namespace ui
