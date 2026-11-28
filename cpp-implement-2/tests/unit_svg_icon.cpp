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

constexpr char kRunIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><circle cx="40" cy="16" r="6" fill="#2563eb"/><path d="M18 50l10-14 10 4 8-12M28 36l-8-10 12-6" fill="none" stroke="#2563eb" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/></svg>)SVG";
constexpr char kAppleIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><circle cx="32" cy="36" r="20" fill="#dc2626"/><path d="M32 16c0-6 4-10 10-12" fill="none" stroke="#7c2d12" stroke-width="4" stroke-linecap="round"/></svg>)SVG";
constexpr char kCatIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M14 27l3-14 12 8h6l12-8 3 14c3 13-6 23-18 23S11 40 14 27z" fill="#f59e0b"/><circle cx="25" cy="34" r="3" fill="#1f2937"/><circle cx="39" cy="34" r="3" fill="#1f2937"/></svg>)SVG";
constexpr char kHashIcon[] =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M24 10l-4 44M44 10l-4 44M12 25h40M10 41h40" fill="none" stroke="#2563eb" stroke-width="5" stroke-linecap="round"/></svg>)SVG";

}  // namespace

TEST_CASE("Legacy word tables gain svg_icon with an empty default",
          "[unit][database][svg-icon]") {
    DatabaseGuard guard;
    sqlite3* database = OpenInMemoryDatabase();

    REQUIRE(ExecuteSql("CREATE TABLE english_word (id INTEGER PRIMARY KEY, word TEXT NOT NULL)"));
    REQUIRE(ExecuteSql("CREATE TABLE japanese_word (id INTEGER PRIMARY KEY, kanji TEXT NOT NULL)"));
    REQUIRE(ExecuteSql("INSERT INTO english_word (id, word) VALUES (7, 'run')"));
    REQUIRE(ExecuteSql("INSERT INTO japanese_word (id, kanji) VALUES (7, '猫')"));

    std::wstring svgIcon;
    REQUIRE_FALSE(TableHasColumn(database, "english_word", "svg_icon"));
    REQUIRE_FALSE(TryGetStringField(GetEnglishWordJson(7), L"svg_icon", svgIcon));

    REQUIRE(EnsureColumn("english_word", "svg_icon", "TEXT NOT NULL DEFAULT ''"));
    REQUIRE(EnsureColumn("japanese_word", "svg_icon", "TEXT NOT NULL DEFAULT ''"));
    REQUIRE(TableHasColumn(database, "english_word", "svg_icon"));
    REQUIRE(TableHasColumn(database, "japanese_word", "svg_icon"));
    REQUIRE(EnsureColumn("english_word", "svg_icon", "TEXT NOT NULL DEFAULT ''"));

    REQUIRE(TryGetStringField(GetEnglishWordJson(7), L"svg_icon", svgIcon));
    REQUIRE(svgIcon.empty());
    REQUIRE(TryGetStringField(GetJapaneseWordJson(7), L"svg_icon", svgIcon));
    REQUIRE(svgIcon.empty());
}

TEST_CASE("Word queries expose the stored SVG icon", "[unit][database][svg-icon]") {
    DatabaseGuard databaseGuard;
    SettingsGuard settingsGuard;
    OpenInMemoryDatabase();
    g_settings.ignoreWords = IgnoreWords{};

    REQUIRE(ExecuteSql(
        "CREATE TABLE english_word (id INTEGER PRIMARY KEY, word TEXT NOT NULL, "
        "svg_icon TEXT NOT NULL DEFAULT '')"));
    REQUIRE(ExecuteSql(
        "CREATE TABLE japanese_word (id INTEGER PRIMARY KEY, kanji TEXT NOT NULL, "
        "hiragana TEXT NOT NULL DEFAULT '', katakana TEXT NOT NULL DEFAULT '', "
        "romaji TEXT NOT NULL DEFAULT '', svg_icon TEXT NOT NULL DEFAULT '')"));
    REQUIRE(ExecuteSql(
        "CREATE TABLE word_show_progress (language TEXT NOT NULL, word_id INTEGER NOT NULL, "
        "show_count INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(language, word_id))"));

    char* insert = sqlite3_mprintf(
        "INSERT INTO english_word (id, word, svg_icon) VALUES (1, 'run', %Q), "
        "(2, 'apple', %Q)", kRunIcon, kAppleIcon);
    REQUIRE(ExecuteSql(insert));
    sqlite3_free(insert);

    insert = sqlite3_mprintf(
        "INSERT INTO japanese_word (id, kanji, hiragana, katakana, romaji, svg_icon) "
        "VALUES (1, '猫', 'ねこ', 'ネコ', 'neko', %Q), "
        "(2, '番号', 'ばんごう', 'バンゴウ', 'bangou', %Q)", kCatIcon, kHashIcon);
    REQUIRE(ExecuteSql(insert));
    sqlite3_free(insert);

    std::wstring svgIcon;
    REQUIRE(TryGetStringField(GetEnglishWordJson(1), L"svg_icon", svgIcon));
    REQUIRE(svgIcon == Utf8ToWide(kRunIcon));
    REQUIRE(svgIcon.find(L"<text") == std::wstring::npos);
    REQUIRE(TryGetStringField(GetJapaneseWordJson(2), L"svg_icon", svgIcon));
    REQUIRE(svgIcon == Utf8ToWide(kHashIcon));
    REQUIRE(svgIcon.find(L"<text") == std::wstring::npos);

    g_settings.ignoreWords.english.push_back(L"run");
    g_settings.ignoreWords.japanese.push_back(L"neko");
    REQUIRE(TryGetStringField(GetRandomEnglishWordJson(), L"svg_icon", svgIcon));
    REQUIRE(svgIcon == Utf8ToWide(kAppleIcon));
    REQUIRE(TryGetStringField(GetRandomJapaneseWordJson(), L"svg_icon", svgIcon));
    REQUIRE(svgIcon == Utf8ToWide(kHashIcon));
}
