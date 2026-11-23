#include "controls/text_area.h"

#include "dom/node.h"
#include "dom/tree_ops.h"
#include "paint/canvas.h"
#include "paint/text_draw.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

std::vector<std::wstring> splitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring current;

    for (wchar_t c : text) {
        if (c == L'\n') {
            lines.push_back(current);
            current.clear();
        }
        else {
            current.push_back(c);
        }
    }

    lines.push_back(current);
    return lines;
}

} // namespace

void TextArea::init(Element& el) {
    value = el.attr(L"value");

    if (value.empty())
        value = textContent(el);

    placeholder = el.attr(L"placeholder");
    caret = static_cast<int>(value.size());
    selAnchor = -1;
}

std::vector<int> TextArea::wrapLines(Element& el, float width) const {
    (void)width;

    std::vector<int> starts;
    starts.push_back(0);

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\n')
            starts.push_back(static_cast<int>(i + 1));
    }

    (void)el;
    return starts;
}

void TextArea::paint(Canvas& canvas, Element& el, float scale) {
    scale_ = scale;
    RectF content = el.box.contentRect();

    if (content.w <= 0 || content.h <= 0)
        return;

    canvas.pushClip(content, {});

    TextRun base = makeRun(value.empty() ? placeholder : value, el.style, scale);
    base.color = value.empty() ? Color::rgb(156, 163, 175) : el.style.color;
    measureRun(base);

    float lineH = base.lineHeightPx > 0 ? base.lineHeightPx : base.fontPx * 1.5f;
    float y = content.y - scrollY;

    if (value.empty()) {
        base.x = content.x + 4 * scale;
        base.y = y + std::max(0.f, (lineH - base.height) / 2);
        canvas.drawText(base);
        y += lineH;
    }
    else {
        std::vector<std::wstring> paragraphs = splitLines(value);

        for (const std::wstring& paragraph : paragraphs) {
            TextRun run = makeRun(paragraph, el.style, scale);
            measureRun(run);
            run.x = content.x + 4 * scale;
            run.y = y + std::max(0.f, (lineH - run.height) / 2);
            canvas.drawText(run);
            y += lineH;
        }
    }

    if (el.stateFlags & StateFocus) {
        if (caretVisible) {
            float caretX = content.x + 4 * scale;
            float caretY = content.y;

            TextArea::wrapLines(el, content.w);

            int consumed = 0;
            std::vector<std::wstring> lines = splitLines(value);

            for (size_t i = 0; i < lines.size(); ++i) {
                int lineStart = consumed;
                int lineEnd = consumed + static_cast<int>(lines[i].size());

                if (caret >= lineStart && caret <= lineEnd) {
                    std::wstring prefix = lines[i].substr(0, std::max(0, caret - lineStart));
                    TextRun run = makeRun(prefix, el.style, scale);
                    measureRun(run);
                    caretX = content.x + 4 * scale + run.width;
                    caretY = content.y - scrollY + static_cast<float>(i) * lineH;
                    break;
                }

                consumed = lineEnd + 1;
            }

            canvas.drawLine(
                { caretX, caretY + 2 },
                { caretX, caretY + lineH - 2 },
                std::max(1.f, scale),
                el.style.color
            );
        }
    }

    canvas.popClip();
}

bool TextArea::onMouse(Element& el, const MouseEvent& e) {
    if (e.button != 0 || !e.down)
        return false;

    RectF content = el.box.contentRect();
    TextRun run = makeRun(value, el.style, scale_);
    measureRun(run);

    float lineH = run.lineHeightPx > 0 ? run.lineHeightPx : run.fontPx * 1.5f;
    int line = static_cast<int>((e.y - content.y + scrollY) / std::max(1.f, lineH));

    std::vector<std::wstring> lines = splitLines(value);
    line = std::max(0, std::min(static_cast<int>(lines.size()) - 1, line));

    int start = 0;

    for (int i = 0; i < line; ++i)
        start += static_cast<int>(lines[i].size()) + 1;

    std::vector<float> offsets = charOffsets(run);
    float relative = e.x - content.x - 4 * scale_;
    int column = 0;

    while (column < static_cast<int>(lines[line].size()) &&
           offsets[column + 1] <= relative)
        ++column;

    caret = start + column;
    selAnchor = -1;
    caretVisible = true;
    return true;
}

bool TextArea::onKey(Element& el, const KeyEvent& e) {
    int size = static_cast<int>(value.size());
    caret = std::max(0, std::min(size, caret));
    bool changed = false;

    switch (e.vk) {
    case VK_LEFT:
        if (caret > 0) --caret;
        break;
    case VK_RIGHT:
        if (caret < size) ++caret;
        break;
    case VK_UP:
        moveCaretVertical(el, -1);
        break;
    case VK_DOWN:
        moveCaretVertical(el, +1);
        break;
    case VK_HOME: {
        size_t pos = value.rfind(L'\n', caret == 0 ? 0 : caret - 1);
        caret = pos == std::wstring::npos ? 0 : static_cast<int>(pos + 1);
        break;
    }
    case VK_END: {
        size_t pos = value.find(L'\n', caret);
        caret = pos == std::wstring::npos ? size : static_cast<int>(pos);
        break;
    }
    case VK_DELETE:
        if (caret < size) {
            value.erase(caret, 1);
            changed = true;
        }
        break;
    default:
        return false;
    }

    caretVisible = true;

    if (changed && onChange)
        onChange(value);

    return true;
}

bool TextArea::onChar(Element& el, wchar_t ch) {
    (void)el;

    if (ch == L'\b') {
        if (caret > 0 && !value.empty()) {
            value.erase(caret - 1, 1);
            --caret;

            if (onChange)
                onChange(value);
        }

        return true;
    }

    if (ch == L'\r')
        ch = L'\n';

    if (ch == 27 || ch == 127)
        return true;

    if (ch < 32 && ch != L'\n')
        return false;

    caret = std::max(0, std::min(static_cast<int>(value.size()), caret));
    value.insert(caret, 1, ch);
    ++caret;
    caretVisible = true;

    if (onChange)
        onChange(value);

    return true;
}

void TextArea::onFocus(Element& el, bool focused) {
    el.stateFlags = focused
        ? (el.stateFlags | StateFocus)
        : (el.stateFlags & ~StateFocus);

    caretVisible = true;
}

bool TextArea::onBlinkTick() {
    caretVisible = !caretVisible;
    return true;
}

void TextArea::setValue(const std::wstring& text) {
    value = text;
    caret = static_cast<int>(value.size());
    selAnchor = -1;
    scrollY = 0;

    if (onChange)
        onChange(value);
}

void TextArea::moveCaretVertical(Element& el, int delta) {
    size_t lineStart = value.rfind(L'\n', caret == 0 ? 0 : caret - 1);
    lineStart = lineStart == std::wstring::npos ? 0 : lineStart + 1;
    int column = caret - static_cast<int>(lineStart);

    if (delta < 0) {
        if (lineStart == 0)
            return;

        size_t previousStart = value.rfind(L'\n', lineStart - 2);
        previousStart = previousStart == std::wstring::npos ? 0 : previousStart + 1;
        caret = static_cast<int>(previousStart) + column;
    }
    else {
        size_t lineEnd = value.find(L'\n', caret);

        if (lineEnd == std::wstring::npos)
            return;

        size_t nextStart = lineEnd + 1;
        size_t nextEnd = value.find(L'\n', nextStart);

        if (nextEnd == std::wstring::npos)
            nextEnd = value.size();

        caret = static_cast<int>(nextStart) +
                std::min(column, static_cast<int>(nextEnd - nextStart));
    }

    (void)el;
}

} // namespace ui
