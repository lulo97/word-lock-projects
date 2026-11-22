// progress.cpp
//
// Study progress store: %USERPROFILE%\kotobacpp\progress.json. See progress.h
// for the load/save semantics (defaults, .bak on corrupt, atomic write).

#include <windows.h>

#include "progress.h"
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

std::wstring ProgressStore::defaultPath() {
    const wchar_t* home = _wgetenv(L"USERPROFILE");
    return std::wstring(home ? home : L".") + L"\\kotobacpp\\progress.json";
}

ProgressStore::ProgressStore(std::wstring path) : path_(std::move(path)) {}

bool ProgressStore::load() {
    progress_ = Progress{};

    std::wstring dir = parentDir(path_);
    if (!dir.empty() && !ensureDir(dir))
        return false;

    std::ifstream f(std::string(ui::wideToUtf8(path_)), std::ios::binary);
    if (!f)
        return true;   // missing -> defaults

    std::string bytes((std::istreambuf_iterator<char>(f)), {});
    auto j = nlohmann::json::parse(bytes, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        if (!MoveFileExW(path_.c_str(), (path_ + L".bak").c_str(),
                         MOVEFILE_REPLACE_EXISTING))
            UI_LOG("progress", L"corrupt progress, could not move to %ls.bak",
                   path_.c_str());
        else
            UI_LOG("progress", L"corrupt progress, moved to %ls.bak", path_.c_str());
        return true;
    }

    progress_.version = j.value("version", 1);
    progress_.attempts.clear();
    if (j.contains("attempts") && j["attempts"].is_object())
        for (auto& [k, v] : j["attempts"].items())
            if (v.is_number())
                progress_.attempts[ui::utf8ToWide(k)] = v.get<int>();
    return true;
}

bool ProgressStore::save() const {
    std::wstring dir = parentDir(path_);
    if (!dir.empty() && !ensureDir(dir))
        return false;

    nlohmann::json j;
    j["version"] = progress_.version;
    j["attempts"] = nlohmann::json::object();
    for (const auto& [k, v] : progress_.attempts)
        j["attempts"][ui::wideToUtf8(k)] = v;

    // Atomic: temp file + rename (same as ConfigStore).
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
    return true;
}

int ProgressStore::attempts(const std::wstring& word) const {
    if (const auto it = progress_.attempts.find(word); it != progress_.attempts.end())
        return it->second;
    return 0;
}

void ProgressStore::record(const std::wstring& word) {
    ++progress_.attempts[word];
}

void ProgressStore::reset() {
    progress_.attempts.clear();
}
