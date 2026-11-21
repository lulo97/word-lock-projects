#pragma once
#include <string>
#include <vector>
#include "core/types.h"

namespace ui {

struct Shadow {
    float offsetX = 0, offsetY = 0, blur = 0, spread = 0;
    Color color = Color::rgba(0, 0, 0, 51);
    bool inset = false;
};

// Final style of one element after cascade, variants, inheritance and preflight.
struct ComputedStyle {
    // display / position
    Display display = Display::Block;
    Position position = Position::Static;
    Length left, right, top, bottom;
    int zIndex = 0;
    float translateXPct = 0, translateYPct = 0;   // -translate-y-1/2 => -50

    // box
    Length width, height, minWidth, minHeight, maxWidth, maxHeight;
    Edges margin, padding, borderWidth;
    Color borderColor[4];                         // top,right,bottom,left
    CornerRadii radius;
    float spaceY = 0, spaceX = 0;                 // space-y-N (applied to children by layout)

    // flex
    FlexDirection dir = FlexDirection::Row;
    JustifyContent justify = JustifyContent::Start;
    AlignItems align = AlignItems::Stretch;
    float grow = 0, shrink = 1;
    float gapX = 0, gapY = 0;

    // paint
    Color background = Color::transparent();
    std::vector<Shadow> shadows;
    float opacity = 1;
    Overflow overflowX = Overflow::Visible, overflowY = Overflow::Visible;
    bool pointerEvents = true;
    CursorKind cursor = CursorKind::Arrow;

    // inherited
    Color color = Color::rgb(0, 0, 0);
    float fontSize = 16;
    int fontWeight = 400;
    bool italic = false;
    std::wstring fontFamily = L"Segoe UI";
    float letterSpacing = 0;                      // px
    float lineHeight = 1.5f;                      // multiplier, or px if lineHeightIsPx
    bool lineHeightIsPx = false;
    TextAlign textAlign = TextAlign::Left;
    TextTransform transform = TextTransform::None;
};

// Properties that a class declaration can set. One enum value per ComputedStyle field group.
enum class Prop {
    Display, Position, Left, Right, Top, Bottom, ZIndex, TranslateX, TranslateY,
    Width, Height, MinWidth, MinHeight, MaxWidth, MaxHeight,
    MarginT, MarginR, MarginB, MarginL, PaddingT, PaddingR, PaddingB, PaddingL,
    BorderWidthT, BorderWidthR, BorderWidthB, BorderWidthL, BorderColor,
    RadiusTL, RadiusTR, RadiusBR, RadiusBL,
    SpaceX, SpaceY, FlexDir, Justify, Align, Grow, Shrink, GapX, GapY,
    Background, Shadow, Opacity, OverflowX, OverflowY, PointerEvents, Cursor,
    Color, FontSize, FontWeight, Italic, FontFamily, LetterSpacing, LineHeight,
    TextAlign, TextTransform
};

// One atomic style change produced by a class. Value is a tagged union kept simple.
struct Declaration {
    Prop prop;
    float number = 0;           // px / multiplier / enum index
    Length length;              // for Length props
    Color color;                // for color props
    std::wstring text;          // font family, etc
    std::vector<Shadow> shadows;
};
using Declarations = std::vector<Declaration>;

// Apply one declaration onto a style. In/Out: style (modified).
void applyDeclaration(ComputedStyle& style, const Declaration& d);

} // namespace ui
