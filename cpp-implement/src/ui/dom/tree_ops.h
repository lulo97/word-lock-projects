#pragma once
#include <functional>
#include <memory>
#include "dom/node.h"

namespace ui {

// Find first element with matching id (depth-first). In: root, id. Out: element or nullptr.
Element* findById(Element& root, const std::wstring& id);

// Collect all elements with a tag. In: root, tag. Out: list of pointers.
std::vector<Element*> findByTag(Element& root, const std::wstring& tag);

// Append child, set its parent. In: parent, child (ownership moves). Out: raw pointer to child.
Node* appendChild(Element& parent, std::unique_ptr<Node> child);

// Remove and destroy a child. In: parent, child pointer. Out: true if removed.
bool removeChild(Element& parent, Node* child);

// Remove all children of an element.
void clearChildren(Element& parent);

// Replace all children with one text node. In: el, text. Out: none. Marks layout dirty.
void setText(Element& el, const std::wstring& text);

// Concatenate descendant text. In: el. Out: text.
std::wstring textContent(const Element& el);

// Deep-copy a subtree (attrs, classes, children; not layout/control state).
// In: src. Out: new detached element.
std::unique_ptr<Element> cloneSubtree(const Element& src);

// Walk all elements pre-order. In: root, callback (return false to skip children).
void walk(Element& root, const std::function<bool(Element&)>& fn);

// Mark element and ancestors layoutDirty (and optionally styleDirty).
void markDirty(Element& el, bool style, bool layout);

} // namespace ui
