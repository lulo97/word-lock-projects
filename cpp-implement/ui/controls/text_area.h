#pragma once
#include "controls/control.h"

namespace ui {

// Multi-line text input with wrapping and vertical scroll.
class TextArea : public Control {
public:
    std::wstring value;
    std::wstring placeholder;
    int caret = 0;
    int selAnchor = -1;
    float scrollY = 0;
    float scale_ = 1.f;            // last known DPI scale (set in paint)
    bool caretVisible = true;
    std::function<void(const std::wstring&)> onChange;

    void init(Element& el);
    void paint(Canvas& c, Element& el, float scale) override;
    bool onMouse(Element& el, const MouseEvent& e) override;
    bool onKey(Element& el, const KeyEvent& e) override;      // + Up/Down, Enter
    bool onChar(Element& el, wchar_t ch) override;
    void onFocus(Element& el, bool focused) override;
    CursorKind cursor() const override { return CursorKind::IBeam; }
    bool onBlinkTick() override;

    void setValue(const std::wstring& text);
    // Wrap text into visual lines for the content width. In: width px. Out: line start indices.
    std::vector<int> wrapLines(Element& el, float width) const;
    // Move caret up/down one visual line keeping x. In: delta(-1/+1).
    void moveCaretVertical(Element& el, int delta);
};

} // namespace ui
