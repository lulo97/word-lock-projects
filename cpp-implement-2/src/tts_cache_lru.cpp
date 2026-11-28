#include "app.h"

namespace wordlock {

bool TtsCacheRemove(const std::string& lang, const std::string& voice,
                    const std::string& text) {
    if (g_db == nullptr) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db,
                           "DELETE FROM tts_cache WHERE lang=? AND voice=? AND text=?",
                           -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, lang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, voice.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

int64_t TtsCacheTotalBytes() {
    if (g_db == nullptr) {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    int64_t total = 0;
    if (sqlite3_prepare_v2(g_db,
                           "SELECT COALESCE(SUM(byte_size),0) FROM tts_cache",
                           -1, &statement, nullptr) == SQLITE_OK) {
        if (sqlite3_step(statement) == SQLITE_ROW) {
            total = static_cast<int64_t>(sqlite3_column_int64(statement, 0));
        }
        sqlite3_finalize(statement);
    }
    return total;
}

void TtsCacheEvict(int64_t maxBytes) {
    while (TtsCacheTotalBytes() > maxBytes) {
        const int64_t before = TtsCacheTotalBytes();
        if (!ExecuteSql("DELETE FROM tts_cache WHERE rowid = ("
                        "SELECT rowid FROM tts_cache "
                        "ORDER BY last_used_at ASC LIMIT 1)")) {
            break;
        }
        if (TtsCacheTotalBytes() >= before) {
            break;  // No progress: avoid spinning on an unexpected schema state.
        }
    }
}

}  // namespace wordlock
