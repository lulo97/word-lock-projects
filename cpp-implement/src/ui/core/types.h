#pragma once
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {

// RGBA color, 0..255 per channel. a=255 is opaque.
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    static Color rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }
    static Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return {r, g, b, a}; }
    static Color transparent() { return {0, 0, 0, 0}; }
    COLORREF toColorRef() const { return RGB(r, g, b); }
};

// Float rectangle (x,y,w,h). Used by layout and paint.
struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct PointF { float x = 0, y = 0; };
struct SizeF  { float w = 0, h = 0; };

// Four sides (margin, padding, border width).
struct Edges { float top = 0, right = 0, bottom = 0, left = 0; };

// Per-corner radius (supports rounded-l-lg etc).
struct CornerRadii { float tl = 0, tr = 0, br = 0, bl = 0; };

// A CSS length: auto, px, or percent of containing block.
struct Length {
    enum class Unit { Auto, Px, Percent, Screen } unit = Unit::Auto;
    float value = 0;
    static Length px(float v) { return {Unit::Px, v}; }
    static Length percent(float v) { return {Unit::Percent, v}; }
    bool isAuto() const { return unit == Unit::Auto; }
    // Resolve to pixels. Input: basis (containing size, for percent/screen). Output: px, or fallback if auto.
    float resolve(float basis, float fallback) const;
};

enum class Display { Block, Flex, Inline, InlineBlock, None };
enum class Position { Static, Relative, Absolute };
enum class FlexDirection { Row, Column };
enum class JustifyContent { Start, Center, End, Between, Around, Evenly };
enum class AlignItems { Stretch, Start, Center, End };
enum class TextAlign { Left, Center, Right };
enum class TextTransform { None, Uppercase, Lowercase };
enum class Overflow { Visible, Hidden, Auto };
enum class CursorKind { Arrow, Hand, IBeam };

// Element state bit flags used by variants (hover:, focus:).
enum StateFlag : uint32_t { StateHover = 1, StateFocus = 2, StateActive = 4, StateDisabled = 8 };

} // namespace ui
