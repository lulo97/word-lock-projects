#include "app.h"

namespace wordlock {

HICON LoadAppIcon(int width, int height) {
    HINSTANCE instance = g_hInstance != nullptr ? g_hInstance : GetModuleHandleW(nullptr);
    return reinterpret_cast<HICON>(
        LoadImageW(instance,
                   MAKEINTRESOURCE(IDI_APP_ICON),
                   IMAGE_ICON,
                   width,
                   height,
                   width == 0 && height == 0 ? LR_DEFAULTSIZE : 0));
}

}  // namespace wordlock
