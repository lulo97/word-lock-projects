#include "app.h"

#include <ctime>

namespace wordlock {

std::wstring NormalizeTtsText(const std::wstring& text) {
    return Trim(text);
}

bool TtsCacheGet(const std::string& lang, const std::string& voice,
                  const std::string& text, std::vector<unsigned char>& audio) {
    if (g_db == nullptr) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db,
                           "SELECT audio FROM tts_cache "
                           "WHERE lang=? AND voice=? AND text=?",
                           -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, lang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, voice.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, text.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(statement, 0);
        const int size = sqlite3_column_bytes(statement, 0);
        if (blob != nullptr && size > 0) {
            const auto* bytes = static_cast<const unsigned char*>(blob);
            audio.assign(bytes, bytes + size);
            found = true;
        }
    }
    sqlite3_finalize(statement);

    if (found) {
        sqlite3_stmt* update = nullptr;
        if (sqlite3_prepare_v2(g_db,
                               "UPDATE tts_cache SET last_used_at=?, hit_count=hit_count+1 "
                               "WHERE lang=? AND voice=? AND text=?",
                               -1, &update, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(update, 1, static_cast<sqlite3_int64>(std::time(nullptr)));
            sqlite3_bind_text(update, 2, lang.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(update, 3, voice.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(update, 4, text.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(update);
            sqlite3_finalize(update);
        }
    }
    return found;
}

bool TtsCachePut(const std::string& lang, const std::string& voice,
                  const std::string& text, const std::vector<unsigned char>& audio) {
    if (g_db == nullptr || audio.empty()) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db,
                           "INSERT INTO tts_cache "
                           "(lang, voice, text, audio, byte_size, created_at, last_used_at, "
                           "hit_count) VALUES (?,?,?,?,?,?,?,0) "
                           "ON CONFLICT(lang, voice, text) DO UPDATE SET "
                           "audio=excluded.audio, byte_size=excluded.byte_size, "
                           "last_used_at=excluded.last_used_at",
                           -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    const sqlite3_int64 now = static_cast<sqlite3_int64>(std::time(nullptr));
    sqlite3_bind_text(statement, 1, lang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, voice.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(statement, 4, audio.data(), static_cast<int>(audio.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 5, static_cast<sqlite3_int64>(audio.size()));
    sqlite3_bind_int64(statement, 6, now);
    sqlite3_bind_int64(statement, 7, now);

    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (ok) {
        TtsCacheEvict(kTtsCacheMaxBytes);
    }
    return ok;
}

}  // namespace wordlock
