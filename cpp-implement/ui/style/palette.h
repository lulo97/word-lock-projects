#pragma once
#include <string>
#include "core/types.h"

namespace ui {

// Look up a Tailwind palette color.
// In: name like "slate-400", "white", "transparent". Out: true + color on hit.
bool lookupPaletteColor(const std::wstring& name, Color& out);

// Parse "#rgb", "#rrggbb", "#rrggbbaa", "rgb(...)", "rgba(...)".
// In: css color text. Out: true + color on success.
bool parseCssColor(const std::wstring& text, Color& out);

// Parse a color token that may end with "/NN" opacity.
// In: "gray-500/30" or "[#1e293b]". Out: true + color with alpha applied.
bool parseColorToken(const std::wstring& token, Color& out);

} // namespace ui
