#include "app.h"

namespace wordlock {

std::wstring GetRandomEnglishWordJson() {
    const std::vector<std::string> ignored = LowerUtf8List(g_settings.ignoreWords.english);
    std::string sql = "SELECT * FROM english_word";
    if (!ignored.empty()) {
        sql += " WHERE LOWER(word) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ")";
    }
    sql += " ORDER BY RANDOM() LIMIT 1";
    return QueryRowJson(sql, ignored, {"examples", "examples_vi"});
}

std::wstring GetEnglishWordJson(int id) {
    return QueryRowJson("SELECT * FROM english_word WHERE id = ?", {std::to_string(id)},
                        {"examples", "examples_vi"});
}

std::wstring GetRandomMcqJson() {
    return QueryRowJson("SELECT * FROM mcq ORDER BY RANDOM() LIMIT 1", {}, {"answers"});
}

std::wstring GetMcqJson(int id) {
    return QueryRowJson("SELECT * FROM mcq WHERE id = ?", {std::to_string(id)}, {"answers"});
}

std::wstring GetRandomJapaneseWordJson() {
    const std::vector<std::string> ignored = LowerUtf8List(g_settings.ignoreWords.japanese);
    std::string sql = "SELECT * FROM japanese_word";
    if (!ignored.empty()) {
        sql += " WHERE LOWER(kanji) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(hiragana) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(katakana) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(romaji) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ")";
    }
    sql += " ORDER BY RANDOM() LIMIT 1";

    std::vector<std::string> parameters;
    for (int repeat = 0; repeat < 4; ++repeat) {
        parameters.insert(parameters.end(), ignored.begin(), ignored.end());
    }
    return QueryRowJson(sql, parameters, {"examples", "examples_en", "examples_vi"});
}

std::wstring GetJapaneseWordJson(int id) {
    return QueryRowJson("SELECT * FROM japanese_word WHERE id = ?", {std::to_string(id)},
                        {"examples", "examples_en", "examples_vi"});
}

}  // namespace wordlock
