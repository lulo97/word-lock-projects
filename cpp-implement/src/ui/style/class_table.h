#pragma once
#include <string>
#include "style/style.h"

namespace ui {

// Exact-match Tailwind classes (flex, hidden, italic, text-xs, font-bold, rounded-lg...).
// In: class (no variant prefix). Out: true + declarations appended to out.
bool lookupStaticClass(const std::wstring& cls, Declarations& out);

// Classes that are intentionally ignored (transition-*, select-none, antialiased...).
bool isIgnoredClass(const std::wstring& cls);

} // namespace ui
