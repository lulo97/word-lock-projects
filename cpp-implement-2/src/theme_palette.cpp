#include "app.h"

namespace wordlock {

namespace {

struct ThemeEntry {
    const wchar_t* id;
    ThemePalette palette;
};

const ThemeEntry kPalettes[] = {
    {L"paper", {RGB(0xfb, 0xfa, 0xf7), RGB(0x1f, 0x29, 0x37), L"Georgia", false}},
    {L"brutal", {RGB(0xff, 0xf5, 0x9d), RGB(0x00, 0x00, 0x00), L"Arial Black", true}},
    {L"candy", {RGB(0xff, 0xe4, 0xef), RGB(0x83, 0x18, 0x43), L"Comic Sans MS", false}},
    {L"glass", {RGB(0xe0, 0xf2, 0xfe), RGB(0x1e, 0x3a, 0x8a), L"Segoe UI", false}},
    {L"midnight", {RGB(0x1e, 0x29, 0x3b), RGB(0xe2, 0xe8, 0xf0), L"Georgia", false}},
    {L"notebook", {RGB(0xff, 0xfe, 0xf8), RGB(0x1e, 0x29, 0x3b), L"Segoe Print", false}},
    {L"sakura", {RGB(0xff, 0xe3, 0xea), RGB(0x7f, 0x1d, 0x3a), L"Georgia", false}},
    {L"synthwave", {RGB(0x2d, 0x1b, 0x4e), RGB(0xf5, 0xd0, 0xfe), L"Courier New", true}},
    {L"terminal", {RGB(0x04, 0x14, 0x0a), RGB(0x4a, 0xde, 0x80), L"Consolas", false}},
    {L"zen", {RGB(0xfa, 0xfa, 0xf9), RGB(0x44, 0x40, 0x3c), L"Georgia", false}},
};

}  // namespace

ThemePalette GetThemePalette(const std::wstring& theme) {
    for (const ThemeEntry& entry : kPalettes) {
        if (theme == entry.id) {
            return entry.palette;
        }
    }
    return kPalettes[0].palette;
}

}  // namespace wordlock
