#pragma once
#include <string>
#include "style/style.h"

namespace ui {

// Generic prefix-based class parser (p-*, w-*, bg-*, text-*, border-*, tracking-*, ...).
// In: class (no variant prefix, may start with '-' for negatives). Out: true + declarations appended.
bool applyGenericClass(const std::wstring& cls, Declarations& out);

// Individual handlers. Each: In: class. Out: true if it recognized the class.
bool ruleSpacing(const std::wstring& cls, Declarations& out);     // p/m/gap/space
bool ruleSizing(const std::wstring& cls, Declarations& out);       // w/h/min/max
bool ruleText(const std::wstring& cls, Declarations& out);         // size/align/color/[..]
bool ruleFont(const std::wstring& cls, Declarations& out);         // weight/family
bool ruleLeadingTracking(const std::wstring& cls, Declarations& out);
bool ruleBackground(const std::wstring& cls, Declarations& out);
bool ruleBorder(const std::wstring& cls, Declarations& out);       // width + color
bool ruleRounded(const std::wstring& cls, Declarations& out);
bool ruleShadow(const std::wstring& cls, Declarations& out);
bool rulePosition(const std::wstring& cls, Declarations& out);     // left/top/inset/translate/z
bool ruleFlex(const std::wstring& cls, Declarations& out);         // grow/shrink/basis

} // namespace ui
