#pragma once
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "core/types.h"
#include "style/style.h"
#include "layout/box.h"
#include "controls/control.h"

namespace ui {

// Base of the DOM tree. Owns nothing but its parent pointer.
struct Node {
    enum class Kind { Element, Text };
    Kind kind;
    Node* parent = nullptr;
    explicit Node(Kind k) : kind(k) {}
    virtual ~Node() = default;
};

// Raw text between tags. Whitespace collapsing happens at layout time, not here.
struct TextNode : Node {
    std::wstring text;
    TextNode() : Node(Kind::Text) {}
};

// An HTML element. Parser fills tag/id/attrs/children; later stages fill the rest.
struct Element : Node {
    std::wstring tag;                                        // lowercase tag name
    std::wstring id;                                         // id attribute
    std::vector<std::pair<std::wstring, std::wstring>> attrs;// all attributes in order
    std::vector<std::unique_ptr<Node>> children;

    std::vector<std::wstring> classList;   // split class="" (filled by parser)
    ComputedStyle style;                   // filled by style stage
    LayoutBox box;                         // filled by layout stage
    std::unique_ptr<Control> control;      // input/textarea/range state, else null
    uint32_t stateFlags = 0;               // StateHover | StateFocus | ...
    bool styleDirty = true;                // needs re-resolve
    bool layoutDirty = true;               // needs re-layout

    Element() : Node(Kind::Element) {}
    ~Element() override;

    // Get attribute value. In: name. Out: value or empty string.
    std::wstring attr(const std::wstring& name) const;
    // True if attribute exists (for boolean attrs like autofocus).
    bool hasAttr(const std::wstring& name) const;
};

} // namespace ui
