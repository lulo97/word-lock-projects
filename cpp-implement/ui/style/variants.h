#pragma once
#include <string>
#include <vector>
#include "style/style.h"

namespace ui {

enum class Variant { None, Hover, Focus, Active, Disabled, Sm, Md, Lg };

// A class with its variant stripped: "hover:bg-gray-100" -> {Hover, "bg-gray-100"}.
struct VariantClass {
    Variant variant = Variant::None;
    std::wstring base;
};

// Split a class token on ':' (supports stacked prefixes by keeping the first known one).
VariantClass splitVariant(const std::wstring& token);

// Declarations grouped per variant for one element.
struct VariantSet {
    Declarations base;
    Declarations hover, focus, active, disabled;
    Declarations sm, md, lg;
};

// Build a VariantSet from a class list (uses class_table + class_rules).
VariantSet buildVariantSet(const std::vector<std::wstring>& classList);

// Is a variant active now? In: variant, stateFlags, viewportWidth. Out: bool.
bool isVariantActive(Variant v, uint32_t stateFlags, int viewportWidth);

} // namespace ui
