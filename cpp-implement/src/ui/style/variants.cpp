#include "style/variants.h"

#include "core/utf.h"
#include "style/class_rules.h"
#include "style/class_table.h"

namespace ui {

namespace {

Variant variantFromName(const std::wstring& name) {
    if (name == L"hover") return Variant::Hover;
    if (name == L"focus") return Variant::Focus;
    if (name == L"active") return Variant::Active;
    if (name == L"disabled") return Variant::Disabled;
    if (name == L"sm") return Variant::Sm;
    if (name == L"md") return Variant::Md;
    if (name == L"lg") return Variant::Lg;
    return Variant::None;
}

void append(Declarations& target, const Declarations& source) {
    target.insert(target.end(), source.begin(), source.end());
}

Declarations declarationsFor(const std::wstring& base) {
    Declarations out;

    if (lookupStaticClass(base, out))
        return out;

    if (applyGenericClass(base, out))
        return out;

    out.clear();
    return out;
}

} // namespace

VariantClass splitVariant(const std::wstring& token) {
    VariantClass out;
    out.base = token;

    std::vector<std::wstring> parts = split(token, L':');

    if (parts.size() <= 1)
        return out;

    out.base = parts.back();

    // A stacked token like md:hover:bg-x keeps the first condition we
    // understand. Responsive prefixes are intentionally applied first.
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        Variant variant = variantFromName(toLower(parts[i]));

        if (variant != Variant::None) {
            out.variant = variant;
            break;
        }
    }

    return out;
}

VariantSet buildVariantSet(const std::vector<std::wstring>& classList) {
    VariantSet set;

    for (const std::wstring& token : classList) {
        VariantClass vc = splitVariant(token);
        std::wstring base = toLower(vc.base);

        if (base.empty() || isIgnoredClass(base))
            continue;

        Declarations decls = declarationsFor(base);

        if (decls.empty())
            continue;

        switch (vc.variant) {
        case Variant::None:     append(set.base, decls); break;
        case Variant::Hover:    append(set.hover, decls); break;
        case Variant::Focus:    append(set.focus, decls); break;
        case Variant::Active:   append(set.active, decls); break;
        case Variant::Disabled: append(set.disabled, decls); break;
        case Variant::Sm:       append(set.sm, decls); break;
        case Variant::Md:       append(set.md, decls); break;
        case Variant::Lg:       append(set.lg, decls); break;
        }
    }

    return set;
}

bool isVariantActive(Variant v, uint32_t stateFlags, int viewportWidth) {
    switch (v) {
    case Variant::Hover:    return (stateFlags & StateHover) != 0;
    case Variant::Focus:    return (stateFlags & StateFocus) != 0;
    case Variant::Active:   return (stateFlags & StateActive) != 0;
    case Variant::Disabled: return (stateFlags & StateDisabled) != 0;
    case Variant::Sm:       return viewportWidth >= 640;
    case Variant::Md:       return viewportWidth >= 768;
    case Variant::Lg:       return viewportWidth >= 1024;
    case Variant::None:
    default:                return true;
    }
}

std::vector<ClassIssue> findClassIssues(const std::vector<std::wstring>& classList) {
    std::vector<ClassIssue> out;

    for (const std::wstring& token : classList) {
        VariantClass vc = splitVariant(token);
        std::wstring base = toLower(vc.base);

        if (base.empty() || isIgnoredClass(base))
            continue;

        // "dark:bg-x", "group-hover:x": splitVariant() falls back to Variant::None,
        // so the base class gets applied UNCONDITIONALLY. Report it.
        if (token.find(L':') != std::wstring::npos && vc.variant == Variant::None)
            out.push_back({ token, L"unsupported variant prefix (applied unconditionally!)" });

        if (declarationsFor(base).empty())
            out.push_back({ token, L"unknown class (ignored)" });
    }

    return out;
}

} // namespace ui
