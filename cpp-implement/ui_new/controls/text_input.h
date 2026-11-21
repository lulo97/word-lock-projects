#pragma once
#include "controls/control.h"

namespace ui {

// Single-line text input: caret, selection, scroll, clipboard, IME.
class TextInput : public Control {
public:
    std::wstring value;
    std::wstring placeholder;
    int caret = 0;                 // index in value
    int selAnchor = -1;            // selection start or -1
    float scrollX = 0;             // horizontal scroll px
    bool caretVisible = true;
    std::function<void(const std::wstring&)> onChange;

    // Init from element attrs (value, placeholder, type). In: element.
    void init(Element& el);

    void paint(Canvas& c, Element& el, float scale) override;
    bool onMouse(Element& el, const MouseEvent& e) override;
    bool onKey(Element& el, const KeyEvent& e) override;      // arrows, Home/End, Del, Ctrl+A/C/V/X
    bool onChar(Element& el, wchar_t ch) override;            // insert / backspace
    void onFocus(Element& el, bool focused) override;
    SizeF intrinsicSize(Element& el, float scale) override;
    CursorKind cursor() const override { return CursorKind::IBeam; }
    bool onBlinkTick() override;

    // Replace selection (or insert at caret). In: text. Fires onChange.
    void insertText(const std::wstring& text);
    // Set value programmatically. In: text. Moves caret to end.
    void setValue(const std::wstring& text);
    // Map x pixel to character index. In: x relative to content box, element. Out: index.
    int indexFromX(Element& el, float x) const;
    // Keep caret within visible area by adjusting scrollX. In: element, visible width.
    void ensureCaretVisible(Element& el, float visibleW);
    // IME composition string handling. In: composition text, commit flag.
    void onImeComposition(const std::wstring& comp, bool commit);
};

} // namespace ui
