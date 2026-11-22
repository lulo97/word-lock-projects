// word_db.cpp
//
// Word database: load embedded JSON, parse, fill HTML templates, pick random.

#include "word_db.h"
#include "core/utf.h"
#include "debug_log.h"
#include "embedded_assets.h"
#include "json.hpp"

#include <random>
#include <string_view>
#include <unordered_map>

namespace {

std::wstring get_wstring(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_string())
        return L"";
    return ui::utf8ToWide(j[key].get<std::string>());
}

} // namespace

std::string load_asset_bytes(const std::string& path) {
    if (const assets::EmbeddedAsset* asset = assets::find_asset(path)) {
        return std::string(reinterpret_cast<const char*>(asset->data), asset->size);
    }
    return {};
}

WordData parse_word_json(const std::string& json) {
    WordData w{};
    auto j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded() || !j.is_object())
        return w;

    w.kanji = get_wstring(j, "kanji");
    w.hiragana = get_wstring(j, "hiragana");
    w.romaji = get_wstring(j, "romaji");
    w.pos = get_wstring(j, "pos");
    w.meaning_ja = get_wstring(j, "meaning_ja");
    w.meaning_ja_romaji = get_wstring(j, "meaning_ja_romaji");
    w.meaning_vi = get_wstring(j, "meaning_vi");
    w.audio = get_wstring(j, "audio");

    if (j.contains("examples") && j["examples"].is_array()) {
        for (const auto& item : j["examples"]) {
            if (!item.is_object())
                continue;
            w.examples.push_back({
                get_wstring(item, "ja"),
                get_wstring(item, "romaji"),
                get_wstring(item, "vi")
            });
        }
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& tag : j["tags"]) {
            if (tag.is_string())
                w.tags.push_back(ui::utf8ToWide(tag.get<std::string>()));
        }
    }

    return w;
}

std::wstring fill_template(const std::wstring& html, const WordData& w) {
    std::unordered_map<std::wstring, std::wstring> values = {
        {L"kanji", w.kanji},
        {L"hiragana", w.hiragana},
        {L"romaji", w.romaji},
        {L"pos", w.pos},
        {L"meaning_ja", w.meaning_ja},
        {L"meaning_ja_romaji", w.meaning_ja_romaji},
        {L"meaning_vi", w.meaning_vi},
        {L"audio", w.audio},
    };

    for (std::size_t idx = 0; idx < w.examples.size(); ++idx) {
        const std::wstring prefix = L"example" + std::to_wstring(idx + 1);
        values[prefix + L"_ja"] = w.examples[idx].ja;
        values[prefix + L"_romaji"] = w.examples[idx].romaji;
        values[prefix + L"_vi"] = w.examples[idx].vi;
    }

    for (std::size_t idx = 0; idx < w.tags.size(); ++idx) {
        values[L"tag" + std::to_wstring(idx + 1)] = w.tags[idx];
    }

    std::wstring result;
    result.reserve(html.size() + 256);

    for (std::wstring::size_type i = 0; i < html.size(); ) {
        if (i + 1 < html.size() && html[i] == L'{' && html[i + 1] == L'{') {
            const auto close = html.find(L"}}", i + 2);
            if (close != std::wstring::npos) {
                const std::wstring key = html.substr(i + 2, close - i - 2);
                if (const auto it = values.find(key); it != values.end()) {
                    result += it->second;
                    i = close + 2;
                    continue;
                }
            }
        }
        result.push_back(html[i]);
        ++i;
    }

    // Unmatched {{key}} placeholders stay in the page: log them so a
    // template/JSON field mismatch is visible instead of showing raw text.
    std::wstring::size_type pos = result.find(L"{{");
    while (pos != std::wstring::npos) {
        UI_LOG("app", L"unresolved placeholder near offset %zu: '%ls'",
               pos, result.substr(pos, 40).c_str());
        pos = result.find(L"{{", pos + 2);
    }

    return result;
}

WordData pick_random_word() {
    constexpr std::string_view kPrefix = "database/words/";
    constexpr std::string_view kSuffix = ".json";

    std::vector<std::string> wordPaths;
    for (std::size_t i = 0; i < assets::kAssetCount; ++i) {
        const auto& asset = assets::kAssets[i];
        const std::string_view path = asset.relative_path;
        if (path.size() > kPrefix.size() + kSuffix.size() &&
            path.substr(0, kPrefix.size()) == kPrefix &&
            path.substr(path.size() - kSuffix.size()) == kSuffix) {
            wordPaths.emplace_back(path);
        }
    }

    if (wordPaths.empty())
        return {};

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, wordPaths.size() - 1);
    return parse_word_json(load_asset_bytes(wordPaths[dist(rng)]));
}
