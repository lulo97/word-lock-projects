#include "layout/inline_text.h"

#include "core/utf.h"
#include "layout/layout_units.h"
#include "paint/text_draw.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>

namespace ui {

namespace {

// true: center the em box (fontPx tall) in the line box, so glyphs stay inside
// a box whose line-height >= font size. false: center GDI's whole cell.
constexpr bool kCenterEmBox = true;

std::map<const Element*, std::vector<LineBox>> g_lines;

void collectFrom(Element& el, const ComputedStyle& textStyle, float scale,
                 std::vector<TextRun>& runs) {
    for (auto& childPtr : el.children) {
        if (childPtr->kind == Node::Kind::Text) {
            std::wstring text = collapseWhitespace(
                static_cast<TextNode*>(childPtr.get())->text
            );
            if (text.empty())
                continue;
            runs.push_back(makeRun(text, textStyle, scale));
            continue;
        }

        Element* child = static_cast<Element*>(childPtr.get());
        if (child->style.display == Display::None ||
            child->style.display == Display::Block ||
            child->style.display == Display::Flex ||
            child->style.display == Display::InlineBlock ||
            child->style.position == Position::Absolute)
            continue;

        collectFrom(*child, child->style, scale, runs);
    }
}

// Distance from line top to baseline (a) and baseline to line bottom (d).
void lineContribution(const TextRun& r, float& a, float& d) {
    const float top = kCenterEmBox ? (r.ascent - r.internalLeading) : r.ascent;
    const float bottom = r.descent;
    const float half = (r.lineHeightPx - (top + bottom)) * 0.5f;
    a = top + half;
    d = bottom + half;
}

void finishLine(std::vector<LineBox>& lines, LineBox& line) {
    if (line.runs.empty())
        return;

    float ascent = 0, descent = 0;
    line.width = 0;
    for (const TextRun& r : line.runs) {
        float a, d;
        lineContribution(r, a, d);
        ascent = std::max(ascent, a);
        descent = std::max(descent, d);
        line.width = std::max(line.width, r.x + r.width);
    }
    line.baseline = std::round(ascent);
    line.height = ascent + descent;
    if (line.height <= 0) line.height = 16;
    lines.push_back(line);
    line = LineBox{};
}

struct Token {
    TextRun run;
    bool newline = false;
    bool spaceBefore = false;
    float spaceWidth = 0;
    bool forbiddenStart = false;
};

std::vector<Token> tokenize(const std::vector<TextRun>& runs) {
    std::vector<Token> out;
    bool space = false;
    float spaceW = 0;

    for (const TextRun& r : runs) {
        const std::wstring& t = r.text;
        size_t i = 0;
        while (i < t.size()) {
            const wchar_t c = t[i];
            if (c == L'\n') {
                Token n; n.newline = true;
                out.push_back(n);
                space = false;
                ++i;
                continue;
            }
            if (iswspace(c)) {
                space = true;
                spaceW = sliceRun(r, L" ").width;
                ++i;
                continue;
            }

            size_t end = i + 1;
            if (!isCjk(c)) {
                while (end < t.size() && !iswspace(t[end]) && !isCjk(t[end]))
                    ++end;
            }

            Token tok;
            tok.run = sliceRun(r, t.substr(i, end - i));
            tok.spaceBefore = space;
            tok.spaceWidth = spaceW;
            tok.forbiddenStart = isLineStartForbidden(c);
            out.push_back(std::move(tok));
            space = false;
            i = end;
        }
    }
    return out;
}

} // namespace

std::vector<TextRun> collectInlineRuns(Element& el, float scale) {
    std::vector<TextRun> runs;
    collectFrom(el, el.style, scale, runs);
    return runs;
}

std::vector<LineBox> breakLines(const std::vector<TextRun>& runs, float maxWidth, float /*scale*/) {
    const bool wrap = maxWidth > 0;
    std::vector<LineBox> lines;
    LineBox line;
    float x = 0;
    auto flush = [&] { finishLine(lines, line); x = 0; };

    for (Token& tok : tokenize(runs)) {
        if (tok.newline) { flush(); continue; }

        bool empty = line.runs.empty();
        float gap = (!empty && tok.spaceBefore) ? tok.spaceWidth : 0.f;
        const float w = tok.run.width;
        const bool fits = !wrap || fitsIn(x + gap + w, maxWidth);

        if (!fits && !empty && !tok.forbiddenStart) {
            flush();
            empty = true;
            gap = 0;
        }

        if (wrap && empty && !fitsIn(w, maxWidth)) {
            // Overlong single word: split by character.
            const std::wstring& s = tok.run.text;
            size_t start = 0;
            float acc = 0;
            auto emit = [&](size_t from, size_t to) {
                TextRun seg = sliceRun(tok.run, s.substr(from, to - from));
                seg.x = 0;
                line.runs.push_back(seg);
                x = seg.width;
            };
            for (size_t k = 0; k < s.size(); ++k) {
                float adv = sliceRun(tok.run, s.substr(k, 1)).width;
                if (k > start && !fitsIn(acc + adv, maxWidth)) {
                    emit(start, k);
                    flush();
                    start = k;
                    acc = 0;
                }
                acc += adv;
            }
            emit(start, s.size());
            continue;
        }

        if (!fits && !empty)
            tok.run.hanging = true;

        x += gap;
        tok.run.x = x;
        line.runs.push_back(tok.run);
        x += w;
    }

    flush();

    if (lines.empty()) {
        LineBox l;
        l.height = 16;
        lines.push_back(l);
    }
    return lines;
}

InlineExtent measureInline(Element& el, float scale) {
    std::vector<TextRun> runs = collectInlineRuns(el, scale);
    InlineExtent e;
    for (const Token& t : tokenize(runs)) {
        if (!t.newline)
            e.minContent = std::max(e.minContent, t.run.width);
    }
    for (const LineBox& l : breakLines(runs, 0.f, scale))
        e.maxContent = std::max(e.maxContent, l.width);
    return e;
}

void alignLines(std::vector<LineBox>& lines, const ComputedStyle& style, float contentWidth) {
    for (LineBox& line : lines) {
        float offset = 0;
        if (style.textAlign == TextAlign::Center)
            offset = (contentWidth - line.width) / 2;
        else if (style.textAlign == TextAlign::Right)
            offset = contentWidth - line.width;
        offset = std::max(0.f, offset);
        for (TextRun& run : line.runs)
            run.x += offset;
    }
}

float layoutInline(Element& el, float contentX, float contentY, float innerW, float scale) {
    std::vector<TextRun> runs = collectInlineRuns(el, scale);
    std::vector<LineBox> lines = breakLines(runs, innerW, scale);
    alignLines(lines, el.style, innerW);

    float y = contentY;
    for (LineBox& line : lines) {
        line.y = y;
        for (TextRun& run : line.runs) {
            run.x += contentX;
            run.y = y + line.baseline - run.ascent; // baseline alignment
        }
        y += line.height;
    }

    g_lines[&el] = lines;
    return y - contentY;
}

bool hasInlineContent(const Element& el) {
    if (el.tag == L"input" || el.tag == L"textarea" ||
        el.tag == L"svg" || (el.tag == L"div" && el.style.display == Display::Flex))
        return false;

    if (el.children.empty())
        return false;

    bool hasInline = false;
    for (const auto& childPtr : el.children) {
        if (childPtr->kind == Node::Kind::Text) {
            const std::wstring& text = static_cast<const TextNode*>(childPtr.get())->text;
            if (!trim(text).empty())
                hasInline = true;
            continue;
        }

        const Element* child = static_cast<const Element*>(childPtr.get());
        if (child->style.position == Position::Absolute ||
            child->style.display == Display::None)
            continue;
        // Atomic/replaced elements have their own box; they can't be flattened into runs.
        if (child->tag == L"svg" || child->tag == L"img" ||
            child->tag == L"input" || child->tag == L"textarea")
            return false;
        if (child->style.display == Display::Block ||
            child->style.display == Display::Flex ||
            child->style.display == Display::InlineBlock)
            return false;
        hasInline = true;
    }
    return hasInline;
}

const std::vector<LineBox>& getLines(const Element& el) {
    static const std::vector<LineBox> empty;
    auto it = g_lines.find(&el);
    return it == g_lines.end() ? empty : it->second;
}

void translateLines(Element& el, float dx, float dy) {
    auto it = g_lines.find(&el);
    if (it == g_lines.end())
        return;
    for (LineBox& line : it->second) {
        line.y += dy;
        for (TextRun& run : line.runs) {
            run.x += dx;
            run.y += dy;
        }
    }
}

void clearLineCache() {
    g_lines.clear();
}

void eraseLines(const Element& el) {
    g_lines.erase(&el);
}

} // namespace ui
