#include "dom/tree_ops.h"

#include <algorithm>

namespace ui {

Element* findById(Element& root, const std::wstring& id) {
    if (root.id == id)
        return &root;

    for (auto& child : root.children) {
        Element* element = dynamic_cast<Element*>(child.get());

        if (!element)
            continue;

        Element* found = findById(*element, id);

        if (found)
            return found;
    }

    return nullptr;
}

std::vector<Element*> findByTag(Element& root, const std::wstring& tag) {
    std::vector<Element*> out;

    walk(root, [&](Element& el) {
        if (el.tag == tag)
            out.push_back(&el);

        return true;
    });

    return out;
}

Node* appendChild(Element& parent, std::unique_ptr<Node> child) {
    if (!child)
        return nullptr;

    child->parent = &parent;
    Node* raw = child.get();
    parent.children.push_back(std::move(child));
    markDirty(parent, false, true);
    return raw;
}

bool removeChild(Element& parent, Node* child) {
    if (!child)
        return false;

    auto it = std::find_if(
        parent.children.begin(), parent.children.end(),
        [&](const std::unique_ptr<Node>& node) {
            return node.get() == child;
        }
    );

    if (it == parent.children.end())
        return false;

    parent.children.erase(it);
    markDirty(parent, false, true);
    return true;
}

void clearChildren(Element& parent) {
    parent.children.clear();
    markDirty(parent, false, true);
}

void setText(Element& el, const std::wstring& text) {
    clearChildren(el);

    auto node = std::make_unique<TextNode>();
    node->text = text;
    appendChild(el, std::move(node));
}

std::wstring textContent(const Element& el) {
    std::wstring out;

    for (const auto& child : el.children) {
        if (child->kind == Node::Kind::Text) {
            out += static_cast<const TextNode*>(child.get())->text;
        }
        else {
            const Element* element = static_cast<const Element*>(child.get());
            out += textContent(*element);
        }
    }

    return out;
}

namespace {

std::unique_ptr<Element> cloneElement(const Element& src) {
    auto out = std::make_unique<Element>();
    out->tag = src.tag;
    out->id = src.id;
    out->attrs = src.attrs;
    out->classList = src.classList;
    out->stateFlags = src.stateFlags;
    out->styleDirty = true;
    out->layoutDirty = true;

    for (const auto& child : src.children) {
        if (child->kind == Node::Kind::Text) {
            auto text = std::make_unique<TextNode>();
            text->text = static_cast<const TextNode*>(child.get())->text;
            text->parent = out.get();
            out->children.push_back(std::move(text));
        }
        else {
            auto sub = cloneElement(
                *static_cast<const Element*>(child.get())
            );
            sub->parent = out.get();
            out->children.push_back(std::move(sub));
        }
    }

    return out;
}

} // namespace

std::unique_ptr<Element> cloneSubtree(const Element& src) {
    return cloneElement(src);
}

void walk(Element& root, const std::function<bool(Element&)>& fn) {
    if (!fn(root))
        return;

    // Copy pointers first: the callback may mutate the tree.
    std::vector<Element*> elements;

    for (auto& child : root.children) {
        Element* element = dynamic_cast<Element*>(child.get());

        if (element)
            elements.push_back(element);
    }

    for (Element* element : elements) {
        if (element->parent != &root)
            continue; // removed by an earlier callback

        walk(*element, fn);
    }
}

void markDirty(Element& el, bool style, bool layout) {
    Element* current = &el;

    while (current) {
        if (style)
            current->styleDirty = true;

        if (layout)
            current->layoutDirty = true;

        current = dynamic_cast<Element*>(current->parent);
    }
}

} // namespace ui
