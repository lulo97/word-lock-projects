#include "app.h"

namespace wordlock {

void InitializeDatabase() {
    const std::string path = IsTestDataMode()
                                 ? ":memory:"
                                 : WideToUtf8(GetDataDirectory() + L"pomodoro_lock_cpp.db");
    if (sqlite3_open(path.c_str(), &g_db) != SQLITE_OK) {
        g_db = nullptr;
        return;
    }

    ExecuteSql("CREATE TABLE IF NOT EXISTS english_word ("
               "id INTEGER PRIMARY KEY, word TEXT NOT NULL, meaning_vi TEXT NOT NULL, "
               "examples TEXT NOT NULL, examples_vi TEXT NOT NULL DEFAULT '[]')");
    ExecuteSql("CREATE TABLE IF NOT EXISTS mcq ("
               "id INTEGER PRIMARY KEY, question TEXT NOT NULL, answers TEXT NOT NULL, "
               "correct_answer_idx INTEGER NOT NULL, explanation TEXT NOT NULL)");
    ExecuteSql("CREATE TABLE IF NOT EXISTS japanese_word ("
               "id INTEGER PRIMARY KEY, kanji TEXT NOT NULL, hiragana TEXT NOT NULL, "
               "katakana TEXT NOT NULL, romaji TEXT NOT NULL DEFAULT '', meaning_en TEXT NOT NULL, "
               "examples TEXT NOT NULL, examples_en TEXT NOT NULL DEFAULT '[]', "
               "examples_vi TEXT NOT NULL DEFAULT '[]')");

    EnsureColumn("english_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'");
    EnsureColumn("japanese_word", "romaji", "TEXT NOT NULL DEFAULT ''");
    EnsureColumn("japanese_word", "examples_en", "TEXT NOT NULL DEFAULT '[]'");
    EnsureColumn("japanese_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'");

    auto runSeed = [](const char* sql) {
        char* error = nullptr;
        const int result = sqlite3_exec(g_db, sql, nullptr, nullptr, &error);
        if (error != nullptr) {
            sqlite3_free(error);
        }
        (void)result;
    };

    char* sql = sqlite3_mprintf(
        "INSERT INTO english_word (id, word, meaning_vi, examples, examples_vi) VALUES "
        "(0, 'apple', 'quả táo', "
        "'[\"he has an apple\",\"apple is a popular company\",\"an apple a day keep the doctor away\"]', "
        "'[\"anh ấy có một quả táo\",\"Apple là một công ty nổi tiếng\","
        "\"một quả táo mỗi ngày giúp không cần gặp bác sĩ\"]') "
        "ON CONFLICT(id) DO UPDATE SET word=excluded.word, meaning_vi=excluded.meaning_vi, "
        "examples=excluded.examples, examples_vi=excluded.examples_vi");
    runSeed(sql);
    sqlite3_free(sql);

    runSeed("INSERT OR IGNORE INTO mcq (id, question, answers, correct_answer_idx, explanation) "
            "VALUES (0, '1+1=?', '[\"1\",\"2\",\"3\",\"4\"]', 1, '1+1=2')");

    sql = sqlite3_mprintf(
        "INSERT INTO japanese_word "
        "(id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi) "
        "VALUES (0, '桜', 'さくら', 'サクラ', 'sakura', 'cherry blossom', "
        "'[\"桜が咲きました。\",\"春に桜を見ます。\"]', "
        "'[\"The cherry blossoms bloomed.\",\"I see cherry blossoms in spring.\"]', "
        "'[\"Hoa anh đào đã nở.\",\"Tôi nhìn hoa anh đào vào mùa xuân.\"]') "
        "ON CONFLICT(id) DO UPDATE SET kanji=excluded.kanji, hiragana=excluded.hiragana, "
        "katakana=excluded.katakana, romaji=excluded.romaji, meaning_en=excluded.meaning_en, "
        "examples=excluded.examples, examples_en=excluded.examples_en, "
        "examples_vi=excluded.examples_vi");
    runSeed(sql);
    sqlite3_free(sql);
}

}  // namespace wordlock
