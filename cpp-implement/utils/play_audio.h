#pragma once

#include <windows.h>

#include <string>

// Play a .wav file directly, or any format supported by ffmpeg (including
// .ogg) by first converting it to a temporary WAV with ffmpeg.exe from PATH.
//
// By default playback is asynchronous and this returns after playback starts.
// Pass wait = true to block until playback finishes.
//
// Returns false if the file does not exist, ffmpeg conversion fails, or the
// audio device could not be opened.
bool play_audio(const std::wstring& filePath, bool wait = false);

// Stop the current playback and remove any temporary converted file.
void stop_audio();

// True while audio is currently being played.
bool is_audio_playing();
