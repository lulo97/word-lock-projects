#pragma once
#include <string>
#include <unordered_map>
#include "dom/node.h"

namespace ui {

// Replace {{key}} placeholders in a string. In: text, values. Out: substituted text.
std::wstring applyTemplate(const std::wstring& text, const std::unordered_map<std::wstring, std::wstring>& values);

// Deep-clone a template element's children and fill placeholders in text nodes and attributes.
// In: template element, values. Out: list of new detached nodes.
std::vector<std::unique_ptr<Node>> instantiateTemplate(const Element& tpl,
        const std::unordered_map<std::wstring, std::wstring>& values);

// Wire a click handler to the element (stores in element attrs / Button control).
void bindClick(Element& el, std::function<void()> fn);

} // namespace ui
