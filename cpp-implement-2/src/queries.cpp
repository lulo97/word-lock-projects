#include "app.h"

namespace wordlock {

std::wstring QueryRowJson(const std::string& sql,
                          const std::vector<std::string>& parameters,
                          const std::vector<std::string>& rawJsonColumns) {
    if (g_db == nullptr) {
        return L"null";
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return L"null";
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        sqlite3_bind_text(statement, static_cast<int>(i + 1), parameters[i].c_str(),
                          static_cast<int>(parameters[i].size()), SQLITE_TRANSIENT);
    }

    std::wstring json = L"null";
    if (sqlite3_step(statement) == SQLITE_ROW) {
        json = L"{";
        const int columns = sqlite3_column_count(statement);
        for (int i = 0; i < columns; ++i) {
            if (i != 0) {
                json += L",";
            }
            const char* name = sqlite3_column_name(statement, i);
            json += L"\"" + JsonEscape(Utf8ToWide(name == nullptr ? "" : name)) + L"\":";
            const unsigned char* text = sqlite3_column_text(statement, i);
            const std::string value = text == nullptr ? "" : reinterpret_cast<const char*>(text);
            if (sqlite3_column_type(statement, i) == SQLITE_NULL) {
                json += L"null";
            } else if (std::find(rawJsonColumns.begin(), rawJsonColumns.end(),
                                 name == nullptr ? "" : name) != rawJsonColumns.end()) {
                json += Utf8ToWide(value);
            } else if (sqlite3_column_type(statement, i) == SQLITE_INTEGER ||
                       sqlite3_column_type(statement, i) == SQLITE_FLOAT) {
                json += Utf8ToWide(value);
            } else {
                json += L"\"" + JsonEscape(Utf8ToWide(value)) + L"\"";
            }
        }
        json += L"}";
    }
    sqlite3_finalize(statement);
    return json;
}

std::wstring LowerWide(const std::wstring& value) {
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return out;
}

std::vector<std::string> LowerUtf8List(const std::vector<std::wstring>& values) {
    std::vector<std::string> out;
    for (const std::wstring& value : values) {
        const std::wstring lowered = LowerWide(value);
        if (!lowered.empty()) {
            out.push_back(WideToUtf8(lowered));
        }
    }
    return out;
}

}  // namespace wordlock
