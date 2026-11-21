#include "app/bindings.h"

#include "controls/button.h"
#include "core/utf.h"
#include "dom/tree_ops.h"

#include <memory>

namespace ui {

namespace {

std::unique_ptr<Node> cloneNodeWithValues(
    const Node& node,
    const std::unordered_map<std::wstring, std::wstring>& values
) {
    if (node.kind == Node::Kind::Text) {
        auto out = std::make_unique<TextNode>();
        out->text = applyTemplate(
            static_cast<const TextNode&>(node).text,
            values
        );
        return out;
    }

    const Element& src = static_cast<const Element&>(node);
    auto out = std::make_unique<Element>();
    out->tag = src.tag;
    out->id = src.id;
    out->classList = src.classList;
    out->attrs = src.attrs;

    for (auto& attr : out->attrs)
        attr.second = applyTemplate(attr.second, values);

    for (const auto& child : src.children) {
        auto copy = cloneNodeWithValues(*child, values);
        copy->parent = out.get();
        out->children.push_back(std::move(copy));
    }

    return out;
}

} // namespace

std::wstring applyTemplate(
    const std::wstring& text,
    const std::unordered_map<std::wstring, std::wstring>& values
) {
    std::wstring out = text;

    for (const auto& entry : values) {
        std::wstring marker = L"{{" + entry.first + L"}}";
        size_t pos = 0;

        while ((pos = out.find(marker, pos)) != std::wstring::npos) {
            out.replace(pos, marker.size(), entry.second);
            pos += entry.second.size();
        }
    }

    return out;
}

std::vector<std::unique_ptr<Node>> instantiateTemplate(
    const Element& tpl,
    const std::unordered_map<std::wstring, std::wstring>& values
) {
    std::vector<std::unique_ptr<Node>> out;

    for (const auto& child : tpl.children)
        out.push_back(cloneNodeWithValues(*child, values));

    return out;
}

void bindClick(Element& el, std::function<void()> fn) {
    if (auto* button = dynamic_cast<Button*>(el.control.get())) {
        button->onClick = std::move(fn);
        return;
    }

    // DOM-level click handlers are stored by Document::onClick(id).
}

} // namespace ui
