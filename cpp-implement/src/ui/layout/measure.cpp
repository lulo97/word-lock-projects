#include "layout/measure.h"

#include "core/utf.h"
#include "dom/tree_ops.h"
#include "paint/text_draw.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ui {

namespace {

std::map<Element*, Measure> g_cache;

float horizontalBox(const ComputedStyle& style, float scale) {
    return (style.padding.left + style.padding.right +
            style.borderWidth.left + style.borderWidth.right) * scale;
}

Measure textMeasure(const std::wstring& rawText, const ComputedStyle& style, float scale) {
    Measure out;
    std::wstring text = trim(collapseWhitespace(rawText));
    TextRun run = makeRun(text, style, scale);
    measureRun(run);
    out.maxContent = run.width;

    float longest = 0;
    size_t start = 0;

    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || iswspace(text[i])) {
            if (i > start) {
                TextRun word = makeRun(text.substr(start, i - start), style, scale);
                measureRun(word);
                longest = std::max(longest, word.width);
            }

            start = i + 1;
        }
    }

    out.minContent = longest;
    return out;
}

} // namespace

Measure measureElement(Element& el, float scale) {
    auto cached = g_cache.find(&el);

    if (cached != g_cache.end())
        return cached->second;

    Measure result;

    if (el.style.display == Display::None) {
        g_cache[&el] = result;
        return result;
    }

    if (!el.style.width.isAuto()) {
        if (el.style.width.unit == Length::Unit::Px ||
            el.style.width.unit == Length::Unit::Screen) {
            float width = el.style.width.value * scale;
            result.minContent = width;
            result.maxContent = width;
            g_cache[&el] = result;
            return result;
        }
    }
    else if (el.control) {
        SizeF intrinsic = el.control->intrinsicSize(el, scale);
        result.minContent = std::max(result.minContent, intrinsic.w);
        result.maxContent = std::max(result.maxContent, intrinsic.w);
    }

    if (!el.children.empty()) {
        bool onlyText = true;

        for (const auto& child : el.children) {
            if (child->kind != Node::Kind::Text) {
                onlyText = false;
                break;
            }
        }

        if (onlyText) {
            Measure text = textMeasure(textContent(el), el.style, scale);

            if (el.style.width.isAuto()) {
                result.minContent = std::max(result.minContent, text.minContent);
                result.maxContent = std::max(result.maxContent, text.maxContent);
            }
        }
        else {
            for (auto& childPtr : el.children) {
                Element* child = dynamic_cast<Element*>(childPtr.get());

                if (!child || child->style.display == Display::None ||
                    child->style.position == Position::Absolute)
                    continue;

                Measure childMeasure = measureElement(*child, scale);
                bool fixedBox = !child->style.width.isAuto() &&
                    (child->style.width.unit == Length::Unit::Px ||
                     child->style.width.unit == Length::Unit::Screen);
                // A child's margin is part of its outer contribution to the
                // parent's intrinsic width. Omitting it (e.g. a block with
                // ml-4 under a shrink-to-fit flex item) makes the parent a few
                // pixels too narrow, so its text wraps early.
                float marginH = (child->style.margin.left + child->style.margin.right) * scale;
                float extra = marginH + (fixedBox ? 0.f : horizontalBox(child->style, scale));

                if (el.style.display == Display::Flex &&
                    el.style.dir == FlexDirection::Row) {
                    result.maxContent += childMeasure.maxContent + extra;
                    result.minContent += childMeasure.minContent + extra;
                }
                else {
                    result.minContent = std::max(result.minContent, childMeasure.minContent + extra);
                    result.maxContent = std::max(result.maxContent, childMeasure.maxContent + extra);
                }
            }

            if (el.style.display == Display::Flex &&
                el.style.dir == FlexDirection::Row &&
                el.children.size() > 1) {
                float gap = el.style.gapX * scale * (el.children.size() - 1);
                result.maxContent += gap;
                result.minContent += gap;
            }
        }
    }

    result.minContent = std::max(0.f, result.minContent);
    result.maxContent = std::max(result.minContent, result.maxContent);

    g_cache[&el] = result;
    return result;
}

float measureHeightForWidth(Element& el, float width, float scale) {
    if (!el.style.height.isAuto())
        return el.style.height.resolve(width, width) * scale;

    Measure text = measureElement(el, scale);

    if (text.maxContent > 0) {
        float lines = std::max(1.f, std::ceil(text.maxContent / std::max(1.f, width * 0.55f)));
        float lineHeight = el.style.lineHeightIsPx
            ? el.style.lineHeight * scale
            : el.style.fontSize * scale * el.style.lineHeight;
        return lines * lineHeight;
    }

    return 0;
}

SizeF measureText(const std::wstring& text, const ComputedStyle& style, float scale) {
    TextRun run = makeRun(text, style, scale);
    measureRun(run);
    return { run.width, run.height };
}

void clearMeasureCache() {
    g_cache.clear();
}

} // namespace ui
