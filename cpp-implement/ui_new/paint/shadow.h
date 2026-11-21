#pragma once
#include "paint/canvas.h"
#include "style/style.h"

namespace ui {

// Approximate a box-shadow by drawing several expanding low-alpha rounded rects.
// In: canvas, element border rect, radii, shadow definition, scale. Out: pixels outside border box.
void drawShadow(Canvas& canvas, const RectF& rect, const CornerRadii& radii, const Shadow& s, float scale);

// Presets for Tailwind names. In: name ("sm","md","lg","xl","2xl"). Out: shadow list.
std::vector<Shadow> shadowPreset(const std::wstring& name);

// Parse arbitrary "0_4px_16px_rgba(0,0,0,0.2)". In: inner text. Out: true + shadow.
bool parseShadowArbitrary(const std::wstring& inner, Shadow& out);

} // namespace ui
