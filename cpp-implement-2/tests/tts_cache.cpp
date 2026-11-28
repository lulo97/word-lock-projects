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

void OpenCacheDatabase() {
    sqlite3* database = nullptr;
    REQUIRE(sqlite3_open(":memory:", &database) == SQLITE_OK);
    g_db = database;
    REQUIRE(ExecuteSql(
        "CREATE TABLE tts_cache ("
        "lang TEXT NOT NULL, voice TEXT NOT NULL, text TEXT NOT NULL, "
        "audio BLOB NOT NULL, byte_size INTEGER NOT NULL, "
        "created_at INTEGER NOT NULL, last_used_at INTEGER NOT NULL, "
        "hit_count INTEGER NOT NULL DEFAULT 0, "
        "PRIMARY KEY(lang, voice, text))"));
    REQUIRE(ExecuteSql("CREATE INDEX idx_tts_cache_last_used ON tts_cache(last_used_at)"));
}

int CountRows(const std::string& where) {
    sqlite3_stmt* statement = nullptr;
    const std::string sql = "SELECT COUNT(*) FROM tts_cache WHERE " + where;
    REQUIRE(sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
    const int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count;
}

int64_t IntField(const std::string& sql) {
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
    const int64_t value = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

std::vector<unsigned char> Blob(size_t size, unsigned char first) {
    std::vector<unsigned char> bytes(size);
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<unsigned char>(first + i);
    }
    return bytes;
}

}  // namespace

TEST_CASE("TTS cache round-trips audio and isolates lang, voice and text",
          "[unit][tts][cache]") {
    DatabaseGuard guard;
    OpenCacheDatabase();

    const auto hello = Blob(5, 1);
    const auto overwritten = Blob(3, 9);
    const auto unicode = Blob(4, 40);
    const std::string sakura = WideToUtf8(L"桜");
    const std::string vietnamese = WideToUtf8(L"tiếng Việt");

    std::vector<unsigned char> audio;
    REQUIRE_FALSE(TtsCacheGet("en", "female", "hello", audio));

    REQUIRE(TtsCachePut("en", "female", "hello", hello));
    REQUIRE(TtsCacheGet("en", "female", "hello", audio));
    REQUIRE(audio == hello);

    REQUIRE_FALSE(TtsCacheGet("ja", "female", "hello", audio));
    REQUIRE_FALSE(TtsCacheGet("en", "male", "hello", audio));
    REQUIRE_FALSE(TtsCacheGet("en", "female", "hello ", audio));

    REQUIRE(TtsCachePut("en", "female", "hello", overwritten));
    REQUIRE(TtsCacheGet("en", "female", "hello", audio));
    REQUIRE(audio == overwritten);

    REQUIRE_FALSE(TtsCachePut("en", "female", "empty", {}));
    REQUIRE(CountRows("text='empty'") == 0);

    REQUIRE(TtsCachePut("ja", "female", sakura, unicode));
    REQUIRE(TtsCacheGet("ja", "female", sakura, audio));
    REQUIRE(audio == unicode);

    REQUIRE(TtsCachePut("vi", "female", vietnamese, unicode));
    REQUIRE(TtsCacheGet("vi", "female", vietnamese, audio));
    REQUIRE(audio == unicode);
}

TEST_CASE("TTS cache touches LRU metadata and evicts least recently used rows",
          "[unit][tts][cache]") {
    DatabaseGuard guard;
    OpenCacheDatabase();

    const auto megabyte = Blob(1024 * 1024, 0);
    REQUIRE(TtsCachePut("en", "female", "first", megabyte));
    REQUIRE(TtsCachePut("en", "female", "second", megabyte));
    REQUIRE(TtsCachePut("en", "female", "third", megabyte));
    REQUIRE(TtsCacheTotalBytes() == 3LL * 1024 * 1024);

    REQUIRE(ExecuteSql("UPDATE tts_cache SET last_used_at=100 WHERE text='first'"));
    REQUIRE(ExecuteSql("UPDATE tts_cache SET last_used_at=200 WHERE text='second'"));
    REQUIRE(ExecuteSql("UPDATE tts_cache SET last_used_at=300 WHERE text='third'"));

    std::vector<unsigned char> audio;
    REQUIRE(TtsCacheGet("en", "female", "first", audio));
    REQUIRE(TtsCacheGet("en", "female", "first", audio));
    REQUIRE(IntField("SELECT hit_count FROM tts_cache WHERE text='first'") == 2);
    REQUIRE(IntField("SELECT last_used_at FROM tts_cache WHERE text='first'") > 100);

    TtsCacheEvict(2LL * 1024 * 1024);
    REQUIRE(CountRows("text='first'") == 1);
    REQUIRE(CountRows("text='second'") == 0);
    REQUIRE(CountRows("text='third'") == 1);
    REQUIRE(TtsCacheTotalBytes() <= 2LL * 1024 * 1024);

    REQUIRE(TtsCacheRemove("en", "female", "third"));
    REQUIRE_FALSE(TtsCacheGet("en", "female", "third", audio));
}
