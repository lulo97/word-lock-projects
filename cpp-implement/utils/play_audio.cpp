#include "play_audio.h"

#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cwchar>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr const wchar_t* AUDIO_ALIAS = L"cpp_play_audio";

struct AudioState {
    std::mutex mutex;
    std::thread worker;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> playing{false};

    ~AudioState() {
        stopRequested.store(true);

        if (worker.joinable())
            worker.join();
    }
};

AudioState& audio_state() {
    static AudioState state;
    return state;
}

std::wstring absolute_path(const std::wstring& path) {
    DWORD needed = GetFullPathNameW(
        path.c_str(), 0, nullptr, nullptr
    );

    if (needed == 0)
        return path;

    std::vector<wchar_t> buffer(needed);

    DWORD length = GetFullPathNameW(
        path.c_str(),
        needed,
        buffer.data(),
        nullptr
    );

    if (length == 0 || length >= needed)
        return path;

    return std::wstring(buffer.data(), length);
}

bool has_wav_extension(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    size_t slash = path.find_last_of(L"\\/");

    if (dot == std::wstring::npos)
        return false;

    if (slash != std::wstring::npos && dot < slash)
        return false;

    return _wcsicmp(path.c_str() + dot, L".wav") == 0;
}

std::wstring make_temp_wav_path() {
    wchar_t tempDir[MAX_PATH]{};

    DWORD length = GetTempPathW(MAX_PATH, tempDir);

    if (length == 0 || length >= MAX_PATH)
        return L"";

    wchar_t tempFile[MAX_PATH]{};

    if (!GetTempFileNameW(tempDir, L"cppaud", 0, tempFile))
        return L"";

    return tempFile;
}

bool run_ffmpeg(
    const std::wstring& inputPath,
    const std::wstring& outputPath,
    float volume
) {
    // Quote paths; Windows file names cannot contain double quotes.
    std::wstring filter = L"volume=";
    wchar_t gain[32]{};
    swprintf(gain, 32, L"%g", static_cast<double>(volume));
    filter += gain;

    std::wstring commandLine =
        L"ffmpeg.exe -hide_banner -loglevel error -y -i \"" +
        inputPath +
        L"\" -vn -af \"" + filter +
        L"\" -f wav -acodec pcm_s16le \"" + outputPath + L"\"";

    std::vector<wchar_t> command(
        commandLine.begin(), commandLine.end()
    );
    command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process)) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(
        process.hProcess, 60000
    );

    DWORD exitCode = 1;

    if (waitResult == WAIT_OBJECT_0)
        GetExitCodeProcess(process.hProcess, &exitCode);
    else
        TerminateProcess(process.hProcess, 1);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    if (waitResult != WAIT_OBJECT_0 || exitCode != 0)
        return false;

    WIN32_FILE_ATTRIBUTE_DATA attributes{};

    if (!GetFileAttributesExW(
            outputPath.c_str(),
            GetFileExInfoStandard,
            &attributes)) {
        return false;
    }

    return attributes.nFileSizeHigh != 0 ||
           attributes.nFileSizeLow != 0;
}

std::wstring mci_command(const std::wstring& action) {
    return action + L" " + AUDIO_ALIAS;
}

// All MCI calls for one alias must happen on the same thread, so this worker
// owns the whole lifecycle: open, play, status polling, stop and close.
void playback_worker(
    std::wstring fileToPlay,
    std::wstring tempFileToDelete,
    std::promise<bool> started
) {
    AudioState& state = audio_state();

    auto close_and_cleanup = [&] {
        std::wstring closeCommand = mci_command(L"close");
        mciSendStringW(closeCommand.c_str(), nullptr, 0, nullptr);

        if (!tempFileToDelete.empty())
            DeleteFileW(tempFileToDelete.c_str());
    };

    std::wstring openCommand =
        L"open \"" + fileToPlay +
        L"\" type waveaudio alias " + AUDIO_ALIAS;

    if (mciSendStringW(
            openCommand.c_str(), nullptr, 0, nullptr) != 0) {
        close_and_cleanup();
        state.playing.store(false);
        started.set_value(false);
        return;
    }

    std::wstring playCommand = mci_command(L"play");

    if (mciSendStringW(
            playCommand.c_str(), nullptr, 0, nullptr) != 0) {
        close_and_cleanup();
        state.playing.store(false);
        started.set_value(false);
        return;
    }

    state.playing.store(true);
    started.set_value(true);

    std::wstring statusCommand =
        mci_command(L"status") + L" mode";

    // Give MCI a moment to enter the playing state.
    Sleep(10);

    while (!state.stopRequested.load()) {
        wchar_t mode[64]{};

        MCIERROR error = mciSendStringW(
            statusCommand.c_str(),
            mode,
            64,
            nullptr
        );

        if (error != 0)
            break;

        if (_wcsicmp(mode, L"playing") != 0)
            break;

        Sleep(50);
    }

    if (state.stopRequested.load()) {
        std::wstring stopCommand = mci_command(L"stop");
        mciSendStringW(stopCommand.c_str(), nullptr, 0, nullptr);
    }

    close_and_cleanup();
    state.playing.store(false);
}

} // namespace

bool play_audio(const std::wstring& filePath, bool wait, float volume) {
    std::wstring inputPath = absolute_path(filePath);

    DWORD attributes = GetFileAttributesW(inputPath.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }

    stop_audio();

    std::wstring playbackPath = inputPath;
    std::wstring tempFile;

    // MCI's waveaudio device only handles WAV and cannot reliably change the
    // volume, so transcode anything else (including .ogg) and non-unity gain
    // through ffmpeg into a temporary WAV.
    const bool needsGain = volume < 0.999f || volume > 1.001f;

    if (!has_wav_extension(inputPath) || needsGain) {
        tempFile = make_temp_wav_path();

        if (tempFile.empty())
            return false;

        if (!run_ffmpeg(inputPath, tempFile, volume)) {
            DeleteFileW(tempFile.c_str());
            return false;
        }

        playbackPath = tempFile;
    }

    AudioState& state = audio_state();

    std::lock_guard<std::mutex> lock(state.mutex);

    state.stopRequested.store(false);

    std::promise<bool> started;
    std::future<bool> startResult = started.get_future();

    state.worker = std::thread(
        playback_worker,
        playbackPath,
        tempFile,
        std::move(started)
    );

    if (!startResult.get()) {
        if (state.worker.joinable())
            state.worker.join();

        return false;
    }

    if (wait && state.worker.joinable())
        state.worker.join();

    return true;
}

void stop_audio() {
    AudioState& state = audio_state();

    std::lock_guard<std::mutex> lock(state.mutex);

    state.stopRequested.store(true);

    if (state.worker.joinable())
        state.worker.join();

    state.playing.store(false);
}

bool is_audio_playing() {
    return audio_state().playing.load();
}
