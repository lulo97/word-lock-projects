#pragma once

#include <map>
#include <string>

// Study progress: %USERPROFILE%\kotobacpp\progress.json (M1).
// attempts[wordKey] = how many times the word has been picked. The even
// picker reads this to never repeat a word until all have been picked once.
struct Progress {
    int version = 1;
    std::map<std::wstring, int> attempts;
};

// Same load/save semantics as ConfigStore: defaults on missing, .bak on
// corrupt, atomic write.
class ProgressStore {
public:
    explicit ProgressStore(std::wstring path = defaultPath());

    // %USERPROFILE%\kotobacpp\progress.json
    static std::wstring defaultPath();

    bool load();
    bool save() const;

    int attempts(const std::wstring& word) const;
    void record(const std::wstring& word);   // attempts[word]++
    void reset();                             // clear all attempts (in memory)

    const Progress& get() const { return progress_; }

private:
    std::wstring path_;
    Progress progress_;
};
