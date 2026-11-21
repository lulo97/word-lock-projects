"""Play pronunciation audio files without blocking the Tk UI."""

import os
import shutil
import subprocess
from pathlib import Path

CREATE_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def _configured_volume():
    """Read SOUND_VOLUME from config.json, falling back to the default gain."""
    try:
        import settings

        return settings.sound_volume()
    except Exception:
        return 2.0


class AudioPlayer:
    """Play OGG files with ffplay, falling back to the OS default player."""

    def __init__(self):
        self._process = None

    def _find_ffplay(self):
        ffplay = shutil.which("ffplay")
        if ffplay:
            return ffplay

        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg:
            name = "ffplay.exe" if os.name == "nt" else "ffplay"
            candidate = Path(ffmpeg).with_name(name)
            if candidate.is_file():
                return str(candidate)
        return None

    def play(self, audio_path, volume=None):
        path = Path(audio_path)
        if not path.is_file():
            return False

        self.stop()
        gain = _configured_volume() if volume is None else volume
        try:
            gain = float(gain)
        except (TypeError, ValueError):
            gain = 2.0
        gain = max(0.0, gain)

        ffplay = self._find_ffplay()
        if ffplay:
            command = [
                ffplay,
                "-nodisp",
                "-autoexit",
                "-loglevel",
                "quiet",
            ]
            # ffplay's -volume caps at 100%, so use the volume filter to
            # amplify beyond the Windows master volume (default 2x).
            if abs(gain - 1.0) > 0.001:
                command += ["-af", f"volume={gain:.2f}"]
            command.append(str(path))
            try:
                self._process = subprocess.Popen(
                    command,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    creationflags=CREATE_NO_WINDOW,
                )
                return True
            except OSError:
                self._process = None

        try:
            if os.name == "nt":
                os.startfile(str(path))
            else:
                subprocess.Popen(
                    ["xdg-open", str(path)],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
            return True
        except OSError:
            return False

    def stop(self):
        if self._process is not None and self._process.poll() is None:
            try:
                self._process.terminate()
            except OSError:
                pass
        self._process = None

    def is_playing(self):
        """True while ffplay is still running."""
        return self._process is not None and self._process.poll() is None

    def shutdown(self):
        self.stop()
