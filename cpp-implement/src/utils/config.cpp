// config.cpp
//
// User config store: %USERPROFILE%\kotobacpp\config.json. See config.h for the
// load/save semantics (defaults, .bak on corrupt, atomic write).

#include <windows.h>

#include "config.h"
#include "core/utf.h"
#include "debug_log.h"
#include "json.hpp"

#include <fstream>

namespace {

bool dirExists(const std::wstring& p) {
    DWORD attrs = GetFileAttributesW(p.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool ensureDir(const std::wstring& p) {
    if (dirExists(p))
        return true;
    return CreateDirectoryW(p.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring parentDir(const std::wstring& path) {
    size_t slash = path.find_last_of(L"/\\");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

} // namespace

std::wstring ConfigStore::defaultPath() {
    const wchar_t* home = _wgetenv(L"USERPROFILE");
    return std::wstring(home ? home : L".") + L"\\kotobacpp\\config.json";
}

ConfigStore::ConfigStore(std::wstring path) : path_(std::move(path)) {}

bool ConfigStore::load() {
    config_ = Config{};

    std::wstring dir = parentDir(path_);
    if (!dir.empty() && !ensureDir(dir))
        return false;

    std::ifstream f(std::string(ui::wideToUtf8(path_)), std::ios::binary);
    if (!f)
        return true;   // missing -> defaults

    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    auto j = nlohmann::json::parse(bytes, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        // Corrupt: keep a .bak copy for inspection, fall back to defaults.
        if (!MoveFileExW(path_.c_str(), (path_ + L".bak").c_str(),
                         MOVEFILE_REPLACE_EXISTING))
            UI_LOG("config", L"corrupt config, could not move to %ls.bak",
                   path_.c_str());
        else
            UI_LOG("config", L"corrupt config, moved to %ls.bak", path_.c_str());
        return true;
    }

    config_.version = j.value("version", 1);
    config_.countdownSeconds = j.value("countdownSeconds", 600);
    config_.volume = static_cast<float>(j.value("volume", 2.0));
    config_.randomUi = j.value("randomUi", true);
    config_.startWithWindows = j.value("startWithWindows", false);
    config_.freezeChrome = j.value("freezeChrome", true);
    config_.maxFreezeMinutes = j.value("maxFreezeMinutes", 10);
    if (j.contains("ignoreWords") && j["ignoreWords"].is_array())
        for (const auto& w : j["ignoreWords"])
            if (w.is_string())
                config_.ignoreWords.push_back(ui::utf8ToWide(w.get<std::string>()));
    return true;
}

bool ConfigStore::save() const {
    std::wstring dir = parentDir(path_);
    if (!dir.empty() && !ensureDir(dir))
        return false;

    nlohmann::json j;
    j["version"] = config_.version;
    j["countdownSeconds"] = config_.countdownSeconds;
    j["volume"] = config_.volume;
    j["ignoreWords"] = nlohmann::json::array();
    for (const auto& w : config_.ignoreWords)
        j["ignoreWords"].push_back(ui::wideToUtf8(w));
    j["randomUi"] = config_.randomUi;
    j["startWithWindows"] = config_.startWithWindows;
    j["freezeChrome"] = config_.freezeChrome;
    j["maxFreezeMinutes"] = config_.maxFreezeMinutes;

    // Atomic: temp file + rename. A crash mid-write can never leave a
    // half-written config.
    std::wstring tmp = path_ + L".tmp";
    {
        std::ofstream out(std::string(ui::wideToUtf8(tmp)), std::ios::binary);
        if (!out)
            return false;
        out << j.dump(2);
        out.flush();
        if (!out)
            return false;
    }
    if (!MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    UI_LOG("config", L"saved %ls", path_.c_str());
    return true;
}
