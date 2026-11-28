#include "app.h"

namespace wordlock {

bool FetchTtsWav(const std::wstring& text, const std::wstring& lang,
                 std::vector<unsigned char>& wav, std::wstring& error) {
    error = kTtsUnavailableMessage;
    wav.clear();

    const std::wstring mock = GetEnvironmentValue(kTtsMockEnv);
    if (!mock.empty()) {
        AppendTestMessageLog(L"tts:network");
        if (mock == L"fail" || !TryGetMockTtsWav(wav)) {
            return false;
        }
        error.clear();
        return true;
    }

    HINTERNET session = WinHttpOpen(L"PomodoroLockCpp TTS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 300000, 300000);

    HINTERNET connect = WinHttpConnect(session, kTtsHost, kTtsPort, 0);
    if (connect == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"POST", kTtsPath, nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (request == nullptr) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::string body = "{\"input\":" + WideToUtf8(JsLiteral(text)) +
                             ",\"lang\":" + WideToUtf8(JsLiteral(lang)) +
                             ",\"voice\":\"" + kTtsVoice + "\"}";
    const DWORD bodySize = static_cast<DWORD>(body.size());
    WinHttpAddRequestHeaders(request, L"Content-Type: application/json\r\n",
                             static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    if (!WinHttpSendRequest(request, nullptr, 0,
                            reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
                            bodySize, bodySize, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_NO_HEADER_INDEX, &status, &statusSize, nullptr);
    if (status < 200 || status >= 300) {
        error = std::wstring(kTtsUnavailableMessage) + L" (HTTP " + std::to_wstring(status) + L")";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    AppendTestMessageLog(L"tts:network");

    std::vector<unsigned char> responseBytes;
    std::vector<unsigned char> buffer(16384);
    while (true) {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read) ||
            read == 0) {
            break;
        }
        responseBytes.insert(responseBytes.end(), buffer.begin(), buffer.begin() + read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (responseBytes.empty()) {
        return false;
    }
    wav = std::move(responseBytes);
    error.clear();
    return true;
}

}  // namespace wordlock
