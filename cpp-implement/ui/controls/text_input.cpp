#include "controls/text_input.h"

#include "dom/node.h"
#include "paint/canvas.h"
#include "paint/text_draw.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

std::wstring clipboardText() {
    std::wstring out;

    if (!OpenClipboard(nullptr))
        return out;

    HANDLE data = GetClipboardData(CF_UNICODETEXT);

    if (data) {
        const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data));

        if (text) {
            out = text;
            GlobalUnlock(data);
        }
    }

    CloseClipboard();
    return out;
}

void setClipboardText(const std::wstring& text) {
    if (!OpenClipboard(nullptr))
        return;

    EmptyClipboard();

    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);

    if (memory) {
        void* target = GlobalLock(memory);

        if (target) {
            memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        }
    }

    CloseClipboard();
}

int clampIndex(int value, int size) {
    return std::max(0, std::min(size, value));
}

} // namespace

void TextInput::init(Element& el) {
    value = el.attr(L"value");
    placeholder = el.attr(L"placeholder");
    caret = static_cast<int>(value.size());
    selAnchor = -1;
}

void TextInput::paint(Canvas& canvas, Element& el, float scale) {
    RectF content = el.box.contentRect();

    if (content.w <= 0 || content.h <= 0)
        return;

    bool empty = value.empty();
    TextRun run = makeRun(empty ? placeholder : value, el.style, scale);

    if (empty)
        run.color = Color::rgb(156, 163, 175);

    measureRun(run);

    std::vector<float> offsets = charOffsets(run);
    int safeCaret = clampIndex(caret, static_cast<int>(value.size()));
    float caretOffset = offsets.empty() ? 0.f : offsets[safeCaret];

    ensureCaretVisible(el, content.w);

    if (!empty && caretOffset - scrollX > content.w)
        scrollX = caretOffset - content.w;

    if (!empty && caretOffset - scrollX < 0)
        scrollX = caretOffset;

    scrollX = std::max(0.f, scrollX);

    canvas.pushClip(content, {});
    run.x = content.x - scrollX;
    run.y = content.y + (content.h - run.height) / 2;
    canvas.drawText(run);

    if (el.stateFlags & StateFocus) {
        if (caretVisible) {
            float caretX = content.x + caretOffset - scrollX;
            caretX = std::max(content.x, std::min(content.right() - 1, caretX));
            float caretH = std::min(content.h - 4, run.fontPx * 1.2f);
            float top = content.y + (content.h - caretH) / 2;

            canvas.drawLine(
                { caretX, top },
                { caretX, top + caretH },
                std::max(1.f, scale),
                el.style.color
            );
        }
    }

    canvas.popClip();
}

bool TextInput::onMouse(Element& el, const MouseEvent& e) {
    if (e.button != 0 || !e.down)
        return false;

    RectF content = el.box.contentRect();
    caret = indexFromX(el, e.x - content.x + scrollX);
    selAnchor = -1;
    caretVisible = true;
    return true;
}

bool TextInput::onKey(Element& el, const KeyEvent& e) {
    bool changed = false;
    int size = static_cast<int>(value.size());
    caret = clampIndex(caret, size);

    if (e.ctrl && e.vk == 'A') {
        selAnchor = 0;
        caret = size;
        return true;
    }

    if (e.ctrl && e.vk == 'C') {
        if (selAnchor >= 0) {
            int a = std::min(selAnchor, caret);
            int b = std::max(selAnchor, caret);
            setClipboardText(value.substr(a, b - a));
        }
        return true;
    }

    if (e.ctrl && e.vk == 'X') {
        if (selAnchor >= 0) {
            int a = std::min(selAnchor, caret);
            int b = std::max(selAnchor, caret);
            setClipboardText(value.substr(a, b - a));
            value.erase(a, b - a);
            caret = a;
            selAnchor = -1;

            if (onChange)
                onChange(value);
        }
        return true;
    }

    if (e.ctrl && e.vk == 'V') {
        insertText(clipboardText());
        return true;
    }

    switch (e.vk) {
    case VK_LEFT:
        caret = clampIndex(caret - 1, size);
        selAnchor = -1;
        break;

    case VK_RIGHT:
        caret = clampIndex(caret + 1, size);
        selAnchor = -1;
        break;

    case VK_HOME:
        caret = 0;
        selAnchor = -1;
        break;

    case VK_END:
        caret = size;
        selAnchor = -1;
        break;

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

bool TextInput::onChar(Element& el, wchar_t ch) {
    (void)el;

    if (ch == L'\b') {
        if (caret > 0 && !value.empty()) {
            value.erase(caret - 1, 1);
            --caret;
            caretVisible = true;

            if (onChange)
                onChange(value);
        }

        return true;
    }

    if (ch == L'\r' || ch == L'\n' || ch == 27 || ch == 127)
        return true;

    if (ch < 32)
        return false;

    std::wstring text(1, ch);
    insertText(text);
    return true;
}

void TextInput::onFocus(Element& el, bool focused) {
    (void)el;

    el.stateFlags = focused
        ? (el.stateFlags | StateFocus)
        : (el.stateFlags & ~StateFocus);

    caretVisible = true;
}

SizeF TextInput::intrinsicSize(Element& el, float scale) {
    float lineHeight = el.style.lineHeightIsPx
        ? el.style.lineHeight * scale
        : el.style.fontSize * scale * el.style.lineHeight;

    return { 200 * scale, std::max(lineHeight, el.style.fontSize * scale * 1.2f) };
}

bool TextInput::onBlinkTick() {
    caretVisible = !caretVisible;
    return true;
}

void TextInput::insertText(const std::wstring& text) {
    if (text.empty())
        return;

    int size = static_cast<int>(value.size());
    caret = clampIndex(caret, size);

    int start = caret;
    int end = caret;

    if (selAnchor >= 0) {
        start = std::max(0, std::min(selAnchor, caret));
        end = std::min(size, std::max(selAnchor, caret));
    }

    value.replace(start, end - start, text);
    caret = start + static_cast<int>(text.size());
    selAnchor = -1;
    caretVisible = true;

    if (onChange)
        onChange(value);
}

void TextInput::setValue(const std::wstring& text) {
    value = text;
    caret = static_cast<int>(value.size());
    selAnchor = -1;
    scrollX = 0;

    if (onChange)
        onChange(value);
}

int TextInput::indexFromX(Element& el, float x) const {
    (void)el;

    TextRun run = makeRun(value, el.style, 1.f);
    std::vector<float> offsets = charOffsets(run);

    int best = 0;
    float bestDistance = x;

    for (size_t i = 0; i < offsets.size(); ++i) {
        float distance = std::fabs(offsets[i] - x);

        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int>(i);
        }
    }

    return best;
}

void TextInput::ensureCaretVisible(Element& el, float visibleW) {
    if (visibleW <= 0)
        return;

    TextRun run = makeRun(value, el.style, 1.f);
    std::vector<float> offsets = charOffsets(run);
    int safeCaret = clampIndex(caret, static_cast<int>(value.size()));
    float caretX = offsets.empty() ? 0.f : offsets[safeCaret];

    if (caretX - scrollX > visibleW)
        scrollX = caretX - visibleW;

    if (caretX - scrollX < 0)
        scrollX = caretX;

    scrollX = std::max(0.f, scrollX);
}

void TextInput::onImeComposition(const std::wstring& comp, bool commit) {
    if (commit)
        insertText(comp);
}

} // namespace ui
