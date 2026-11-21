#include "style/style.h"

namespace ui {

void applyDeclaration(ComputedStyle& style, const Declaration& d) {
    switch (d.prop) {
    case Prop::Display:        style.display = static_cast<Display>(static_cast<int>(d.number)); break;
    case Prop::Position:       style.position = static_cast<Position>(static_cast<int>(d.number)); break;
    case Prop::Left:           style.left = d.length; break;
    case Prop::Right:          style.right = d.length; break;
    case Prop::Top:            style.top = d.length; break;
    case Prop::Bottom:         style.bottom = d.length; break;
    case Prop::ZIndex:         style.zIndex = static_cast<int>(d.number); break;
    case Prop::TranslateX:     style.translateXPct = d.number; break;
    case Prop::TranslateY:     style.translateYPct = d.number; break;

    case Prop::Width:          style.width = d.length; break;
    case Prop::Height:         style.height = d.length; break;
    case Prop::MinWidth:       style.minWidth = d.length; break;
    case Prop::MinHeight:      style.minHeight = d.length; break;
    case Prop::MaxWidth:       style.maxWidth = d.length; break;
    case Prop::MaxHeight:      style.maxHeight = d.length; break;

    case Prop::MarginT:        style.margin.top = d.number; break;
    case Prop::MarginR:        style.margin.right = d.number; break;
    case Prop::MarginB:        style.margin.bottom = d.number; break;
    case Prop::MarginL:        style.margin.left = d.number; break;
    case Prop::PaddingT:       style.padding.top = d.number; break;
    case Prop::PaddingR:       style.padding.right = d.number; break;
    case Prop::PaddingB:       style.padding.bottom = d.number; break;
    case Prop::PaddingL:       style.padding.left = d.number; break;
    case Prop::BorderWidthT:   style.borderWidth.top = d.number; break;
    case Prop::BorderWidthR:   style.borderWidth.right = d.number; break;
    case Prop::BorderWidthB:   style.borderWidth.bottom = d.number; break;
    case Prop::BorderWidthL:   style.borderWidth.left = d.number; break;
    case Prop::BorderColor:
        style.borderColor[0] = d.color;
        style.borderColor[1] = d.color;
        style.borderColor[2] = d.color;
        style.borderColor[3] = d.color;
        break;

    case Prop::RadiusTL:       style.radius.tl = d.number; break;
    case Prop::RadiusTR:       style.radius.tr = d.number; break;
    case Prop::RadiusBR:       style.radius.br = d.number; break;
    case Prop::RadiusBL:       style.radius.bl = d.number; break;

    case Prop::SpaceX:         style.spaceX = d.number; break;
    case Prop::SpaceY:         style.spaceY = d.number; break;

    case Prop::FlexDir:        style.dir = static_cast<FlexDirection>(static_cast<int>(d.number)); break;
    case Prop::Justify:        style.justify = static_cast<JustifyContent>(static_cast<int>(d.number)); break;
    case Prop::Align:          style.align = static_cast<AlignItems>(static_cast<int>(d.number)); break;
    case Prop::Grow:           style.grow = d.number; break;
    case Prop::Shrink:         style.shrink = d.number; break;
    case Prop::GapX:           style.gapX = d.number; break;
    case Prop::GapY:           style.gapY = d.number; break;

    case Prop::Background:     style.background = d.color; break;
    case Prop::Shadow:         style.shadows = d.shadows; break;
    case Prop::Opacity:        style.opacity = d.number; break;
    case Prop::OverflowX:      style.overflowX = static_cast<Overflow>(static_cast<int>(d.number)); break;
    case Prop::OverflowY:      style.overflowY = static_cast<Overflow>(static_cast<int>(d.number)); break;
    case Prop::PointerEvents:  style.pointerEvents = d.number != 0; break;
    case Prop::Cursor:         style.cursor = static_cast<CursorKind>(static_cast<int>(d.number)); break;

    case Prop::Color:          style.color = d.color; break;
    case Prop::FontSize:       style.fontSize = d.number; break;
    case Prop::FontWeight:     style.fontWeight = static_cast<int>(d.number); break;
    case Prop::Italic:         style.italic = d.number != 0; break;
    case Prop::FontFamily:     style.fontFamily = d.text; break;
    case Prop::LetterSpacing:  style.letterSpacing = d.number; break;
    case Prop::LineHeight:
        style.lineHeight = d.number;
        style.lineHeightIsPx = d.text.empty();
        break;
    case Prop::TextAlign:      style.textAlign = static_cast<TextAlign>(static_cast<int>(d.number)); break;
    case Prop::TextTransform:  style.transform = static_cast<TextTransform>(static_cast<int>(d.number)); break;

    default:
        break;
    }
}

} // namespace ui
