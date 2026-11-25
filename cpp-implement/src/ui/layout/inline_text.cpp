#include "layout/inline_text.h"

#include "core/utf.h"
#include "paint/text_draw.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <map>

namespace ui {

namespace {

std::map<const Element*, std::vector<LineBox>> g_lines;

void collectFrom(Element& el, const ComputedStyle& textStyle, std::vector<TextRun>& runs) {
    for (auto& childPtr : el.children) {
        if (childPtr->kind == Node::Kind::Text) {
            std::wstring text = collapseWhitespace(
                static_cast<TextNode*>(childPtr.get())->text
            );

            if (text.empty())
                continue;

            runs.push_back(makeRun(text, textStyle, 1.f));
            continue;
        }

        Element* child = static_cast<Element*>(childPtr.get());

        if (child->style.display == Display::None ||
            child->style.display == Display::Block ||
            child->style.display == Display::Flex ||
            child->style.position == Position::Absolute)
            continue;

        collectFrom(*child, child->style, runs);
    }
}

LineBox newLine() {
    LineBox line;
    line.height = 0;
    line.width = 0;
    line.baseline = 0;
    line.y = 0;
    return line;
}

TextRun subRun(const TextRun& run, const std::wstring& text, bool forceCjk) {
    TextRun out = run;
    out.text = text;
    out.width = 0;
    out.height = 0;
    out.forceCjk = forceCjk;
    return out;
}

void finishLine(std::vector<LineBox>& lines, LineBox& line) {
    if (line.runs.empty())
        return;

    line.width = 0;

    for (const TextRun& run : line.runs)
        line.width = std::max(line.width, run.x + run.width);

    lines.push_back(line);
    line = newLine();
}

bool hasCjkText(const std::wstring& text) {
    for (wchar_t c : text)
        if (isCjk(c))
            return true;
    return false;
}

// Whole-run and per-token GDI measurements are rounded independently, so a
// line can overflow maxContent by a fraction of a pixel. Treat that as "fits".
constexpr float kWrapTolerancePx = 3.0f;

} // namespace

std::vector<TextRun> collectInlineRuns(Element& el) {
    std::vector<TextRun> runs;
    collectFrom(el, el.style, runs);
    return runs;
}

std::vector<LineBox> breakLines(const std::vector<TextRun>& runs, float maxWidth, float scale) {
    (void)scale;

    std::vector<LineBox> lines;
    LineBox line = newLine();

    float x = 0;
    bool pendingSpace = false;
    TextRun spaceRun;

    auto flush = [&] {
        finishLine(lines, line);
        x = 0;
        pendingSpace = false;
    };

    for (const TextRun& original : runs) {
        const bool runHasCjk = hasCjkText(original.text);
        std::vector<float> offsets = charOffsets(original);

        if (offsets.size() != original.text.size() + 1) {
            offsets.assign(original.text.size() + 1, 0.f);
            for (size_t i = 0; i <= original.text.size(); ++i)
                offsets[i] = static_cast<float>(i) * original.fontPx * 0.55f;
        }

        size_t i = 0;

        while (i < original.text.size()) {
            wchar_t c = original.text[i];

            if (c == L'\n') {
                flush();
                ++i;
                continue;
            }

            if (iswspace(c)) {
                if (!line.runs.empty())
                    pendingSpace = true;

                spaceRun = subRun(original, L" ", runHasCjk);
                spaceRun.x = 0;
                measureRun(spaceRun);
                ++i;
                continue;
            }

            size_t end = i + 1;

            if (!isCjk(c)) {
                while (end < original.text.size() &&
                       !iswspace(original.text[end]) &&
                       !isCjk(original.text[end]) &&
                       original.text[end] != L'\n')
                    ++end;
            }

            std::wstring tokenText = original.text.substr(i, end - i);
            TextRun token = subRun(original, tokenText, runHasCjk);
            measureRun(token);
            if (_wgetenv(L"WRAP_DBG")) {
                fwprintf(stderr, L"DBG token='%ls' w=%.1f x=%.1f max=%.1f empty=%d\n",
                          tokenText.c_str(), token.width, x, maxWidth,
                          (int)line.runs.empty());
            }

            float spaceWidth = pendingSpace ? spaceRun.width : 0.f;
            float needed = (line.runs.empty() ? 0.f : spaceWidth) + token.width;

            // A CJK line must not start with closing punctuation (。、」…):
            // let it hang past the edge instead of wrapping it alone.
            const bool forbiddenStart = isLineStartForbidden(c);

            if (maxWidth > 0 && x + needed > maxWidth + kWrapTolerancePx &&
                !line.runs.empty() && !forbiddenStart) {
                flush();
                spaceWidth = 0;
            }

            if (maxWidth > 0 && x + spaceWidth + token.width > maxWidth + kWrapTolerancePx &&
                line.runs.empty() && token.width > maxWidth + kWrapTolerancePx) {
                // Overlong single word: split by character.
                std::wstring chunk;

                for (size_t k = i; k < end; ++k) {
                    std::wstring next = chunk + original.text[k];
                    TextRun test = subRun(original, next, runHasCjk);
                    measureRun(test);

                    if (maxWidth > 0 && test.width > maxWidth + kWrapTolerancePx && !chunk.empty()) {
                        TextRun segment = subRun(original, chunk, runHasCjk);
                        measureRun(segment);
                        segment.x = x;
                        line.runs.push_back(segment);
                        line.height = std::max(line.height, segment.lineHeightPx);
                        line.baseline = std::max(line.baseline, segment.height);
                        flush();
                        chunk.clear();
                    }

                    chunk += original.text[k];
                }

                if (!chunk.empty()) {
                    TextRun segment = subRun(original, chunk, runHasCjk);
                    measureRun(segment);
                    segment.x = x;
                    line.runs.push_back(segment);
                    line.height = std::max(line.height, segment.lineHeightPx);
                    line.baseline = std::max(line.baseline, segment.height);
                    x += segment.width;
                }

                i = end;
                continue;
            }

            if (pendingSpace && !line.runs.empty())
                x += spaceWidth;

            token.x = x;
            line.runs.push_back(token);
            line.height = std::max(line.height, token.lineHeightPx);
            line.baseline = std::max(line.baseline, token.height);
            x += token.width;

            pendingSpace = false;
            i = end;
        }
    }

    finishLine(lines, line);

    for (LineBox& l : lines) {
        if (l.height <= 0) {
            for (const TextRun& run : l.runs)
                l.height = std::max(l.height, run.lineHeightPx);
        }

        if (l.height <= 0)
            l.height = 16;
    }

    return lines;
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
    std::vector<TextRun> runs = collectInlineRuns(el);

    // collectInlineRuns() stores logical CSS metrics; layout works in
    // physical pixels.
    for (TextRun& run : runs) {
        run.fontPx *= scale;
        run.letterSpacing *= scale;
        run.lineHeightPx *= scale;
    }

    std::vector<LineBox> lines = breakLines(runs, innerW, scale);
    alignLines(lines, el.style, innerW);

    float y = contentY;

    for (LineBox& line : lines) {
        line.y = y;

        for (TextRun& run : line.runs) {
            run.x += contentX;
            run.y = y + std::max(0.f, (line.height - run.height) * 0.5f);
        }

        y += line.height;
    }

    g_lines[&el] = lines;
    return y - contentY;
}

bool hasInlineContent(const Element& el) {
    if (el.tag == L"input" || el.tag == L"textarea" ||
        el.tag == L"svg" || el.tag == L"div" && el.style.display == Display::Flex)
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

        // Replaced elements establish their own box and are painted by the
        // normal element walk, not as inline text runs.
        if (child->tag == L"svg" || child->tag == L"img" ||
            child->tag == L"input" || child->tag == L"textarea")
            return false;

        if (child->style.display == Display::Block ||
            child->style.display == Display::Flex)
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
