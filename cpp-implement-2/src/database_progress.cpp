#include "app.h"

namespace wordlock {

void RecordWordShow(const std::string& language, int wordId) {
    if (g_db == nullptr || wordId < 0) {
        return;
    }
    const char* value = language == "japanese" ? "japanese" : "english";
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO word_show_progress (language, word_id, show_count) "
        "VALUES (?, ?, 1) ON CONFLICT(language, word_id) "
        "DO UPDATE SET show_count = show_count + 1";
    if (sqlite3_prepare_v2(g_db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(statement, 1, value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, wordId);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

std::wstring GetWordProgressJson() {
    std::wstring json = L"[";
    if (g_db != nullptr) {
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "SELECT language, word_id, show_count FROM word_show_progress "
            "ORDER BY language, word_id";
        if (sqlite3_prepare_v2(g_db, sql, -1, &statement, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(statement) == SQLITE_ROW) {
                const unsigned char* language = sqlite3_column_text(statement, 0);
                const std::wstring name =
                    language != nullptr ? Utf8ToWide(reinterpret_cast<const char*>(language))
                                        : std::wstring();
                if (!first) {
                    json += L",";
                }
                first = false;
                json += L"{\"language\":" + JsLiteral(name) + L",\"wordId\":" +
                        std::to_wstring(sqlite3_column_int(statement, 1)) +
                        L",\"showCount\":" +
                        std::to_wstring(sqlite3_column_int(statement, 2)) + L"}";
            }
            sqlite3_finalize(statement);
        }
    }
    return json + L"]";
}

std::wstring GetDatabaseInfoJson() {
    bool inMemory = false;
    if (g_db != nullptr) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(g_db, "PRAGMA database_list", -1, &statement, nullptr) ==
            SQLITE_OK) {
            while (sqlite3_step(statement) == SQLITE_ROW) {
                const unsigned char* file = sqlite3_column_text(statement, 2);
                if (file == nullptr || file[0] == '\0') {
                    inMemory = true;
                }
            }
            sqlite3_finalize(statement);
        }
    }
    return std::wstring(L"{\"available\":") + BoolLiteral(g_db != nullptr) +
           L",\"inMemory\":" + BoolLiteral(inMemory) + L"}";
}

}  // namespace wordlock
