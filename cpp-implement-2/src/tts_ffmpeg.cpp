#include "app.h"

#include <atomic>
#include <chrono>

namespace wordlock {
namespace {

std::atomic<uint64_t> g_tempSequence{0};

}  // namespace

TtsTempFiles::~TtsTempFiles() {
    std::error_code ignored;
    std::filesystem::remove(input, ignored);
    std::filesystem::remove(output, ignored);
}

TtsTempFiles MakeTtsTempFiles(const wchar_t* inputExtension,
                              const wchar_t* outputExtension) {
    const uint64_t stamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::wstring stem = L"wordlock_tts_" + std::to_wstring(GetCurrentProcessId()) +
                              L"_" + std::to_wstring(stamp) + L"_" +
                              std::to_wstring(g_tempSequence.fetch_add(1));
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    return {directory / (stem + inputExtension), directory / (stem + outputExtension)};
}

bool WriteTtsTempFile(const std::filesystem::path& path,
                      const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

bool ReadTtsTempFile(const std::filesystem::path& path,
                     std::vector<unsigned char>& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    return !bytes.empty();
}

std::wstring QuoteTtsPath(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

bool RunTtsFfmpeg(const std::wstring& arguments) {
    wchar_t executable[MAX_PATH]{};
    const DWORD length = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH,
                                     executable, nullptr);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    const std::wstring application(executable, length);
    const std::wstring command = L"\"" + application + L"\" " + arguments;
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(application.c_str(), commandLine.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &process)) {
        return false;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

}  // namespace wordlock
