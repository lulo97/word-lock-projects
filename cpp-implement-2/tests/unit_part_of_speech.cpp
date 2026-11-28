#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

namespace {

struct DatabaseGuard {
    sqlite3* previous = g_db;

    ~DatabaseGuard() {
        if (g_db != nullptr && g_db != previous) {
            sqlite3_close(g_db);
        }
        g_db = previous;
    }
};

struct SettingsGuard {
    AppSettings previous = g_settings;

    ~SettingsGuard() { g_settings = previous; }
};

sqlite3* OpenInMemoryDatabase() {
    sqlite3* database = nullptr;
    REQUIRE(sqlite3_open(":memory:", &database) == SQLITE_OK);
    g_db = database;
    return database;
}

bool TableHasColumn(sqlite3* database, const char* table, const char* column) {
    const std::string sql = std::string("PRAGMA table_info(") + table + ")";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(statement, 1);
        if (name != nullptr && strcmp(reinterpret_cast<const char*>(name), column) == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    return found;
}

}  // namespace

TEST_CASE("Legacy word tables gain part_of_speech with a noun default",
          "[unit][database][part-of-speech]") {
    DatabaseGuard guard;
    sqlite3* database = OpenInMemoryDatabase();

    REQUIRE(ExecuteSql("CREATE TABLE english_word (id INTEGER PRIMARY KEY, word TEXT NOT NULL)"));
    REQUIRE(ExecuteSql("CREATE TABLE japanese_word (id INTEGER PRIMARY KEY, kanji TEXT NOT NULL)"));
    REQUIRE(ExecuteSql("INSERT INTO english_word (id, word) VALUES (7, 'run')"));
    REQUIRE(ExecuteSql("INSERT INTO japanese_word (id, kanji) VALUES (7, '猫')"));

    std::wstring partOfSpeech;
    REQUIRE_FALSE(TableHasColumn(database, "english_word", "part_of_speech"));
    REQUIRE_FALSE(TryGetStringField(GetEnglishWordJson(7), L"part_of_speech", partOfSpeech));

    REQUIRE(EnsureColumn("english_word", "part_of_speech", "TEXT NOT NULL DEFAULT 'noun'"));
    REQUIRE(EnsureColumn("japanese_word", "part_of_speech", "TEXT NOT NULL DEFAULT 'noun'"));
    REQUIRE(TableHasColumn(database, "english_word", "part_of_speech"));
    REQUIRE(TableHasColumn(database, "japanese_word", "part_of_speech"));
    // Running startup twice must not add the column again or fail.
    REQUIRE(EnsureColumn("english_word", "part_of_speech", "TEXT NOT NULL DEFAULT 'noun'"));

    REQUIRE(TryGetStringField(GetEnglishWordJson(7), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"noun");
    REQUIRE(TryGetStringField(GetJapaneseWordJson(7), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"noun");
}

TEST_CASE("Word queries expose the stored part of speech",
          "[unit][database][part-of-speech]") {
    DatabaseGuard databaseGuard;
    SettingsGuard settingsGuard;
    OpenInMemoryDatabase();
    g_settings.ignoreWords = IgnoreWords{};

    REQUIRE(ExecuteSql(
        "CREATE TABLE english_word (id INTEGER PRIMARY KEY, word TEXT NOT NULL, "
        "part_of_speech TEXT NOT NULL DEFAULT 'noun')"));
    REQUIRE(ExecuteSql(
        "CREATE TABLE japanese_word (id INTEGER PRIMARY KEY, kanji TEXT NOT NULL, "
        "hiragana TEXT NOT NULL DEFAULT '', katakana TEXT NOT NULL DEFAULT '', "
        "romaji TEXT NOT NULL DEFAULT '', part_of_speech TEXT NOT NULL DEFAULT 'noun')"));
    REQUIRE(ExecuteSql(
        "CREATE TABLE word_show_progress (language TEXT NOT NULL, word_id INTEGER NOT NULL, "
        "show_count INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(language, word_id))"));
    REQUIRE(ExecuteSql(
        "INSERT INTO english_word (id, word, part_of_speech) VALUES "
        "(1, 'run', 'verb'), (2, 'apple', 'noun')"));
    REQUIRE(ExecuteSql(
        "INSERT INTO japanese_word (id, kanji, hiragana, katakana, romaji, part_of_speech) "
        "VALUES (1, '猫', 'ねこ', 'ネコ', 'neko', 'noun'), "
        "(2, '走る', 'はしる', 'ハシル', 'hashiru', 'verb')"));

    std::wstring partOfSpeech;
    REQUIRE(TryGetStringField(GetEnglishWordJson(1), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"verb");
    REQUIRE(TryGetStringField(GetJapaneseWordJson(2), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"verb");

    g_settings.ignoreWords.english.push_back(L"run");
    g_settings.ignoreWords.japanese.push_back(L"neko");
    REQUIRE(TryGetStringField(GetRandomEnglishWordJson(), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"noun");
    REQUIRE(TryGetStringField(GetRandomJapaneseWordJson(), L"part_of_speech", partOfSpeech));
    REQUIRE(partOfSpeech == L"verb");
}
