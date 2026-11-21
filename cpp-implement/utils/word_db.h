#pragma once

#include <string>
#include <vector>

// One example sentence (Japanese, romaji, Vietnamese).
struct ExampleSentence {
    std::wstring ja;
    std::wstring romaji;
    std::wstring vi;
};

// Parsed word entry from database/words/<word>.json.
struct WordData {
    std::wstring kanji;
    std::wstring hiragana;
    std::wstring romaji;
    std::wstring pos;
    std::wstring meaning_ja;
    std::wstring meaning_ja_romaji;
    std::wstring meaning_vi;
    std::vector<ExampleSentence> examples;
    std::vector<std::wstring> tags;
    std::wstring audio;
};

// Load an embedded asset by its relative path (e.g. "htmls/info.html").
// Returns empty string if not found.
std::string load_asset_bytes(const std::string& path);

// Parse a word JSON string into WordData. Returns empty WordData on failure.
WordData parse_word_json(const std::string& json);

// Fill {{key}} placeholders in HTML with word data.
std::wstring fill_template(const std::wstring& html, const WordData& word);

// Pick a random word from embedded database/words/*.json.
WordData pick_random_word();
