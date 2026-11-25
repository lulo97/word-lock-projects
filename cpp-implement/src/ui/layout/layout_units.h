#pragma once
#include <cmath>

namespace ui {

// Layout compares sizes on a 1/64 px grid. Absorbs float round-trip noise only,
// not measurement error.
constexpr float kLayoutUnitsPerPx = 64.f;

inline int toUnits(float px) { return static_cast<int>(std::lround(px * kLayoutUnitsPerPx)); }
inline bool fitsIn(float used, float avail) { return toUnits(used) <= toUnits(avail); }

} // namespace ui
