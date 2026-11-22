#pragma once

#include <map>
#include <string>
#include <vector>

// One example sentence. Old format: ja/romaji/vi. New format (M2 generator):
// jp/reading/romaji/en/audio.
struct ExampleSentence {
    std::wstring ja;
    std::wstring romaji;
    std::wstring vi;
    std::wstring jp;        // new format (== ja)
    std::wstring reading;   // new format
    std::wstring en;        // new format
    std::wstring audio;     // new format
};

// Parsed word entry from database/words/<word>.json. Supports both the old
// format (kanji/hiragana/pos/...) and the M2 generator format
// (word/reading/meaning/type/audio + examples with jp/reading/en/audio).
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
    // New format (M2 generator):
    std::wstring word;      // == kanji
    std::wstring reading;   // == hiragana
    std::wstring meaning;   // English
    std::wstring type;      // e.g. "i-adjective"
};

// Canonical key for a word: `word` if present, else `kanji`. This is the
// key used by progress.json attempts and config ignoreWords.
std::wstring wordKey(const WordData& w);

// Light startup check (M4): returns an empty string for a usable word, or a
// short reason ("missing word", "romaji contains Japanese", ...). The picker
// excludes words with a non-empty issue and logs it as `db`.
std::wstring wordIssue(const WordData& w);

// Load an embedded asset by its relative path (e.g. "htmls/info.html").
// Returns empty string if not found.
std::string load_asset_bytes(const std::string& path);

// Parse a word JSON string into WordData. Returns empty WordData on failure.
WordData parse_word_json(const std::string& json);

// Number of words in the embedded database/words/*.json.
size_t count_words();

// Fill {{key}} placeholders in HTML with word data.
std::wstring fill_template(const std::wstring& html, const WordData& word);

// Pick a random word from embedded database/words/*.json.
WordData pick_random_word();

// Even picker (M1): among non-ignored words, pick the one with the lowest
// attempt count (random tie-break). `ignored` comes from Config.ignoreWords,
// `attempts` from Progress (wordKey -> count). A word is never picked twice
// until every non-ignored word has been picked once. `extra` is the test/
// extension seam (app add-word); invalid entries are filtered here too.
WordData pick_even_word(const std::vector<std::wstring>& ignored,
                        const std::map<std::wstring, int>& attempts,
                        const std::vector<WordData>& extra = {});
