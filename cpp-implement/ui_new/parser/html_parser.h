#pragma once
#include <memory>
#include <string>
#include "dom/node.h"

namespace ui {

// Parse HTML text into a DOM tree.
// In: html source. Out: root element (tag "root") whose child is <body> content.
// Rules: every tag creates an Element; non-void tags are pushed on a stack;
// closers pop to nearest matching open tag; <head>/<style>/<script>/<title> are
// raw-text and dropped; comments end at "-->"; quotes inside tags are respected;
// entities decoded; whitespace-only text between blocks dropped.
std::unique_ptr<Element> parseHtml(const std::wstring& html);

// Is this a void tag (input, br, img, hr, meta, link, path-when-self-closed)?
bool isVoidTag(const std::wstring& tag);

// Raw-text tags whose content is skipped (style, script, title).
bool isRawTextTag(const std::wstring& tag);

// Parse attributes out of a tag's inner text.
// In: text after the tag name, e.g. 'class="a b" id=x autofocus'. Out: ordered attr pairs.
std::vector<std::pair<std::wstring, std::wstring>> parseAttributes(const std::wstring& tagInner);

// Tokenizer helper: find the '>' that ends a tag starting at pos, respecting quotes.
// In: html, pos of '<'. Out: index of '>' or npos.
size_t findTagEnd(const std::wstring& html, size_t pos);

} // namespace ui
