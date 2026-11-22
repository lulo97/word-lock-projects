#pragma once

#include <string>
#include <vector>

// User config: %USERPROFILE%\kotobacpp\config.json (M1).
//
// load(): missing file -> defaults. Corrupt file -> moved to <path>.bak and
// defaults are used (the corrupt file is kept for inspection).
// save(): atomic write (temp file + MoveFileEx replace) so a crash can
// never leave a half-written config.
struct Config {
    int version = 1;
    int countdownSeconds = 600;
    float volume = 2.0f;
    std::vector<std::wstring> ignoreWords;
    bool randomUi = true;
    bool startWithWindows = false;
    bool freezeChrome = true;
    int maxFreezeMinutes = 10;
};

class ConfigStore {
public:
    explicit ConfigStore(std::wstring path = defaultPath());

    // %USERPROFILE%\kotobacpp\config.json
    static std::wstring defaultPath();

    bool load();       // false only if the parent directory can't be created
    bool save() const;

    const Config& get() const { return config_; }
    void set(const Config& c) { config_ = c; }

private:
    std::wstring path_;
    Config config_;
};
