#include "app.h"

namespace wordlock {

bool ExecuteSql(const char* sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(g_db, sql, nullptr, nullptr, &error);
    if (error != nullptr) {
        sqlite3_free(error);
    }
    return result == SQLITE_OK;
}

bool EnsureColumn(const char* table, const char* column, const char* definition) {
    const std::string sql = std::string("PRAGMA table_info(") + table + ")";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
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
    if (found) {
        return true;
    }
    const std::string alter = std::string("ALTER TABLE ") + table + " ADD COLUMN " + column + " " + definition;
    return ExecuteSql(alter.c_str());
}

}  // namespace wordlock
