#include "set_icon.h"

#include <map>
#include <mutex>
#include <vector>

namespace {

constexpr int APP_ICON_RESOURCE_ID = 1;
constexpr const wchar_t* DEFAULT_ICON_PATH = L"assets\\icon.ico";
constexpr const wchar_t* FALLBACK_ICON_PATH = L"icon.ico";

struct WindowIcons {
    HICON bigIcon = nullptr;
    HICON smallIcon = nullptr;
};

std::mutex g_iconsMutex;
std::map<HWND, WindowIcons> g_icons;

std::wstring module_directory() {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size())
        );

        if (length == 0)
            return L"";

        if (length < buffer.size() - 1) {
            std::wstring path(buffer.data(), length);
            std::wstring::size_type slash = path.find_last_of(L"\\/");

            if (slash == std::wstring::npos)
                return L".";

            return path.substr(0, slash);
        }

        buffer.resize(buffer.size() * 2);
    }
}

std::wstring current_directory() {
    DWORD length = GetCurrentDirectoryW(0, nullptr);

    if (length == 0)
        return L"";

    std::vector<wchar_t> buffer(length);
    length = GetCurrentDirectoryW(length, buffer.data());

    if (length == 0)
        return L"";

    return std::wstring(buffer.data(), length);
}

bool is_absolute_path(const std::wstring& path) {
    if (path.size() >= 2 && path[1] == L':')
        return true;

    return path.size() >= 2 &&
           path[0] == L'\\' &&
           path[1] == L'\\';
}

std::wstring join_path(
    const std::wstring& directory,
    const std::wstring& path
) {
    if (directory.empty())
        return path;

    wchar_t last = directory.back();

    if (last == L'\\' || last == L'/')
        return directory + path;

    return directory + L"\\" + path;
}

std::wstring parent_directory(const std::wstring& directory) {
    std::wstring::size_type slash = directory.find_last_of(L"\\/");

    if (slash == std::wstring::npos)
        return directory;

    // Keep the root of a drive ("C:\") intact.
    if (slash == 2 && directory.size() >= 3 && directory[1] == L':')
        return directory.substr(0, 3);

    return directory.substr(0, slash);
}

bool file_exists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void add_candidate(
    std::vector<std::wstring>& candidates,
    const std::wstring& path
) {
    if (path.empty())
        return;

    for (const std::wstring& candidate : candidates) {
        if (candidate == path)
            return;
    }

    candidates.push_back(path);
}

std::vector<std::wstring> icon_candidates(const std::wstring& iconPath) {
    std::vector<std::wstring> candidates;

    if (!iconPath.empty() && is_absolute_path(iconPath)) {
        add_candidate(candidates, iconPath);
        return candidates;
    }

    const std::wstring exeDirectory = module_directory();
    const std::wstring parentDirectory = parent_directory(exeDirectory);
    const std::wstring cwd = current_directory();

    const std::wstring relativePath =
        iconPath.empty() ? DEFAULT_ICON_PATH : iconPath;

    // Keep the caller's path first when it is explicitly supplied.
    if (!iconPath.empty())
        add_candidate(candidates, iconPath);

    // The build output lives in build\, so check both the executable
    // directory and its parent (the project directory).
    add_candidate(candidates, join_path(exeDirectory, relativePath));
    add_candidate(candidates, join_path(parentDirectory, relativePath));
    add_candidate(candidates, join_path(cwd, relativePath));

    if (iconPath.empty()) {
        add_candidate(candidates, join_path(exeDirectory, FALLBACK_ICON_PATH));
        add_candidate(candidates, join_path(parentDirectory, FALLBACK_ICON_PATH));
        add_candidate(candidates, join_path(cwd, FALLBACK_ICON_PATH));
    }

    return candidates;
}

HICON load_icon_file(
    const std::wstring& path,
    int width,
    int height
) {
    if (!file_exists(path))
        return nullptr;

    return static_cast<HICON>(LoadImageW(
        nullptr,
        path.c_str(),
        IMAGE_ICON,
        width,
        height,
        LR_LOADFROMFILE | LR_DEFAULTCOLOR
    ));
}

HICON load_icon_resource(
    HINSTANCE instance,
    int resourceId,
    int width,
    int height
) {
    HINSTANCE module = instance ? instance : GetModuleHandleW(nullptr);

    return static_cast<HICON>(LoadImageW(
        module,
        MAKEINTRESOURCEW(resourceId),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR
    ));
}

HICON load_best_icon(
    const std::wstring& iconPath,
    int width,
    int height
) {
    for (const std::wstring& candidate : icon_candidates(iconPath)) {
        HICON icon = load_icon_file(candidate, width, height);

        if (icon)
            return icon;
    }

    // A bad explicit path should not silently fall back to another icon.
    if (!iconPath.empty())
        return nullptr;

    // This is what keeps the icon working for a standalone .exe.
    return load_icon_resource(
        nullptr, APP_ICON_RESOURCE_ID, width, height
    );
}

void destroy_window_icons(WindowIcons& icons) {
    if (icons.bigIcon)
        DestroyIcon(icons.bigIcon);

    if (icons.smallIcon && icons.smallIcon != icons.bigIcon)
        DestroyIcon(icons.smallIcon);

    icons = WindowIcons{};
}

bool apply_icons(HWND hwnd, HICON bigIcon, HICON smallIcon) {
    if (!hwnd || !IsWindow(hwnd)) {
        if (bigIcon)
            DestroyIcon(bigIcon);

        if (smallIcon && smallIcon != bigIcon)
            DestroyIcon(smallIcon);

        return false;
    }

    // If only one size could be loaded, reuse it for both slots.
    if (!bigIcon && smallIcon)
        bigIcon = static_cast<HICON>(CopyIcon(smallIcon));

    if (!smallIcon && bigIcon)
        smallIcon = static_cast<HICON>(CopyIcon(bigIcon));

    if (!bigIcon || !smallIcon) {
        if (bigIcon)
            DestroyIcon(bigIcon);

        if (smallIcon && smallIcon != bigIcon)
            DestroyIcon(smallIcon);

        return false;
    }

    std::lock_guard<std::mutex> lock(g_iconsMutex);

    // Drop entries for windows that no longer exist.
    for (auto it = g_icons.begin(); it != g_icons.end();) {
        if (!IsWindow(it->first))
            it = g_icons.erase(it);
        else
            ++it;
    }

    auto existing = g_icons.find(hwnd);

    if (existing != g_icons.end()) {
        destroy_window_icons(existing->second);
        g_icons.erase(existing);
    }

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));

    // Also update the window class so newly shown frames use the icon.
    SetClassLongPtrW(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(bigIcon));
    SetClassLongPtrW(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(smallIcon));

    WindowIcons icons;
    icons.bigIcon = bigIcon;
    icons.smallIcon = smallIcon;
    g_icons[hwnd] = icons;

    // Refresh the non-client area (title bar / taskbar) immediately.
    SetWindowPos(
        hwnd,
        nullptr,
        0, 0, 0, 0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOZORDER |
        SWP_NOACTIVATE |
        SWP_FRAMECHANGED
    );

    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW
    );

    return true;
}

} // namespace

HICON load_icon(const std::wstring& iconPath, int width, int height) {
    return load_best_icon(iconPath, width, height);
}

bool set_icon(HWND hwnd, const std::wstring& iconPath) {
    if (!hwnd || !IsWindow(hwnd))
        return false;

    const int largeWidth = GetSystemMetrics(SM_CXICON);
    const int largeHeight = GetSystemMetrics(SM_CYICON);
    const int smallWidth = GetSystemMetrics(SM_CXSMICON);
    const int smallHeight = GetSystemMetrics(SM_CYSMICON);

    HICON bigIcon = load_best_icon(iconPath, largeWidth, largeHeight);
    HICON smallIcon = load_best_icon(iconPath, smallWidth, smallHeight);

    return apply_icons(hwnd, bigIcon, smallIcon);
}

bool set_icon_from_resource(
    HWND hwnd,
    int resourceId,
    HINSTANCE instance
) {
    if (!hwnd || !IsWindow(hwnd))
        return false;

    HICON bigIcon = load_icon_resource(
        instance,
        resourceId,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON)
    );

    HICON smallIcon = load_icon_resource(
        instance,
        resourceId,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON)
    );

    return apply_icons(hwnd, bigIcon, smallIcon);
}
