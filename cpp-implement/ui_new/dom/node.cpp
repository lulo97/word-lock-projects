#include "dom/node.h"

namespace ui {

Element::~Element() = default;

std::wstring Element::attr(const std::wstring& name) const {
    for (const auto& entry : attrs) {
        if (entry.first == name)
            return entry.second;
    }

    return L"";
}

bool Element::hasAttr(const std::wstring& name) const {
    for (const auto& entry : attrs) {
        if (entry.first == name)
            return true;
    }

    return false;
}

} // namespace ui
