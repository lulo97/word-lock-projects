#include "core/types.h"

namespace ui {

float Length::resolve(float basis, float fallback) const {
    switch (unit) {
    case Unit::Px:
        return value;
    case Unit::Percent:
        return basis * value / 100.0f;
    case Unit::Screen:
        return basis * value / 100.0f;
    case Unit::Auto:
    default:
        return fallback;
    }
}

} // namespace ui
