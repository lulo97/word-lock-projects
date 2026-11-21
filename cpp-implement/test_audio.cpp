// test_audio.cpp
//
// Build:
//   build.bat --input test_audio.cpp --output build/test_audio.exe
//
// Plays sekai_wa_hiroi_desu.ogg through utils/play_audio.h.
// Run from the project root (where the .ogg file is located).

#include "ui/ui_framework.h"
#include "utils/play_audio.h"

#include <string>

static const wchar_t* AUDIO_FILE = L"sekai_wa_hiroi_desu.ogg";

static const wchar_t* AUDIO_HTML = LR"HTML(
<div class="bg-gray-100 p-6">
    <p class="text-2xl text-black mb-2">Audio Test</p>

    <p class="text-sm text-gray-600 mb-4">
        File: sekai_wa_hiroi_desu.ogg
    </p>

    <p id="status" class="text-base text-black mb-4">
        Ready
    </p>

    <div class="flex gap-2">
        <button id="btn-play"
                class="bg-blue-600 text-white text-base p-4 rounded-lg">
            Play
        </button>

        <button id="btn-stop"
                class="bg-red-500 text-white text-base p-4 rounded-lg">
            Stop
        </button>
    </div>
</div>
)HTML";

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    UIWindow window(L"Audio Test", 620, 400);

    window.load_html(AUDIO_HTML);

    auto play = [&window] {
        bool ok = play_audio(AUDIO_FILE);

        window.set_text(
            L"status",
            ok
                ? L"Playing sekai_wa_hiroi_desu.ogg ..."
                : L"Failed to play. Check that ffmpeg.exe is in PATH."
        );
    };

    window.bind_click(L"btn-play", play);

    window.bind_click(L"btn-stop", [&window] {
        stop_audio();
        window.set_text(L"status", L"Stopped");
    });

    // Start playing immediately so this also works as a quick test.
    play();

    window.run();

    stop_audio();
    return 0;
}
