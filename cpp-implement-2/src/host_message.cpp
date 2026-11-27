#include "app.h"

namespace wordlock {

void HandleHostMessage(ICoreWebView2* sender, const std::wstring& message) {
    std::wstring type;
    if (!TryGetStringField(message, L"type", type)) {
        return;
    }
    AppendTestMessageLog(type);
    std::wstring requestId;
    TryGetStringField(message, L"id", requestId);

    ReplyFn reply{[sender, requestId](const std::wstring& result,
                                      const std::wstring& error) {
        SendHostReply(sender, requestId, result, error);
    }};
    std::wstring text;
    int number = -1;
    if (TryGetStringField(message, L"value", text) ||
        TryGetStringField(message, L"idValue", text)) {
        try {
            number = std::stoi(text);
        } catch (...) {
        }
    }

    if (!HandleContentMessage(type, message, reply, number) &&
        !HandleIgnoreWordsMessage(type, message, reply)) {
        HandleAppMessage(type, message, reply);
    }
}

}  // namespace wordlock
